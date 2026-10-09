#include "radio.h"
#include "bsp.h"
#include "nrf.h"
// Global instance of Radio
Radio* Radio::instance = NULL;

static inline uint32_t enterCritical()
{
   uint32_t primask = __get_PRIMASK();
    __disable_irq();
    __DMB();
    return primask;
}

static inline void exitCritical(uint32_t primask)
{
    __DMB();
    __set_PRIMASK(primask);
}

/*
 * newlib-nano's malloc/free/realloc (used directly by BLEPacket/NanoPbMsg via
 * new/delete, and transitively by the WHAD message queue) are not reentrant:
 * nothing in this project defines __malloc_lock()/__malloc_unlock(), so they
 * default to no-ops. Controller code allocates from directly inside
 * RADIO_IRQHandler (via onReceive()), which can preempt a malloc/free already
 * in progress on the main loop or a timer callback and corrupt the heap.
 * These override newlib's weak hooks to serialize every allocation against
 * that IRQ, the same way enterCritical()/exitCritical() serialize descriptor
 * list access above.
 */
extern "C" {
    struct _reent;
    static uint32_t s_malloc_primask;

    void __malloc_lock(struct _reent *) {
        s_malloc_primask = enterCritical();
    }

    void __malloc_unlock(struct _reent *) {
        exitCritical(s_malloc_primask);
    }
}

/*
 * PPI channels used to route TASKS_RXEN/TASKS_TXEN after a reception,
 * conditionally on whether RADIO->EVENTS_DEVMATCH fired for it, entirely in
 * hardware (see setupAddressMatchPPI()/armAddressMatchTX() below). This is
 * required, not just an alternative to a software decision: EVENTS_DISABLED
 * follows EVENTS_END essentially immediately via the hardware END_DISABLE
 * short, so deciding SHORTS reactively in software (in the END handler) can
 * lose that race. Deciding it as early as DEVMATCH gives comfortable margin,
 * but reacting to DEVMATCH with a CPU interrupt corrupts the in-flight
 * EasyDMA transfer on this chip (confirmed experimentally - see enable()'s
 * comment on why RADIO_INTENSET_DEVMATCH_Msk is deliberately never enabled).
 * PPI is the way to react that early with zero CPU/interrupt involvement.
 *
 * nRF52840 PPI channels 20-31 are pre-programmed/fixed-function (channels
 * 24-25 are already used by enableEncryption() above, wired to
 * RADIO->EVENTS_READY -> CCM->TASKS_KSGEN and RADIO->EVENTS_ADDRESS ->
 * CCM->TASKS_CRYPT); only channels 0-19 are freely programmable, and FORK is
 * only available on that range, so RADIO_PPI_CH_DEVMATCH (which needs it)
 * must come from there. Nothing else in this codebase currently uses PPI
 * channels 0-19 or groups 0-1.
 */
#define RADIO_PPI_CH_DEVMATCH 0
#define RADIO_PPI_CH_RXEN     1
#define RADIO_PPI_CH_TXEN     2

Radio::Radio() {
	this->setProtocol(GENERIC_PROTOCOL);
	this->ready = false;
	this->state = NONE;
	this->txPower = POS0_DBM;
	this->rssi = false;
    this->last_rssi = 0;
	this->autoTXafterRXenabled = false;
	this->controller = NULL;
	this->interFrameSpacing = 0;
	this->filterEnabled = false;
    this->filterMode = FilterMode::Disabled;
    this->reportPdu = true;
    this->switchToTx = false;
	this->matchingEnable = false;
	this->matchingSize = 0;
	this->jammingPatternsEnabled = false;
	this->jammingPatternsCounter = 0;
	this->jammingInterval = 0;
	this->encryption = false;
    this->fastRampUpTime = false;
    this->addrMatch = false;

    /* Initialize jamming patterns queue. */
	this->initJammingPatternsQueue();

    /* Update instance reference (pseudo-singleton). */
	instance = this;

    /* Initialize our RX queue. */
    this->rxHead = this->rxTail = 0;
    this->rxDesc = NULL;

    /* No pending TX. */
    this->pendingTx = false;

    /* One-time PPI wiring for address-match-conditional auto-TX-after-RX.
     * Only the static EEP/TEP/FORK/CHG-membership shape is set up here;
     * which channels are actually enabled is managed dynamically by
     * armAddressMatchTX(), called at every safe (no DMA in flight) point
     * where RX gets (re-)armed. */
    this->setupAddressMatchPPI();
}

void Radio::setupAddressMatchPPI() {
    /* CH_RXEN/CH_TXEN: after any reception or transmission completes
     * (EVENTS_DISABLED), go back to RX or switch to TX. Which of the two is
     * actually wired to fire is entirely determined by CHG[1]/CHG[0]'s
     * enable state (see below), never by directly touching these channels'
     * own enable bit outside of that. */
    NRF_PPI->CH[RADIO_PPI_CH_RXEN].EEP = (uint32_t)&NRF_RADIO->EVENTS_DISABLED;
    NRF_PPI->CH[RADIO_PPI_CH_RXEN].TEP = (uint32_t)&NRF_RADIO->TASKS_RXEN;

    NRF_PPI->CH[RADIO_PPI_CH_TXEN].EEP = (uint32_t)&NRF_RADIO->EVENTS_DISABLED;
    NRF_PPI->CH[RADIO_PPI_CH_TXEN].TEP = (uint32_t)&NRF_RADIO->TASKS_TXEN;

    /* CH_DEVMATCH: the instant a reception's device address matches (well
     * before that reception's END/DISABLED), atomically flip which of the
     * two channels above is enabled - main task enables the TX path's
     * group, fork task disables the RX path's group. This is pure PPI
     * routing: it never writes to a RADIO register, so unlike a direct
     * SHORTS/PACKETPTR write from software, it's safe to fire mid-packet -
     * and unlike the CPU taking an interrupt for the same event, it never
     * touches the CPU/NVIC at all, so it can't disturb EasyDMA either. */
    NRF_PPI->CH[RADIO_PPI_CH_DEVMATCH].EEP = (uint32_t)&NRF_RADIO->EVENTS_DEVMATCH;
    NRF_PPI->CH[RADIO_PPI_CH_DEVMATCH].TEP = (uint32_t)&NRF_PPI->TASKS_CHG[0].EN;
    NRF_PPI->FORK[RADIO_PPI_CH_DEVMATCH].TEP = (uint32_t)&NRF_PPI->TASKS_CHG[1].DIS;

    NRF_PPI->CHG[0] = (1 << RADIO_PPI_CH_TXEN);   // "go TX" path
    NRF_PPI->CHG[1] = (1 << RADIO_PPI_CH_RXEN);   // "go RX" path (default)

    /* Establish the safe default (go RX, no pending match redirect) and
     * leave the whole scheme disabled until armAddressMatchTX() actually
     * wants it - see there for why. */
    NRF_PPI->TASKS_CHG[1].EN = 1;
    NRF_PPI->TASKS_CHG[0].DIS = 1;
    NRF_PPI->CHENCLR = (1 << RADIO_PPI_CH_RXEN) | (1 << RADIO_PPI_CH_TXEN) | (1 << RADIO_PPI_CH_DEVMATCH);
}

void Radio::armAddressMatchTX() {
    if (this->isFilterEnabled()) {
        /* Filtering is active: RXEN/TXEN after this reception are routed
         * entirely through PPI, not SHORTS - enable()/fastFrequencyChange()
         * intentionally omit DISABLED_RXEN/DISABLED_TXEN from SHORTS
         * whenever filtering is on, so there's no redundant/conflicting
         * auto-chain fighting this one.
         *
         * Both writes below only ever run here, at a point with no DMA
         * transfer in flight (mode entry, channel change, or after the
         * previous reception/transmission has fully completed) - never
         * reactively while a packet is being received. */
        NRF_PPI->TASKS_CHG[1].EN = 1;
        NRF_PPI->TASKS_CHG[0].DIS = 1;

        /* Only let a match actually redirect us to TX if we truly have
         * something queued to send; otherwise a match would route to
         * TASKS_TXEN with PACKETPTR still pointing at the RX descriptor,
         * transmitting garbage. */
        if (this->isAutoTXafterRXenabled() && this->pendingTx) {
            NRF_PPI->CHENSET = (1 << RADIO_PPI_CH_DEVMATCH);
        } else {
            NRF_PPI->CHENCLR = (1 << RADIO_PPI_CH_DEVMATCH);
        }
    } else {
        /* No filter: every other protocol/mode relies on SHORTS's own
         * DISABLED_RXEN/DISABLED_TXEN bits directly, as before. Keep this
         * scheme fully disabled so it can never fire alongside them. */
        NRF_PPI->CHENCLR = (1 << RADIO_PPI_CH_RXEN) | (1 << RADIO_PPI_CH_TXEN) | (1 << RADIO_PPI_CH_DEVMATCH);
    }
}

bool Radio::enableEncryption(uint32_t encryptionData) {

	//Configure shorts between  CCM->ENDKSGEN and  CCM->CRYPT
	NRF_CCM->SHORTS |= CCM_SHORTS_ENDKSGEN_CRYPT_Msk;
	// Provision encryption data
	NRF_CCM->CNFPTR = encryptionData;
	// Provision scratch zone
	NRF_CCM->SCRATCHPTR = (uint32_t)(this->encryptionScratchpad);

	/*
	Configure PPI shorts between RADIO->EVENTS_READY and CCM->TASKS_KSGEN
	and between RADIO->EVENTS_ADDRESS and CCM->TASKS_CRYPT
	*/
	NRF_PPI->CHEN = (1 << 24) | (1 << 25);

	this->encryption = true;
	return true;
}

bool Radio::disableEncryption() {
	return true;
}

void Radio::enableMatch(int matchingSize) {
	this->matchingEnable = true;
	this->matchingSize = matchingSize;
}

void Radio::disableMatch() {
	this->matchingEnable = false;
}

bool Radio::isMatchingEnabled() {
	return this->matchingEnable;
}

uint32_t Radio::getJammingInterval() {
	return this->jammingInterval;
}
void Radio::setJammingInterval(uint32_t jammingInterval) {
	this->jammingInterval = jammingInterval;
}

void Radio::initJammingPatternsQueue() {
	this->jammingPatternsQueue = (JammingPatternsQueue*) malloc(sizeof(JammingPatternsQueue));
	this->jammingPatternsQueue->size = 0;
	this->jammingPatternsQueue->first = NULL;
}


bool Radio::enableJammingPatterns() {
	this->jammingPatternsEnabled = true;
	return true;
}

bool  Radio::disableJammingPatterns() {
	this->jammingPatternsEnabled = false;
	return true;
}

bool  Radio::setJammingPatternsCounter(uint8_t counter) {
	this->jammingPatternsCounter = counter;
	return true;
}

uint8_t  Radio::getJammingPatternsCounter() {
	return this->jammingPatternsCounter;
}

void Radio::addJammingPattern(uint8_t* pattern, uint8_t* mask, size_t size, uint8_t position) {
	JammingPattern* current = (JammingPattern*)malloc(sizeof(JammingPattern));
	current->pattern = (uint8_t*)malloc(sizeof(uint8_t)*size);
	for (size_t i=0;i<size;i++) current->pattern[i] = pattern[i];
	current->mask = (uint8_t*)malloc(sizeof(uint8_t)*size);
	for (size_t i=0;i<size;i++) current->mask[i] = mask[i];
	current->size = size;
	current->position = position;
	current->next = this->jammingPatternsQueue->first;
	this->jammingPatternsQueue->size++;
	this->jammingPatternsQueue->first = current;
}

bool Radio::resetJammingPatternsQueue() {
	JammingPattern* remove = this->jammingPatternsQueue->first;
	JammingPattern* current = remove;
	while (remove != NULL) {
		current = remove->next;
		free(remove->pattern);
		free(remove->mask);
		free(remove);
		remove = current;
	}
	this->jammingPatternsQueue->first = NULL;
	this->jammingPatternsQueue->size = 0;
	return true;
}

bool Radio::removeJammingPattern(uint8_t* pattern, uint8_t* mask, size_t size, uint8_t position) {
	JammingPattern* current = this->jammingPatternsQueue->first;
	JammingPattern* remove;
	if (current == NULL) {
		return false;
	}
	else {
		if (size == current->size && position == current->position && compareBuffers(pattern,current->pattern,size) && compareBuffers(mask,current->mask,size)) {
			this->jammingPatternsQueue->first = current->next;
			this->jammingPatternsQueue->size--;
			free(current->pattern);
			free(current->mask);
			free(current);
			return true;
		}
		else {
			while (current->next != NULL) {
				if (size == current->next->size && position == current->next->position && compareBuffers(pattern,current->next->pattern,size) && compareBuffers(mask,current->next->mask,size)) {
					remove = current->next;
					current->next = current->next->next;
					this->jammingPatternsQueue->size--;
					free(remove->pattern);
					free(remove->mask);
					free(remove);
					return true;
				}
				current = current->next;
			}
			return false;
		}
	}
	return false;
}

bool Radio::checkJammingPattern(JammingPattern* pattern, uint8_t *buffer, size_t size) {
	if (pattern->position == 0xFF) {
		bool match = true;
		for (size_t pos=0;pos<size-pattern->size;pos++) {
			match = true;
			for (size_t i=0;i<pattern->size;i++) {
				match = match && ((buffer[i+pos] & pattern->mask[i]) == pattern->pattern[i]);
			}
			if (match) return true;
		}
		return false;
	}
	else {
		bool match = true;
		for (size_t i=0;i<pattern->size;i++) {
			match = match && ((buffer[i+pattern->position] & pattern->mask[i]) == pattern->pattern[i]);
		}
		return match;
	}
	return false;
}

bool Radio::checkJammingPatterns(uint8_t *buffer, size_t size) {
	bool found = false;
	JammingPattern* pattern = this->jammingPatternsQueue->first;
	while (pattern != NULL) {
		found = checkJammingPattern(pattern,buffer,size);
		if (!found) pattern = pattern->next;
		else break;
	}
	return found;
}

Controller* Radio::getController(){
	return this->controller;
}

bool Radio::setController(Controller *controller){
	this->controller = controller;
	return true;
}


RadioState Radio::getState() {
	return this->state;
}
bool Radio::setState(RadioState state) {
	this->state = state;
	return true;
}

Protocol Radio::getProtocol() {
	return this->protocol;
}
bool Radio::setProtocol(Protocol protocol) {
	this->protocol = protocol;
	return true;
}

Endianness Radio::getEndianness() {
	return this->endianness;
}

bool Radio::getFastRampUpTime() {
	return this->fastRampUpTime;
}
bool Radio::setFastRampUpTime(bool fastRampUpTime) {
	this->fastRampUpTime = fastRampUpTime;
	return true;
}

bool Radio::setEndianness(Endianness endianness) {
	this->endianness = endianness;
	return true;
}

Preamble Radio::getPreamble() {
	return this->preamble;
}

bool Radio::setPreamble(uint8_t* pattern, uint8_t size) {
	for (int i=0;i<5;i++) (this->preamble).pattern[i] = 0x00;
	for (int i=0;i<size;i++) {
		(this->preamble).pattern[i] = pattern[i];
	}
	(this->preamble).size = size;
	return true;
}

bool Radio::setPreamble(Preamble preamble) {
	this->preamble = preamble;
	return true;
}

TxPower Radio::getTxPower() {
	return this->txPower;
}

bool Radio::setTxPower(TxPower txPower) {

	this->txPower = txPower;
	return true;
}

bool Radio::setTxPower(int txPower) {
	bool success = false;
	switch (txPower) {
		case -40:
			success = this->setTxPower(NEG40_DBM);
			break;
		case -30:
			success = this->setTxPower(NEG30_DBM);
			break;
		case -20:
			success = this->setTxPower(NEG20_DBM);
			break;
		case -16:
			success = this->setTxPower(NEG16_DBM);
			break;
		case -12:
			success = this->setTxPower(NEG12_DBM);
			break;
		case -8:
			success = this->setTxPower(NEG8_DBM);
			break;
		case -4:
			success = this->setTxPower(NEG4_DBM);
			break;
		case 0:
			success = this->setTxPower(POS0_DBM);
			break;
		case 4:
			success = this->setTxPower(POS4_DBM);
			break;
		case 8:
			success = this->setTxPower(POS8_DBM);
			break;
		default:
			success = false;
	}
	return success;
}

bool Radio::isRssiEnabled() {
	return this->rssi;
}

bool Radio::enableRssi() {
	this->rssi = true;
	return true;
}
bool Radio::disableRssi() {
	this->rssi = false;
	return true;
}

/* TODO: No more user, to remove */
void Radio::setLastRssi(uint8_t rssi) {
    this->last_rssi = rssi;
}

/* TODO: No more user, to remove */
uint8_t Radio::getLastRssi(void) {
    return this->last_rssi;
}

Phy Radio::getPhy() {
	return this->phy;
}

bool Radio::setPhy(Phy phy) {
    this->phy = phy;
    this->generateModeRegister();
    return true;
}

Whitening Radio::getWhitening() {
	return this->whitening;
}

bool Radio::setWhitening(Whitening whitening) {
	this->whitening = whitening;
	return true;
}

uint8_t Radio::getWhiteningDataIv() {
	return this->whiteningDataIv;
}
bool Radio::setWhiteningDataIv(uint8_t iv) {
	this->whiteningDataIv = iv;
	return true;
}

Crc Radio::getCrc() {
	return this->crc;
}
bool Radio::setCrc(Crc crc) {
	this->crc = crc;
	return true;
}

uint8_t Radio::getCrcSize() {
	return this->crcSize;
}

bool Radio::setCrcSize(uint8_t crcSize) {
	this->crcSize = crcSize;
	return true;
}

uint32_t Radio::getCrcInit() {
	return this->crcInit;
}

bool Radio::setCrcInit(uint32_t init) {
	this->crcInit = init;
	return true;
}

uint32_t Radio::getCrcPoly() {
	return this->crcPoly;
}

bool Radio::setCrcPoly(uint32_t poly) {
	this->crcPoly = poly;
	return true;
}

bool Radio::getCrcSkipAddress() {
	return crcSkipAddress;
}
bool Radio::setCrcSkipAddress(bool skip) {
	this->crcSkipAddress = skip;
	return true;
}


bool Radio::isAutoTXafterRXenabled() {
	return this->autoTXafterRXenabled;
}
bool Radio::enableAutoTXafterRX() {
	this->autoTXafterRXenabled = true;
	return true;
}

bool Radio::disableAutoTXafterRX() {
	this->autoTXafterRXenabled = false;
	return true;
}
int Radio::getInterFrameSpacing() {
	return this->interFrameSpacing;
}
bool Radio::setInterFrameSpacing(int ifs) {
	this->interFrameSpacing = ifs;
	return true;
}

Header Radio::getHeader() {
	return this->header;
}
bool Radio::setHeader(uint8_t s0, uint8_t length, uint8_t s1) {
	(this->header).s0 = s0;
	(this->header).length = length;
	(this->header).s1 = s1;
	return true;
}

bool Radio::setHeader(Header header) {
	this->header = header;
	return true;
}

uint8_t Radio::getPayloadLength() {
	return this->payloadLength;
}

bool Radio::setPayloadLength(uint8_t payloadLength) {
	this->payloadLength = payloadLength;
	return true;
}

uint8_t Radio::getExpandPayloadLength() {
	return this->expandPayloadLength;
}

bool Radio::setExpandPayloadLength(uint8_t expandPayloadLength) {
	this->expandPayloadLength = expandPayloadLength;
	return true;
}

bool Radio::setFrequency(int frequency) {
	this->frequency = frequency;
	return true;
}

int Radio::getFrequency() {
	return this->frequency;
}

RadioMode Radio::getMode() {
	return this->mode;
}

bool Radio::setMode(RadioMode mode) {
	this->mode = mode;
	return true;
}

bool Radio::enableFilter(BLEAddress address, FilterMode mode) {
	for (int i=0;i<6;i++) this->filter.bytes[i] = address.bytes[i];
	this->filterEnabled = true;
    this->filterMode = mode;
	return true;
}

bool Radio::isFilterEnabled() {
	return this->filterEnabled;
}

bool Radio::disableFilter() {
	for (int i=0;i<6;i++) this->filter.bytes[i] = 0;
	this->filterEnabled = false;
    this->filterMode = FilterMode::Disabled;
	return true;
}


bool Radio::setPrefixes() {
	this->prefixes.number = 0;
	this->prefixes.prefixes[0] = 0x00;
	this->prefixes.prefixes[1] = 0x00;
	this->prefixes.prefixes[2] = 0x00;
	this->prefixes.prefixes[3] = 0x00;
	this->prefixes.prefixes[4] = 0x00;
	this->prefixes.prefixes[5] = 0x00;
	this->prefixes.prefixes[6] = 0x00;
	return true;
}
bool Radio::setPrefixes(uint8_t a) {
	this->prefixes.number = 1;
	this->prefixes.prefixes[0] = a;
	this->prefixes.prefixes[1] = 0x00;
	this->prefixes.prefixes[2] = 0x00;
	this->prefixes.prefixes[3] = 0x00;
	this->prefixes.prefixes[4] = 0x00;
	this->prefixes.prefixes[5] = 0x00;
	this->prefixes.prefixes[6] = 0x00;
	return true;
}
bool Radio::setPrefixes(uint8_t a,uint8_t b) {
	this->prefixes.number = 2;
	this->prefixes.prefixes[0] = a;
	this->prefixes.prefixes[1] = b;
	this->prefixes.prefixes[2] = 0x00;
	this->prefixes.prefixes[3] = 0x00;
	this->prefixes.prefixes[4] = 0x00;
	this->prefixes.prefixes[5] = 0x00;
	this->prefixes.prefixes[6] = 0x00;
	return true;
}
bool Radio::setPrefixes(uint8_t a,uint8_t b,uint8_t c) {
	this->prefixes.number = 3;
	this->prefixes.prefixes[0] = a;
	this->prefixes.prefixes[1] = b;
	this->prefixes.prefixes[2] = c;
	this->prefixes.prefixes[3] = 0x00;
	this->prefixes.prefixes[4] = 0x00;
	this->prefixes.prefixes[5] = 0x00;
	this->prefixes.prefixes[6] = 0x00;
	return true;
}

bool Radio::setPrefixes(uint8_t a,uint8_t b,uint8_t c, uint8_t d) {
	this->prefixes.number = 4;
	this->prefixes.prefixes[0] = a;
	this->prefixes.prefixes[1] = b;
	this->prefixes.prefixes[2] = c;
	this->prefixes.prefixes[3] = d;
	this->prefixes.prefixes[4] = 0x00;
	this->prefixes.prefixes[5] = 0x00;
	this->prefixes.prefixes[6] = 0x00;
	return true;
}
bool Radio::setPrefixes(uint8_t a,uint8_t b,uint8_t c, uint8_t d, uint8_t e) {
	this->prefixes.number = 5;
	this->prefixes.prefixes[0] = a;
	this->prefixes.prefixes[1] = b;
	this->prefixes.prefixes[2] = c;
	this->prefixes.prefixes[3] = d;
	this->prefixes.prefixes[4] = e;
	this->prefixes.prefixes[5] = 0x00;
	this->prefixes.prefixes[6] = 0x00;
	return true;
}
bool Radio::setPrefixes(uint8_t a,uint8_t b,uint8_t c, uint8_t d, uint8_t e, uint8_t f) {
	this->prefixes.number = 6;
	this->prefixes.prefixes[0] = a;
	this->prefixes.prefixes[1] = b;
	this->prefixes.prefixes[2] = c;
	this->prefixes.prefixes[3] = d;
	this->prefixes.prefixes[4] = e;
	this->prefixes.prefixes[5] = f;
	this->prefixes.prefixes[6] = 0x00;
	return true;
}
bool Radio::setPrefixes(uint8_t a,uint8_t b,uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint8_t g) {
	this->prefixes.number = 7;
	this->prefixes.prefixes[0] = a;
	this->prefixes.prefixes[1] = b;
	this->prefixes.prefixes[2] = c;
	this->prefixes.prefixes[3] = d;
	this->prefixes.prefixes[4] = e;
	this->prefixes.prefixes[5] = f;
	this->prefixes.prefixes[6] = g;
	return true;
}

bool Radio::disable() {
	bool success = false;
	if (NRF_RADIO->STATE > 0)
	{
		NVIC_ClearPendingIRQ(RADIO_IRQn);
		NVIC_DisableIRQ(RADIO_IRQn);

        /* Disable the address-match PPI routing before triggering DISABLE
         * below: CH_RXEN/CH_TXEN both fire on EVENTS_DISABLED, so if left
         * enabled they would immediately re-arm the radio (RXEN or TXEN)
         * right after this explicit disable, defeating it. Must happen
         * before TASKS_DISABLE, not after - the DISABLED event this
         * triggers is exactly what those channels react to. */
        NRF_PPI->CHENCLR = (1 << RADIO_PPI_CH_RXEN) | (1 << RADIO_PPI_CH_TXEN) | (1 << RADIO_PPI_CH_DEVMATCH);

		NRF_RADIO->EVENTS_DISABLED = 0;
		NRF_RADIO->TASKS_EDSTOP = 1;
		NRF_RADIO->TASKS_DISABLE = 1;
		while (NRF_RADIO->EVENTS_DISABLED == 0) {}
		success = true;

        /* Flush RX list. */
        uint32_t primask = enterCritical();
        this->rxHead = this->rxTail = 0;
        exitCritical(primask);
	}
    

	return success;
}
bool Radio::generateTxPowerRegister() {

	switch (this->txPower) {
		case NEG40_DBM:
			NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_Neg40dBm << RADIO_TXPOWER_TXPOWER_Pos);
			return true;
		case NEG30_DBM:
			NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_Neg30dBm << RADIO_TXPOWER_TXPOWER_Pos);
			return true;
		case NEG20_DBM:
			NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_Neg20dBm << RADIO_TXPOWER_TXPOWER_Pos);
			return true;
		case NEG16_DBM:
			NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_Neg16dBm << RADIO_TXPOWER_TXPOWER_Pos);
			return true;
		case NEG12_DBM:
			NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_Neg12dBm << RADIO_TXPOWER_TXPOWER_Pos);
			return true;
		case NEG8_DBM:
			NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_Neg8dBm << RADIO_TXPOWER_TXPOWER_Pos);
			return true;
		case NEG4_DBM:
			NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_Neg4dBm << RADIO_TXPOWER_TXPOWER_Pos);
			return true;
		case POS0_DBM:
			NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_0dBm << RADIO_TXPOWER_TXPOWER_Pos);
			return true;
		case POS4_DBM:
			NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_Pos4dBm << RADIO_TXPOWER_TXPOWER_Pos);
			return true;
		case POS8_DBM:
			NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_Pos8dBm << RADIO_TXPOWER_TXPOWER_Pos);
			return true;
	}
	return false;
}

bool Radio::generateModeCnf0Register() {
	if (this->fastRampUpTime) {
		NRF_RADIO->MODECNF0 |= 1;//| (1 << 8) /* glitch :) ?*/;
	}
	else {
		NRF_RADIO->MODECNF0 &= 0;
	}
	return true;
}

bool Radio::generateFrequencyRegister() {
	bool success = false;
	if (this->frequency >= 0 && this->frequency <= 100) {
		NRF_RADIO->FREQUENCY = this->frequency;
		success = true;
	}
	return success;
}
bool Radio::generateModeRegister() {
	bool success = true;
	switch (this->phy) {
		case ESB_1MBITS:
			NRF_RADIO->MODE = (RADIO_MODE_MODE_Nrf_1Mbit << RADIO_MODE_MODE_Pos);
			break;
		case ESB_2MBITS:
			NRF_RADIO->MODE = (RADIO_MODE_MODE_Nrf_2Mbit << RADIO_MODE_MODE_Pos);
			break;
		case BLE_1MBITS:
			NRF_RADIO->MODE = (RADIO_MODE_MODE_Ble_1Mbit << RADIO_MODE_MODE_Pos);
			break;
		case DOT15D4_WAZABEE:
		case BLE_2MBITS:
			NRF_RADIO->MODE = (RADIO_MODE_MODE_Ble_2Mbit << RADIO_MODE_MODE_Pos);
			break;
		case DOT15D4_NATIVE:
			NRF_RADIO->MODE = (RADIO_MODE_MODE_Ieee802154_250Kbit << RADIO_MODE_MODE_Pos);
			break;
		default:
			success = false;
			break;
	}
	return success;
}

bool Radio::generateBaseAndPrefixRegisters() {
	bool success = false;

	NRF_RADIO->TXADDRESS = 0;
	NRF_RADIO->RXADDRESSES = 1;
	if (this->phy == DOT15D4_NATIVE) {
		NRF_RADIO->SFD = 0xA7;
		NRF_RADIO->MHRMATCHCONF = 0;
		NRF_RADIO->MHRMATCHMAS = 0;
		return true;
	}
	if (this->endianness == BIG) {
		if ((this->preamble).size == 5) {
			NRF_RADIO->BASE0 = bytewise_bit_swap(
				((uint32_t)(this->preamble).pattern[3])<<24 |
				((uint32_t)(this->preamble).pattern[0]) |
				((uint32_t)(this->preamble).pattern[2])<<16  |
				((uint32_t)(this->preamble).pattern[1])<<8
			);
			NRF_RADIO->PREFIX0 = bytewise_bit_swap((this->preamble).pattern[4]) & RADIO_PREFIX0_AP0_Msk;

			if (this->prefixes.number > 0) {
				NRF_RADIO->BASE1 = bytewise_bit_swap(
					((uint32_t)(this->preamble).pattern[3])<<24 |
					((uint32_t)(this->preamble).pattern[0]) |
					((uint32_t)(this->preamble).pattern[2])<<16  |
					((uint32_t)(this->preamble).pattern[1])<<8
				);
				for (size_t i=0;i<this->prefixes.number;i++) {
					if (this->prefixes.number < 3) {
						NRF_RADIO->PREFIX0 |= ((bytewise_bit_swap((this->prefixes).prefixes[i]) & 0xFF) << (8*(i+1)));
					}
					else {
						NRF_RADIO->PREFIX1 |= ((bytewise_bit_swap((this->prefixes).prefixes[i - 3]) & 0xFF) << (8*(i-3)));
					}
					NRF_RADIO->RXADDRESSES |= 1 << (i+1);
				}

			}
			success = true;
		}
		else if ((this->preamble).size == 4) {
			NRF_RADIO->BASE0 = bytewise_bit_swap(
				((uint32_t)(this->preamble).pattern[2])<<24 |
				((uint32_t)0x00) |
				((uint32_t)(this->preamble).pattern[1])<<16  |
				((uint32_t)(this->preamble).pattern[0])<<8
			);


			NRF_RADIO->PREFIX0 = bytewise_bit_swap((this->preamble).pattern[3]) & RADIO_PREFIX0_AP0_Msk;

			if (this->prefixes.number > 0) {
				NRF_RADIO->BASE1 = bytewise_bit_swap(
					((uint32_t)(this->preamble).pattern[2])<<24 |
					((uint32_t)0x00) |
					((uint32_t)(this->preamble).pattern[1])<<16  |
					((uint32_t)(this->preamble).pattern[0])<<8
				);
				for (size_t i=0;i<this->prefixes.number;i++) {
					if (this->prefixes.number < 3) {
						NRF_RADIO->PREFIX0 |= ((bytewise_bit_swap((this->prefixes).prefixes[i]) & 0xFF) << (8*(i+1)));
					}
					else {
						NRF_RADIO->PREFIX1 |= ((bytewise_bit_swap((this->prefixes).prefixes[i - 3]) & 0xFF) << (8*(i-3)));
					}
					NRF_RADIO->RXADDRESSES |= 1 << (i+1);
				}

			}
			success = true;
		}
		else if ((this->preamble).size == 3) {
			NRF_RADIO->BASE0 = bytewise_bit_swap(
				((uint32_t)(this->preamble).pattern[1])<<24 |
				((uint32_t)0x00) |
				((uint32_t)(this->preamble).pattern[0])<<16  |
				((uint32_t)0x00)<<8
			);


			NRF_RADIO->PREFIX0 = bytewise_bit_swap((this->preamble).pattern[2]) & RADIO_PREFIX0_AP0_Msk;
			if (this->prefixes.number > 0) {
				NRF_RADIO->BASE1 = bytewise_bit_swap(
					((uint32_t)(this->preamble).pattern[1])<<24 |
					((uint32_t)0x00) |
					((uint32_t)(this->preamble).pattern[0])<<16  |
					((uint32_t)0x00)<<8
				);
				for (size_t i=0;i<this->prefixes.number;i++) {
					if (this->prefixes.number < 3) {
						NRF_RADIO->PREFIX0 |= ((bytewise_bit_swap((this->prefixes).prefixes[i]) & 0xFF) << (8*(i+1)));
					}
					else {
						NRF_RADIO->PREFIX1 |= ((bytewise_bit_swap((this->prefixes).prefixes[i - 3]) & 0xFF) << (8*(i-3)));
					}
					NRF_RADIO->RXADDRESSES |= 1 << (i+1);
				}

			}
			success = true;
		}
		else if ((this->preamble).size == 2) {
			NRF_RADIO->BASE0 = bytewise_bit_swap(
				((uint32_t)(this->preamble).pattern[0])<<24 |
				((uint32_t)0x00) |
				((uint32_t)0x00)<<16  |
				((uint32_t)0x00)<<8
			);


			NRF_RADIO->PREFIX0 = bytewise_bit_swap((this->preamble).pattern[1]) & RADIO_PREFIX0_AP0_Msk;
			if (this->prefixes.number > 0) {
				NRF_RADIO->BASE1 = bytewise_bit_swap(
					((uint32_t)(this->preamble).pattern[0])<<24 |
					((uint32_t)0x00) |
					((uint32_t)0x00)<<16  |
					((uint32_t)0x00)<<8
				);
				for (size_t i=0;i<this->prefixes.number;i++) {
					if (this->prefixes.number < 3) {
						NRF_RADIO->PREFIX0 |= ((bytewise_bit_swap((this->prefixes).prefixes[i]) & 0xFF) << (8*(i+1)));
					}
					else {
						NRF_RADIO->PREFIX1 |= ((bytewise_bit_swap((this->prefixes).prefixes[i - 3]) & 0xFF) << (8*(i-3)));
					}
					NRF_RADIO->RXADDRESSES |= 1 << (i+1);
				}
			}
			success = true;
		}
		else if ((this->preamble).size == 1) {
			NRF_RADIO->BASE0 = bytewise_bit_swap(
				((uint32_t)0x00)<<24 |
				((uint32_t)0x00) |
				((uint32_t)0x00)<<16  |
				((uint32_t)0x00)<<8
			);


			NRF_RADIO->PREFIX0 = bytewise_bit_swap((this->preamble).pattern[0]) & RADIO_PREFIX0_AP0_Msk;
			if (this->prefixes.number > 0) {
				NRF_RADIO->BASE1 = bytewise_bit_swap(
					((uint32_t)0x00)<<24 |
					((uint32_t)0x00) |
					((uint32_t)0x00)<<16  |
					((uint32_t)0x00)<<8
				);
				for (size_t i=0;i<this->prefixes.number;i++) {
					if (this->prefixes.number < 3) {
						NRF_RADIO->PREFIX0 |= ((bytewise_bit_swap((this->prefixes).prefixes[i]) & 0xFF) << (8*(i+1)));
					}
					else {
						NRF_RADIO->PREFIX1 |= ((bytewise_bit_swap((this->prefixes).prefixes[i - 3]) & 0xFF) << (8*(i-3)));
					}
					NRF_RADIO->RXADDRESSES |= 1 << (i+1);
				}

			}
			success = true;
		}
	}
	else {
		if ((this->preamble).size == 1) {
			NRF_RADIO->BASE0 = 0x00000000;
			NRF_RADIO->PREFIX0 = (this->preamble).pattern[0];
			if (this->prefixes.number > 0) {
				NRF_RADIO->BASE1 = (this->preamble).pattern[0];
				for (size_t i=0;i<this->prefixes.number;i++) {
					if (this->prefixes.number < 3) {
						NRF_RADIO->PREFIX0 |= ((((this->prefixes).prefixes[i]) & 0xFF) << (8*(i+1)));
					}
					else {
						NRF_RADIO->PREFIX1 |= ((((this->prefixes).prefixes[i - 3]) & 0xFF) << (8*(i-3)));
					}
					NRF_RADIO->RXADDRESSES |= 1 << (i+1);
				}

			}
			success = true;
		}
		else if ((this->preamble).size == 2) {
			NRF_RADIO->BASE0 = (
				(((uint32_t)(this->preamble).pattern[1]) << 24) |
				(((uint32_t)0x00) << 16) |
				(((uint32_t)0x00) << 8) |
				(((uint32_t)0x00))
			);
			NRF_RADIO->PREFIX0 = (this->preamble).pattern[0] & RADIO_PREFIX0_AP0_Msk;
			if (this->prefixes.number > 0) {
				NRF_RADIO->BASE1 = (
					(((uint32_t)(this->preamble).pattern[1]) << 24) |
					(((uint32_t)0x00) << 16) |
					(((uint32_t)0x00) << 8) |
					(((uint32_t)0x00))
				);
				for (size_t i=0;i<this->prefixes.number;i++) {
					if (this->prefixes.number < 3) {
						NRF_RADIO->PREFIX0 |= ((((this->prefixes).prefixes[i]) & 0xFF) << (8*(i+1)));
					}
					else {
						NRF_RADIO->PREFIX1 |= ((((this->prefixes).prefixes[i - 3]) & 0xFF) << (8*(i-3)));
					}
					NRF_RADIO->RXADDRESSES |= 1 << (i+1);
				}

			}
			success = true;
		}
		else if ((this->preamble).size == 3) {
			NRF_RADIO->BASE0 = (
				(((uint32_t)(this->preamble).pattern[1]) << 24) |
				(((uint32_t)(this->preamble).pattern[2]) << 16) |
				(((uint32_t)0x00) << 8) |
				(((uint32_t)0x00))
			);
			NRF_RADIO->PREFIX0 = (this->preamble).pattern[0] & RADIO_PREFIX0_AP0_Msk;
			if (this->prefixes.number > 0) {
				NRF_RADIO->BASE1 = (
					(((uint32_t)(this->preamble).pattern[1]) << 24) |
					(((uint32_t)(this->preamble).pattern[2]) << 16) |
					(((uint32_t)0x00) << 8) |
					(((uint32_t)0x00))
				);
				for (size_t i=0;i<this->prefixes.number;i++) {
					if (this->prefixes.number < 3) {
						NRF_RADIO->PREFIX0 |= ((((this->prefixes).prefixes[i]) & 0xFF) << (8*(i+1)));
					}
					else {
						NRF_RADIO->PREFIX1 |= ((((this->prefixes).prefixes[i - 3]) & 0xFF) << (8*(i-3)));
					}
					NRF_RADIO->RXADDRESSES |= 1 << (i+1);
				}
			}
			success = true;
		}
		else if ((this->preamble).size == 4) {
			NRF_RADIO->BASE0 = (
				(((uint32_t)(this->preamble).pattern[1]) << 24) |
				(((uint32_t)(this->preamble).pattern[2]) << 16) |
				(((uint32_t)(this->preamble).pattern[3]) << 8) |
				(((uint32_t)0x00))
			);
			NRF_RADIO->PREFIX0 = (this->preamble).pattern[0] & RADIO_PREFIX0_AP0_Msk;
			if (this->prefixes.number > 0) {
				NRF_RADIO->BASE1 = (
					(((uint32_t)(this->preamble).pattern[1]) << 24) |
					(((uint32_t)(this->preamble).pattern[2]) << 16) |
					(((uint32_t)(this->preamble).pattern[3]) << 8) |
					(((uint32_t)0x00))
				);
				for (size_t i=0;i<this->prefixes.number;i++) {
					if (this->prefixes.number < 3) {
						NRF_RADIO->PREFIX0 |= ((((this->prefixes).prefixes[i]) & 0xFF) << (8*(i+1)));
					}
					else {
						NRF_RADIO->PREFIX1 |= ((((this->prefixes).prefixes[i - 3]) & 0xFF) << (8*(i-3)));
					}
					NRF_RADIO->RXADDRESSES |= 1 << (i+1);
				}
			}

			success = true;
		}
		else if ((this->preamble).size == 5) {
			NRF_RADIO->BASE0 = (
				(((uint32_t)(this->preamble).pattern[1]) << 24) |
				(((uint32_t)(this->preamble).pattern[2]) << 16) |
				(((uint32_t)(this->preamble).pattern[3]) << 8) |
				(((uint32_t)(this->preamble).pattern[4]))
			);
			NRF_RADIO->PREFIX0 = (this->preamble).pattern[0] & RADIO_PREFIX0_AP0_Msk;
			if (this->prefixes.number > 0) {
				NRF_RADIO->BASE1 = (
					(((uint32_t)(this->preamble).pattern[1]) << 24) |
					(((uint32_t)(this->preamble).pattern[2]) << 16) |
					(((uint32_t)(this->preamble).pattern[3]) << 8) |
					(((uint32_t)(this->preamble).pattern[4]))
				);
				for (size_t i=0;i<this->prefixes.number;i++) {
					if (this->prefixes.number < 3) {
						NRF_RADIO->PREFIX0 |= ((((this->prefixes).prefixes[i]) & 0xFF) << (8*(i+1)));
					}
					else {
						NRF_RADIO->PREFIX1 |= ((((this->prefixes).prefixes[i - 3]) & 0xFF) << (8*(i-3)));
					}
					NRF_RADIO->RXADDRESSES |= 1 << (i+1);
				}

			}
			success = true;
		}
	}

	return success;
}

bool Radio::generatePcnf0Register() {
	if (this->phy == DOT15D4_NATIVE) {
		NRF_RADIO->PCNF0 = (8 << RADIO_PCNF0_LFLEN_Pos) |
		(RADIO_PCNF0_PLEN_32bitZero << RADIO_PCNF0_PLEN_Pos) |
		(RADIO_PCNF0_CRCINC_Include << RADIO_PCNF0_CRCINC_Pos);
		return true;
	}
	NRF_RADIO->PCNF0 =  ((this->header).s0 << RADIO_PCNF0_S0LEN_Pos)
	| ((this->header).length << RADIO_PCNF0_LFLEN_Pos)
	| ((this->header).s1 << RADIO_PCNF0_S1LEN_Pos);
	return true;
}

bool Radio::generatePcnf1Register() {
	if (this->phy == DOT15D4_NATIVE) {
		NRF_RADIO->PCNF1 = (128UL << RADIO_PCNF1_MAXLEN_Pos);
		return true;
	}
	NRF_RADIO->PCNF1 = (((this->whitening == HARDWARE_WHITENING ? RADIO_PCNF1_WHITEEN_Enabled : RADIO_PCNF1_WHITEEN_Disabled) << RADIO_PCNF1_WHITEEN_Pos)  & RADIO_PCNF1_WHITEEN_Msk) |
	(((this->endianness == LITTLE ? RADIO_PCNF1_ENDIAN_Little : RADIO_PCNF1_ENDIAN_Big) << RADIO_PCNF1_ENDIAN_Pos) & RADIO_PCNF1_ENDIAN_Msk)  |
	((((this->preamble).size-1) << RADIO_PCNF1_BALEN_Pos) & RADIO_PCNF1_BALEN_Msk ) |
	((this->expandPayloadLength << RADIO_PCNF1_STATLEN_Pos) & RADIO_PCNF1_STATLEN_Msk) |
	((this->payloadLength << RADIO_PCNF1_MAXLEN_Pos) & RADIO_PCNF1_MAXLEN_Msk);
	return true;
}
bool Radio::generateDataWhiteIvRegister() {
	bool success = true;
	if (this->whitening == HARDWARE_WHITENING) {
		NRF_RADIO->DATAWHITEIV = this->whiteningDataIv & 0x3F;
	}
	return success;
}
bool Radio::generateCrcRegisters() {
	bool success = true;
	if (this->crc == HARDWARE_CRC) {
		uint8_t crcsize = 0x00;
		switch (this->crcSize) {
			case 0:
				crcsize = 0x00;
				break;
			case 1:
				crcsize = RADIO_CRCCNF_LEN_One;
				break;
			case 2:
				crcsize = RADIO_CRCCNF_LEN_Two;
				break;
			case 3:
				crcsize = RADIO_CRCCNF_LEN_Three;
				break;
			default:
				crcsize = 0x00;
				break;
		}
		uint32_t skip = 0;
		if (this->phy == DOT15D4_NATIVE) {
			skip = RADIO_CRCCNF_SKIPADDR_Ieee802154 << RADIO_CRCCNF_SKIP_ADDR_Pos;
		}
		else {
			skip = ((this->crcSkipAddress ? RADIO_CRCCNF_SKIP_ADDR_Skip : RADIO_CRCCNF_SKIP_ADDR_Include) << RADIO_CRCCNF_SKIP_ADDR_Pos);
		}
		NRF_RADIO->CRCCNF = ((crcsize << RADIO_CRCCNF_LEN_Pos) << RADIO_CRCCNF_LEN_Pos) | skip;
		NRF_RADIO->CRCINIT = this->crcInit;
		NRF_RADIO->CRCPOLY = this->crcPoly;
	}
	else {
		NRF_RADIO->CRCCNF = 0x0;
		NRF_RADIO->CRCINIT = 0xFFFF;
		NRF_RADIO->CRCPOLY = 0x11021;
	}
	return success;
}


/**
 * Re-configure radio to use a different frequency and IV as fast as possible.
 */

bool Radio::fastFrequencyChange(int frequency,uint8_t iv) {
    /* Switch radio into frequency change mode. */

    /* Disable radio interrupts. */
    NVIC_DisableIRQ(RADIO_IRQn);

    /* 
     * Disable DISABLED->TXEN and DISABLED->RXEN shorts to force
     * radio to go in idle mode once disabled instead of enabling
     * TX or RX modes.
     */
    NRF_RADIO->SHORTS &= ~(RADIO_SHORTS_DISABLED_TXEN_Msk | RADIO_SHORTS_DISABLED_RXEN_Msk);

    /* Same reasoning as SHORTS above, for the PPI-based routing: CH_RXEN/
     * CH_TXEN both fire on EVENTS_DISABLED, so leaving them enabled here
     * would immediately re-arm RX/TX on the OLD frequency the instant
     * TASKS_DISABLE completes below, before FREQUENCY gets updated further
     * down. armAddressMatchTX() re-establishes the right state afterwards. */
    NRF_PPI->CHENCLR = (1 << RADIO_PPI_CH_RXEN) | (1 << RADIO_PPI_CH_TXEN) | (1 << RADIO_PPI_CH_DEVMATCH);

    /* Disable radio, block until radio is disabled. */
    NRF_RADIO->EVENTS_DISABLED = 0;
	NRF_RADIO->TASKS_DISABLE = 1;
	while (NRF_RADIO->EVENTS_DISABLED == 0);

    /* Change PACKETPTR to current RX descriptor. */
    if (this->rxDesc == NULL) {
        this->rxDesc = allocRxDesc();
        if (this->rxDesc == NULL) return false;
    }
    
    NRF_RADIO->PACKETPTR = (uint32_t)(this->rxDesc->payload);

    /* Radio is disabled and not in RX or TX state, change frequency. */
	NRF_RADIO->FREQUENCY = frequency;
	this->frequency = frequency;
	NRF_RADIO->DATAWHITEIV = iv;
	this->whiteningDataIv = iv;

	/*
     * Re-configure the shorts, including DISABLED->RXEN, based on current
     * radio configuration. As in enable(), DISABLED_RXEN is only wired
     * directly here when there is no address filter; with a filter active,
     * DAB/DAP/DACNF (set by enable() and untouched by this function) are
     * still in effect across this channel change, so RXEN/TXEN routing
     * after the next reception must keep going through PPI instead (see
     * armAddressMatchTX()) rather than being decided reactively at
     * DEVMATCH time or too late in the END handler.
     */
    NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;
    if (!this->isFilterEnabled()) {
        NRF_RADIO->SHORTS |= RADIO_SHORTS_DISABLED_RXEN_Msk;
    }
    this->armAddressMatchTX();

    /* Configure short for RSSI measurement if required. */
    if (this->rssi) {
        NRF_RADIO->SHORTS |= RADIO_SHORTS_ADDRESS_RSSISTART_Msk;
    }

    /* If matching is enabled, configure BCC, add short for ADDRESS->BCSTART and enable BCMATCH interrupt. */
    if (this->isMatchingEnabled()) {
        NRF_RADIO->BCC = this->matchingSize;
        NRF_RADIO->SHORTS |= RADIO_SHORTS_ADDRESS_BCSTART_Msk;
        NRF_RADIO->INTENSET |= 1 << 10; // enable BCMATCH event
    }

    /* Clear events. */
    NRF_RADIO->EVENTS_READY = 0;
	NRF_RADIO->EVENTS_END = 0;
    NRF_RADIO->EVENTS_RSSIEND = 0;

    /* Clear pending radio interrupts and enable interrupts again. */
	NVIC_ClearPendingIRQ(RADIO_IRQn);
	NVIC_EnableIRQ(RADIO_IRQn);

	/* Put radio in reception mode and let our IRQ handler process packets. */
    NRF_RADIO->TASKS_RXEN = 1;

    /* Put radio state back to RX (frequency change successfully performed). */
    this->state = RX;
	return true;
}

bool Radio::enable() {
	bool success = true;
	//this->disable();
    this->reportPdu = true;

    /* Wait for HFCLK to be started. */
	NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
	NRF_CLOCK->TASKS_HFCLKSTART = 1;
	while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0);

    /* Configure nRF registers based on current settings. */
	success = this->generateTxPowerRegister();
	if (success) success = this->generateModeRegister();
	if (success) success = this->generateModeCnf0Register();
	if (success) success = this->generateFrequencyRegister();
	if (success) success = this->generateBaseAndPrefixRegisters();
	if (success) success = this->generateCrcRegisters();
	if (success) success = this->generatePcnf0Register();
	if (success) success = this->generatePcnf1Register();
	if (success) success = this->generateDataWhiteIvRegister();


	if (success) {
        /* Configure TIFS register. */
		NRF_RADIO->TIFS = this->interFrameSpacing;

        /* Prepare a RX descriptor and configure radio to use it. */
        if (this->rxDesc == NULL) {
            this->rxDesc = allocRxDesc();
        }
        if (this->rxDesc != NULL) {
            /* Mark descriptor as pending and insert it into our RX queue. */
            this->rxDesc->state = DESC_PENDING;

            /* Set PACKETPTR to this descriptor's payload. */
            NRF_RADIO->PACKETPTR = (uint32_t)(this->rxDesc->payload);
        }

		//NRF_RADIO->PACKETPTR = (uint32_t)(this->rxBuffer);
		if (this->encryption) {
			NRF_RADIO->PACKETPTR = (uint32_t)(this->tmpBuffer);
			NRF_CCM->INPTR = (uint32_t)(this->tmpBuffer);
			NRF_CCM->OUTPTR = (uint32_t)(this->rxBuffer);
			NRF_CCM->MODE = (CCM_MODE_MODE_Decryption << CCM_MODE_MODE_Pos) |
			 								(CCM_MODE_DATARATE_1Mbit << CCM_MODE_DATARATE_Pos) |
			 								(CCM_MODE_LENGTH_Extended << CCM_MODE_LENGTH_Pos);
			bsp_board_led_on(0);
			bsp_board_led_on(1);
		// TODO: update the INPTR and OUTPTR, maybe in interrupt too
		// TODO: add AES interrupt to manage state, or maybe reading right registers is enough ?
		}
		else {
		// TODO: update the INPTR and OUTPTR
		}
		if (this->mode == MODE_NORMAL) {

            /* Ask for END, CRCOK, CRCERROR and DISABLE events only. */
			NRF_RADIO->INTENSET = RADIO_INTENSET_END_Msk /* | RADIO_INTENSET_CRCOK_Msk | RADIO_INTENSET_CRCERROR_Msk*/ | RADIO_INTENSET_TXREADY_Msk;

			if (this->isFilterEnabled()) {

				NRF_RADIO->EVENTS_DEVMISS = 0;
				NRF_RADIO->EVENTS_DEVMATCH = 0;

				/* Deliberately NOT enabling RADIO_INTENSET_DEVMATCH_Msk: taking
				 * a CPU interrupt for DEVMATCH while a reception can still be
				 * in progress corrupts the in-flight EasyDMA transfer on this
				 * chip, regardless of what the handler does in response (this
				 * was confirmed experimentally - disabling the interrupt while
				 * forcing the match decision in software made the corruption
				 * disappear entirely). The comparator still sets
				 * EVENTS_DEVMATCH/EVENTS_DEVMISS on its own either way, so we
				 * just read that flag later, after the reception has fully
				 * completed (see the END handler in RADIO_IRQHandler) instead
				 * of reacting to it via an interrupt. */

				/* Match either address type (random or public) against the
				 * same target address, using both device address slots. */
				NRF_RADIO->DAB[0] = ((uint32_t)(this->filter.bytes[2] << 24) |
				(uint32_t)(this->filter.bytes[3] << 16) |
				(uint32_t)(this->filter.bytes[4] << 8) |
				(uint32_t)(this->filter.bytes[5]));
				NRF_RADIO->DAP[0] = (uint32_t)(this->filter.bytes[0] << 8) | (uint32_t)(this->filter.bytes[1]);

				NRF_RADIO->DAB[1] = NRF_RADIO->DAB[0];
				NRF_RADIO->DAP[1] = NRF_RADIO->DAP[0];
				NRF_RADIO->DACNF = (1 << 8) | 3;
			}
			else {
				NRF_RADIO->EVENTS_DEVMISS = 0;
				NRF_RADIO->EVENTS_DEVMATCH = 0;

				NRF_RADIO->DAB[0] = 0;
				NRF_RADIO->DAP[0] = 0;
				NRF_RADIO->DAB[1] = 0;
				NRF_RADIO->DAP[1] = 0;
				NRF_RADIO->DACNF = 0;
			}


            /* Clear and enable interrupts. */
			NVIC_ClearPendingIRQ(RADIO_IRQn);
			NVIC_EnableIRQ(RADIO_IRQn);

            /* Configure shorts for continuous RX (or TX after a matched RX).
             * DISABLED_RXEN is only wired here directly when there is no
             * address filter: with a filter active, RXEN/TXEN after this
             * reception are routed entirely through PPI instead (see
             * armAddressMatchTX()), since deciding this reactively - whether
             * from the DEVMATCH IRQ (corrupts the in-flight EasyDMA transfer)
             * or from software in the END handler (loses the race against
             * the hardware's own END_DISABLE auto-chain) - doesn't work. */
            NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;
            if (!this->isFilterEnabled()) {
                NRF_RADIO->SHORTS |= RADIO_SHORTS_DISABLED_RXEN_Msk;
            }
            this->armAddressMatchTX();

            /* If RSSI measurement is required, configure the ADDRESS->RSSISTART short. */
			if (this->rssi) {
				NRF_RADIO->SHORTS |= RADIO_SHORTS_ADDRESS_RSSISTART_Msk;
			}

            /* If matching is enabled, configure BCC, add short for ADDRESS->BCSTART and enable BCMATCH interrupt. */
			if (this->isMatchingEnabled()) {
				NRF_RADIO->BCC = this->matchingSize;
				NRF_RADIO->SHORTS |= RADIO_SHORTS_ADDRESS_BCSTART_Msk;
				NRF_RADIO->INTENSET |= 1 << 10; // enable BCMATCH event
			}

            /* Reset END and READY events. */
			NRF_RADIO->EVENTS_END = 0;
			NRF_RADIO->EVENTS_READY = 0;
            NRF_RADIO->EVENTS_CRCOK = 0;
            NRF_RADIO->EVENTS_CRCERROR = 0;
            NRF_RADIO->EVENTS_TXREADY = 0;
			NRF_RADIO->TASKS_RXEN = 1;

            /* State is RX by default. */
			this->state = RX;

		}
		else if (this->mode == MODE_JAMMER) {
			NRF_RADIO->INTENSET = 0x00000008;
			NVIC_ClearPendingIRQ(RADIO_IRQn);
			NVIC_EnableIRQ(RADIO_IRQn);

			NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_TXEN_Msk;
			if (this->rssi) {
				NRF_RADIO->SHORTS |= RADIO_SHORTS_ADDRESS_RSSISTART_Msk;
			}
			if (this->jammingPatternsEnabled) {
				NRF_RADIO->BCC = this->jammingPatternsCounter;//8+6*2*8;
				NRF_RADIO->SHORTS |= RADIO_SHORTS_ADDRESS_BCSTART_Msk;
				NRF_RADIO->INTENSET |= 1 << 10; // enable BCMATCH event
			}
			NRF_RADIO->EVENTS_READY = 0;
			NRF_RADIO->EVENTS_END = 0;
			NRF_RADIO->TASKS_RXEN = 1;
			this->state = JAM_RX;
		}
		else if (this->mode == MODE_ENERGY_DETECTION) {
			NRF_RADIO->INTENSET = 0x00008000;
			NVIC_ClearPendingIRQ(RADIO_IRQn);
			NVIC_EnableIRQ(RADIO_IRQn);
			NRF_RADIO->SHORTS = RADIO_SHORTS_READY_EDSTART_Msk;
			NRF_RADIO->EVENTS_READY = 0;
			NRF_RADIO->EVENTS_END = 0;
			NRF_RADIO->EDCNT = 10;
			NRF_RADIO->TASKS_EDSTART = 1;
			this->state = ENERGY_DETECTION;
		}
	}
	return success;
}

bool Radio::reload() {
	this->disable();
	return this->enable();
}

bool Radio::updateTXBuffer(uint8_t *data, size_t size) {
    /* Descriptor size cannot exceed MAX_PAYLOAD_SIZE. */
    if (size > MAX_PAYLOAD_SIZE) {
        size = MAX_PAYLOAD_SIZE;
    }

    /* Update our TX buffer with the provided data. */
    memcpy(this->txBuffer, data, size);

    /* Pending TX buffer to transmit. */
    this->pendingTx = true;

    /* Success ! */
	return true;
}

int Radio::getMatchingSize() {
	return this->matchingSize;
}

bool Radio::send(uint8_t *data,int size,int frequency, uint8_t channel) {
	bsp_board_led_invert(0);
    NVIC_DisableIRQ(RADIO_IRQn);
	NRF_RADIO->SHORTS = 0;
	NRF_RADIO->EVENTS_DISABLED = 0;
	NRF_RADIO->TASKS_DISABLE = 1;
	while(NRF_RADIO->EVENTS_DISABLED == 0);

	NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_RXEN_Msk;
	if (this->rssi) {
		NRF_RADIO->SHORTS |= RADIO_SHORTS_ADDRESS_RSSISTART_Msk;
	}
	NRF_RADIO->FREQUENCY = frequency;
	NRF_RADIO->DATAWHITEIV = channel;

    /* We need to set our PACKETPTR to the provided buffer's address. */
	NRF_RADIO->PACKETPTR = (uint32_t)data;

	if (this->encryption) {
		memcpy(this->tmpBuffer, data, size);
		NRF_RADIO->PACKETPTR = (uint32_t)(this->txBuffer);
		NRF_CCM->INPTR = (uint32_t)(this->tmpBuffer);
		NRF_CCM->OUTPTR = (uint32_t)(this->txBuffer);
		NRF_CCM->MODE = (CCM_MODE_MODE_Encryption << CCM_MODE_MODE_Pos) |
										(CCM_MODE_DATARATE_1Mbit << CCM_MODE_DATARATE_Pos) |
										(CCM_MODE_LENGTH_Extended << CCM_MODE_LENGTH_Pos);

	// TODO: update the INPTR and OUTPTR, maybe in interrupt too
	// TODO: add AES interrupt to manage state, or maybe reading right registers is enough ?
	}


	// Turn on the transmitter, and wait for it to signal that it's ready to use.
	this->state = TX;
	NRF_RADIO->INTENSET =  0x00000008;

	NVIC_EnableIRQ(RADIO_IRQn);
	NRF_RADIO->EVENTS_READY = 0;
	NRF_RADIO->EVENTS_END = 0;
	NRF_RADIO->TASKS_TXEN = 1;
    return false;
}

static uint8_t jamBuffer[] = {0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
extern "C" void RADIO_IRQHandler(void) {
    /**
     * Process DEVMATCH event. This event is triggered when filtering is enabled
     * and when the first 48-bit of a packet's payload match one of our configured
     * 48-bit Bluetooth Device address.
     *
     * This happens when filtering mode is set to any value but `FilterMode::Disabled`.
     **/
    if (NRF_RADIO->EVENTS_DEVMATCH == 1) {
        /* Forward to event handler. */
        Radio::instance->onDevMatchEvt();

        /* Ack event. */
        NRF_RADIO->EVENTS_DEVMATCH = 0;
    }

    /**
     * Process DEVMISS event. This event is triggered when filtering is enabled but
     * the current received payload's first 48-bit value does not match one of our
     * configured 48-bit addresses.
     **/
    if (NRF_RADIO->EVENTS_DEVMISS == 1) {
        /* Forward to event handler. */
        Radio::instance->onDevMissEvt();

        /* Ack event. */
        NRF_RADIO->EVENTS_DEVMISS = 0;
    }

    /* Process READY event (should not be triggered, not enabled by default). */
	if (NRF_RADIO->EVENTS_READY) {
		NRF_RADIO->EVENTS_READY = 0;
		NRF_RADIO->TASKS_START = 1;
	}

    /* Process BCMATCH (enabled when filter is enabled). */
	if (NRF_RADIO->EVENTS_BCMATCH) {
		NRF_RADIO->EVENTS_BCMATCH = 0;
		if (Radio::instance->getMode() == MODE_NORMAL) {
			Controller *controller = Radio::instance->getController();
			controller->onMatch(Radio::instance->rxBuffer, Radio::instance->getMatchingSize());
			NRF_RADIO->TASKS_BCSTOP = 1;
		}
	}
	
    /* Process EDEND interrupt. */
    if (NRF_RADIO->EVENTS_EDEND) {
        /* Forward to event handler. */
        Radio::instance->onEnergyDetectionEvt();

        /* Ack event and restart energy detection measurements. */
		NRF_RADIO->EVENTS_EDEND = 0;
		NRF_RADIO->TASKS_EDSTART = 1;
	}

    /* Process RSSI measure event (disabled by default). */
    if (NRF_RADIO->EVENTS_RSSIEND) {
        /* Forward to event handler. */
        Radio::instance->onRssiEvt();

        /* Enable RSSI measurement again (triggered by shorts). */
        NRF_RADIO->EVENTS_RSSIEND = 0;
    }

    if (NRF_RADIO->EVENTS_TXREADY) {
        /* TX is in progress, disable the DISABLED->TXEN short and prepare PACKETPTR for the next RX. */
        NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_RXEN_Msk;        

        /* Check if we need to remove or not the DISABLED_TXEN short. */
        NRF_RADIO->EVENTS_TXREADY = 0;
    }

    /**
     * Process an END event that indicates we successfully received a payload.
     * `Radio::onPacketEvt()` is in charge of handling such event and processes
     * the received payload. It also configures the radio's next RX or TX event
     * based on the current configuration.
     **/
    if (NRF_RADIO->EVENTS_END) {
        /* Ack event. */
        NRF_RADIO->EVENTS_END = 0;

        Radio::instance->onPacketEvt();
	}
}

/**
 * RX FIFO management.
 *
 * This is a basic pre-allocated FIFO used to store up to MAX_RX_DESCRIPTORS
 * RX descriptors (payload + metadata) in a form of a ring buffer.
 *
 * Our FIFO is a simple array of `radio_desc_t` structures and we track its
 * head and tail with `rxHead` and `rxTail`. When `rxHead` and `rxTail` are
 * equal, the FIFO is empty. 
 *
 * When we allocate a descriptor, we simply increments `rxHead` (moving forward)
 * and return the address of the corresponding descriptor. If `rxHead` is just
 * behind `rxTail`, the FIFO is full and allocation fails. The allocated descriptor
 * is then passed to the radio and used to receive a payload.
 *
 * Once a payload has been written into the allocated descriptor and processed,
 * we "free" it by incrementing `rxTail`.
 *
 * As a summary:
 * - `rxTail` never goes past `rxHead`;
 * - When `rxHead` and `rxTail` are equal, FIFO is empty;
 * - When next `rxHead` value equals `rxTail`, FIFO is full;
 * - `rxTail` refers to the last pending RX payload to process;
 * - `rxHead` refers to the last available buffer where payload must be written.
 **/


/**
 * Count the free remaining descriptors in our RX FIFO.
 **/

size_t Radio::countFreeRxDesc(void) {
    size_t count = 0;

    /* Check if we still have a free slot. */
    uint32_t primask = enterCritical();
    if (this->rxHead < this->rxTail) {
        count = (this->rxHead + MAX_RX_DESCRIPTORS) - this->rxTail - 1;
    } else if (this->rxHead > this->rxTail) {
        count = this->rxHead - this->rxTail;
    }
    exitCritical(primask);
    
    /* Return result. */
    return count;
}


/**
 * "Allocate" a new RX descriptor by moving our FIFO's head forward
 * (if not full) and returning a pointer to the new head's descriptor.
 **/

radio_desc_t *Radio::allocRxDesc(void) {
    radio_desc_t *p_slot = NULL;
    unsigned int new_head = (this->rxHead + 1) % MAX_RX_DESCRIPTORS;

    uint32_t primask = enterCritical();
    if (new_head != this->rxTail) {
        /* Move head one slot ahead, return NULL if full. */
        this->rxHead = new_head;
        /* Return current slot (head). */
        p_slot = &this->rxDescPool[this->rxHead];
    }
    exitCritical(primask);

    /* Mark descriptor as brand new. */
    p_slot->state = DESC_PENDING;

    /* Return allocated slot. */
    return p_slot;
}


/**
 * Return the last item in our RX FIFO, leaves FIFO's tail
 * untouched.
 **/

radio_desc_t *Radio::lastRxDesc(void) {
    radio_desc_t *p_slot = NULL;

    /* No more descriptors in FIFO. */
    if (this->rxTail == this->rxHead)
        return NULL;

    /* Return the oldest descriptor in FIFO. */
    uint32_t primask = enterCritical();
    p_slot = &this->rxDescPool[this->rxTail];
    exitCritical(primask);

    return p_slot;
}

/**
 * Advance our RX FIFO's tail ("free" the last descriptor
 * of the FIFO after it has been processed).
 **/

void Radio::freeLastRxDesc(void) {
    uint32_t primask = enterCritical();
    if (this->rxTail != this->rxHead) {
        this->rxTail = (this->rxTail + 1) % MAX_RX_DESCRIPTORS;
    }
    exitCritical(primask);
}

/**
 * Determine if we need to send our pending TX buffer.
 **/

bool Radio::mustSwitchToTx(void) {
    /** 
     * These conditions must be met to allow the Radio to go into TX:
     * - Filter is set to TxOnly and we received a matching PDU and we have a pending TX buffer
     * - Filter is set to RxTx and we received a matching PDU and we have a pending TX buffer
     **/
    if (this->filterMode == FilterMode::TxOnly || this->filterMode == FilterMode::RxTx) {
        return (this->addrMatch && this->pendingTx);
    } else {
        return false;
    }
}


/**
 * Device match event handler.
 *
 * This event handler determine based on the current filter mode and auto TX feature
 * if Radio needs to transmit a pending TX buffer once the current payload received.
 * 
 * Deciding before the complete reception of the payload saves time and makes the
 * END event handler faster.
 **/

void Radio::onDevMatchEvt() {
    this->addrMatch = true;

    /* Must report PDU on address match by default. */
    this->reportPdu = true;

    /* Should we switch to TX right after RX? */
    switch (this->filterMode) {
        case FilterMode::RxTx:
        case FilterMode::TxOnly:
            this->switchToTx = (this->pendingTx && this->isAutoTXafterRXenabled());
            break;

        default:
            this->switchToTx = false;
            break;
    }
}


/**
 * Device miss event handler.
 *
 * This handler is called when filtering is enabled and current payload does not
 * match. Decision to switch the radio in TX mode once the current payload received
 * is solely based on the current filtering mode, auto TX feature and if a pending
 * TX buffer is present.
 *
 * The same conditions applied when the radio was first configured and on device match
 * through PPI, so we definitely need to set a TX buffer if TX is already planned.
 **/

void Radio::onDevMissEvt() {
    this->addrMatch = false;

    /* Only report PDU if not in strict RX filtering (RxOnly, RxTx). */
    switch (this->filterMode) {
        case FilterMode::RxTx:
        case FilterMode::RxOnly:
            this->reportPdu = false;
            break;

        default:
            this->reportPdu = true;
            break;
    }

    switch (this->filterMode) {
        /* In RxTx or TxOnly mode, TX happens only if address matches. */
        case FilterMode::RxTx:
        case FilterMode::TxOnly:
            this->switchToTx = false;
            break;

        /* In other modes, it depends on autoTXafterRXenabled and pendingTx. */
        default:
            this->switchToTx = (this->isAutoTXafterRXenabled() && this->pendingTx);
            break;
    }
}


/**
 * Energy detection event handler.
 **/

void Radio::onEnergyDetectionEvt() {
    uint8_t sample = NRF_RADIO->EDSAMPLE;
    NRF_TIMER4->TASKS_CAPTURE[5] = 1UL;
    uint32_t now = NRF_TIMER4->CC[5];

    /* Send ED sample to controller. */
    if (this->controller != NULL) {
        this->controller->onEnergyDetection(now, sample);
    }
}


/**
 * RSSI event handler.
 *
 * Updates the current RX descriptor's RSSI.
 **/

void Radio::onRssiEvt() {
    /* Set current RX descriptor RSSI. */
    if (this->rxDesc != NULL) {
        this->rxDesc->rssi = NRF_RADIO->RSSISAMPLE;
    }
}


/**
 * END event handler.
 *
 * This is the main and more complex event handler that processes
 * incoming payloads and automatically configures the radio for the
 * next event (TX or RX).
 *
 * It is also in charge to notify the controller that a payload has
 * been received, taking advantage of the radio' shorts that make it
 * automatically switch from one mode to another, as well as automatically
 * handle the next TX or RX event. This gives us a few microseconds to
 * forward the received payload to the controller before a new payload
 * is received and this IRQ triggered again.
 *
 * This event handler is *very sensitive* to timing, be careful when
 * modifying it.
 **/

void Radio::onPacketEvt() {
    /* Retrieve the current timestamp. */
    NRF_TIMER4->TASKS_CAPTURE[5] = 1UL;
    uint32_t now = NRF_TIMER4->CC[5];

    /* Process the received payload. */
    if (this->getMode() == MODE_NORMAL) {
        switch (this->getState()) {
            case RX:
                {
                    radio_desc_t *p_pkt = Radio::instance->rxDesc;
                    if (this->switchToTx) {

                        /* RXEN/TXEN routing for THIS reception was already
                         * decided in hardware by PPI the instant DEVMATCH
                         * fired (see armAddressMatchTX()) - nothing to do for
                         * SHORTS here. Safe to write PACKETPTR here though:
                         * this reception is fully complete (we're in the END
                         * handler), so no DMA transfer is in flight anymore. */
                        NRF_RADIO->PACKETPTR = (uint32_t)(this->txBuffer);

                        /* Update RX descriptor. If we run out of descriptors, keep the
                         * one we have and drop this frame (skip it below) instead of
                         * both freeing it and leaving it as the active rxDesc/PACKETPTR
                         * target, which would let it be handed out again while still
                         * being written to by the radio.
                         *
                         * No critical section needed here: popFreeDesc() locks itself,
                         * and rxDesc is only ever touched from this handler or from
                         * enable()/fastFrequencyChange() which disable RADIO_IRQn first. */
                        radio_desc_t *p_next = allocRxDesc();
                        if (p_next != NULL) {
                            this->rxDesc = p_next;
                        } else {
                            p_pkt = NULL;
                        }

                        setState(TX);
                        this->switchToTx = false;

                        /* Switch to RX state, let hardware send the current TX buffer. */
                        bsp_board_led_on(0);

                    } else {
                        /* Give radio another RX descriptor to write into. If the pool
                         * is exhausted, keep using the current one (PACKETPTR/rxDesc
                         * stay untouched) and drop this frame below instead of freeing
                         * it, to avoid it being both "free" and the live DMA target.
                         *
                         * No critical section needed here: see the comment above. */
                        radio_desc_t *p_next = allocRxDesc();
                        if (p_next != NULL) {
                            NRF_RADIO->PACKETPTR = (uint32_t)(p_next->payload);
                            this->rxDesc = p_next;
                        } else {
                            p_pkt = NULL;
                        }

                        /* Re-evaluate whether the *next* reception should be
                         * allowed to auto-TX on a match: txBuffer may have been
                         * populated (or emptied) since this was last decided.
                         * Safe here for the same reason as above - no transfer
                         * is active yet for the reception this arms. */
                        armAddressMatchTX();
                    }

                    /* From now, if the radio starts receiving a new packet it will be
                     * written into the new descriptor's buffer.
                     */
                  
                    /* If filter is enabled, we must only send packets that match
                     * the specified address.
                     *
                     * p_pkt (the descriptor the rejected packet landed in) must be
                     * returned to the free list here before bailing out: by this
                     * point the hardware has already been handed a fresh descriptor
                     * for the next reception (above), so p_pkt is not referenced
                     * anywhere else and this is its only chance to be freed.
                     * Without this, every rejected packet leaks one descriptor
                     * permanently - with filtering typically enabled while waiting
                     * for one specific device's advertisement amid otherwise-busy
                     * RF traffic, this exhausts the entire 30-descriptor pool within
                     * seconds. */
                    

                    /* Drop RX PDU if no need to report. */
                    if (!this->reportPdu) {
                        if (p_pkt != NULL) {
                            /* Free last RX descriptor in our FIFO. */
                            freeLastRxDesc();

                            /* Reset reportPdu for next RX event. */
                            this->reportPdu = true;
                        }
                        return;
                    }

                    /* Save RSSI, CRC info and save packet into RX queue. */
                    if (p_pkt != NULL) {
                        if (this->rssi) {
                            p_pkt->rssi = NRF_RADIO->RSSISAMPLE;
                        }

                        if (this->crc == HARDWARE_CRC) {
                            p_pkt->crc.validity = (NRF_RADIO->CRCSTATUS == 1)?VALID_CRC:INVALID_CRC;
                            p_pkt->crc.value = NRF_RADIO->RXCRC;
                        }

                        /* Process RX packet. Widened to avoid an 8-bit wraparound: a
                         * sender-declared length near 254-255 would otherwise make
                         * "2+length" wrap back to a tiny value, sail past the sanity
                         * check below, and get reported as if that few bytes were
                         * genuinely captured (they weren't - DMA only wrote the
                         * hardware's configured MAXLEN). */
                        uint16_t bufferSize = 0;
                        if (this->phy == DOT15D4_NATIVE)  {
                            bufferSize = 128;
                        }
                        else {
                            if (this->header.s0 != 0) {
                                bufferSize += 1;
                            }
                            if (this->header.s1 != 0) {
                                bufferSize += 1;
                            }
                            if (this->header.length != 0) {
                                bufferSize += 1+(this->header.s0 == 0 ? p_pkt->payload[0] : p_pkt->payload[1]);
                            }
                            else {
                                bufferSize += this->payloadLength;
                            }
                        }

                        /* Update current timestamp. */
                        if (this->phy == DOT15D4_NATIVE) {
                                this->currentTimestamp = now - ((bufferSize + 5) * 8 * 4) - 100;
                        }
                        else {
                            this->currentTimestamp = now - (this->preamble.size + bufferSize)  * 4 * (this->phy == BLE_2MBITS || this->phy == ESB_2MBITS ? 1 : 2) - 100;
                        }

                        /* Save packet timestamp in descriptor. */
                        p_pkt->timestamp = this->currentTimestamp;

                        /* Process received frame (payload) and add to RX queue. */
                        if (bufferSize <= 2 + this->payloadLength) {
                            p_pkt->size = bufferSize;
                        } else {
                            p_pkt->size = 0;
                        }

                        /* Mark descriptor has ready. */
                        p_pkt->state = DESC_READY;

                        /* Notify the controller we received a packet. */
                        if (this->controller != NULL) {
                            /* Forward received frame to controller. */
                            this->controller->onReceive(p_pkt->timestamp, p_pkt->size, p_pkt->payload, p_pkt->crc, p_pkt->rssi);
                        }

                        /* Free descriptor once the controller has processed it. */
                        freeLastRxDesc();
                    } else {
                        bsp_board_led_on(0);
                    }
                }
                break;

            /* TX buffer sent, we must switch PACKETPTR to rxDesc. */
            case TX:
                {
                    NRF_RADIO->PACKETPTR = (uint32_t)(this->rxDesc->payload);

                    /* Pending TX is no more pending (sent). */
                    this->pendingTx = false;

                    /* Re-arm PPI for RX-only. */
                    armAddressMatchTX();
                    this->addrMatch = false;
                    
                    /* Next step: process received frame. */
                    setState(RX);

                    /* Notify controller packet has successfully been sent. */
                    if (this->controller != NULL) {
                        this->controller->onSent(now);
                    }

                    bsp_board_led_off(0);
                }
                break;

            default:
                /* Nothing to do. */
                break;
        }
    } else if (this->mode == MODE_JAMMER) {
        if (this->state == JAM_RX) {
            NRF_RADIO->PACKETPTR = (uint32_t)jamBuffer;
            NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_RXEN_Msk | RADIO_SHORTS_ADDRESS_BCSTART_Msk;
            setState(JAM_TX);
        }
        else if (this->state == JAM_TX) {
            NRF_RADIO->PACKETPTR = (uint32_t)this->rxBuffer;
            if (this->jammingInterval == 0) {
                    NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_TXEN_Msk | RADIO_SHORTS_ADDRESS_BCSTART_Msk;
                    setState(JAM_RX);
                    if (this->controller != NULL) {
                        this->controller->onJam(now);
                    }
            }
            else {
                NRF_RADIO->SHORTS = 0;
                nrf_delay_us(jammingInterval);
                if (this->controller != NULL) {
                    this->controller->onJam(now);
                }
                reload();
            }
        }
    }
}

