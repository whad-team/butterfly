#include "radio.h"
#include "bsp.h"
#include "nrf.h"
// Global instance of Radio
Radio* Radio::instance = NULL;

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
	this->jammingPatternsEnabled = false;
	this->jammingPatternsCounter = 0;
	this->jammingInterval = 0;
	this->encryption = false;

    /* Initialize jamming patterns queue. */
	this->initJammingPatternsQueue();

    /* Update instance reference (pseudo-singleton). */
	instance = this;

    /* Initialize descriptors pool (linked-list). */
    for (int i=0; i < MAX_DESCRIPTORS; i++) {
        this->descPool[i].state = DESC_FREE;

        /* Configure previous and next pointers. */
        if (i == 0) {
            this->descPool[i].header.p_prev = &this->descFreeList;
        } else {
            this->descPool[i].header.p_prev = &this->descPool[i-1].header;
        }
        if (i == (MAX_DESCRIPTORS - 1)) {
            this->descPool[i].header.p_next = &this->descFreeList;
        } else {
            this->descPool[i].header.p_next = &this->descPool[i+1].header;
        }

        /* Set descriptor as free. */
        this->descPool[i].state = DESC_FREE;

        /* Initialize descriptor's properties. */
        this->descPool[i].size = 0;
        this->descPool[i].crc.value = 0;
        this->descPool[i].crc.validity = UNKNOWN_CRC; 
    }

    /* Initialize free descriptors list to our initial pool. */
    this->descFreeList.p_next = &this->descPool[0].header;
    this->descFreeList.p_prev = &this->descPool[MAX_DESCRIPTORS - 1].header;

    /* Other lists are empty (no descriptors, point to NULL). */
    this->descTxList.p_next = &this->descTxList;
    this->descTxList.p_prev = &this->descTxList;
    this->descRxList.p_next = &this->descRxList;
    this->descRxList.p_prev = &this->descRxList;

    /* Current RX descriptor. */
    this->rxDesc = NULL;
    this->txDesc = NULL;
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

bool Radio::enableFilter(BLEAddress address) {
	for (int i=0;i<6;i++) this->filter.bytes[i] = address.bytes[i];
	this->filterEnabled = true;
	return true;
}

bool Radio::isFilterEnabled() {
	return this->filterEnabled;
}

bool Radio::disableFilter() {
	for (int i=0;i<6;i++) this->filter.bytes[i] = 0;
	this->filterEnabled = false;
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

		NRF_RADIO->EVENTS_DISABLED = 0;
		NRF_RADIO->TASKS_EDSTOP = 1;
		NRF_RADIO->TASKS_DISABLE = 1;
		while (NRF_RADIO->EVENTS_DISABLED == 0) {}
		success = true;

        /* Flush RX list. */
        while (hasRxDesc()) {
            pushFreeDesc(popRxDesc());
        }
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

    /* Disable radio, block until radio is disabled. */
    NRF_RADIO->EVENTS_DISABLED = 0;
	NRF_RADIO->TASKS_DISABLE = 1;
	while (NRF_RADIO->EVENTS_DISABLED == 0);

    /* If txDesc is set and not sent, place in Tx list. */
    if (this->txDesc != NULL) {
        pushTxDesc(this->txDesc);
        this->txDesc = NULL;
    }

    /* Flush RX list. */
    while (hasRxDesc()) {
        pushFreeDesc(popRxDesc());
    }

    /* Change PACKETPTR to current RX descriptor. */
    if (this->rxDesc == NULL) {
        this->rxDesc = popFreeDesc();
        if (this->rxDesc == NULL) return false;
    }
    
    NRF_RADIO->PACKETPTR = (uint32_t)(this->rxDesc->payload);

    /* Radio is disabled and not in RX or TX state, change frequency. */
	NRF_RADIO->FREQUENCY = frequency;
	this->frequency = frequency;
	NRF_RADIO->DATAWHITEIV = iv;
	this->whiteningDataIv = iv;

	/* 
     * Re-configure the shorts, including the DISABLED->RXEN and DISABLED->TXEN
     * based on current radio configuration.
     */
    if (this->autoTXafterRXenabled) {
        NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_TXEN_Msk;
        NRF_RADIO->INTENSET |= RADIO_INTENSET_TXREADY_Msk;
        NRF_RADIO->EVENTS_TXREADY = 0;
    } else {
        NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_RXEN_Msk;
        NRF_RADIO->INTENSET &= ~(RADIO_INTENSET_TXREADY_Msk);
        NRF_RADIO->EVENTS_TXREADY = 0;
    }

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
	this->disable();


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
        this->rxDesc = popFreeDesc();
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
			if (this->isFilterEnabled()) {

				NRF_RADIO->EVENTS_DEVMISS = 0;
				NRF_RADIO->EVENTS_DEVMATCH = 0;

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

            /* Ask for END event only. */
			NRF_RADIO->INTENSET = RADIO_INTENSET_END_Msk | RADIO_INTENSET_CRCOK_Msk | RADIO_INTENSET_CRCERROR_Msk;

            /* Clear and enable interrupts. */
			NVIC_ClearPendingIRQ(RADIO_IRQn);
			NVIC_EnableIRQ(RADIO_IRQn);
		
            /* Configure shorts for continuous RX or TX. */
            if (this->autoTXafterRXenabled) {
                NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_TXEN_Msk;
                NRF_RADIO->INTENSET |= RADIO_INTENSET_TXREADY_Msk;
                NRF_RADIO->EVENTS_TXREADY = 0;
            } else {
                NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_RXEN_Msk;
                NRF_RADIO->INTENSET &= ~(RADIO_INTENSET_TXREADY_Msk);
                NRF_RADIO->EVENTS_TXREADY = 0;
            }

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

bool Radio::updateTXBuffer(uint8_t *data, uint8_t size) {
	/*
    for (int i=0;i<size;i++) {
		this->txBuffer[i] = data[i];
	}
    */
    
    /* Get a descriptor. */
    radio_desc_t *pkt_desc = popFreeDesc();
    if (pkt_desc == NULL) {
        return false;
    }

    /* Set descriptor's properties. */
    if (size > 255) size = 255;
    memcpy(pkt_desc->payload, data, size);
    pkt_desc->size = size;
    pkt_desc->state = DESC_PENDING;

    /* Add descriptor to TX queue. */
    pushTxDesc(pkt_desc);

	return true;
}

int Radio::getMatchingSize() {
	return this->matchingSize;
}

bool Radio::send(uint8_t *data,int size,int frequency, uint8_t channel) {
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
    /* Process filter-related events. */
    if (Radio::instance->isFilterEnabled()) {
        if (NRF_RADIO->EVENTS_DEVMATCH == 1) {
            NRF_RADIO->EVENTS_DEVMATCH = 0;
        }
        if (NRF_RADIO->EVENTS_DEVMISS == 1) {
            NRF_RADIO->EVENTS_DEVMISS = 0;
            Radio::instance->reload();
            return;
        }
        else {
            NRF_RADIO->EVENTS_DEVMISS = 0;
            NRF_RADIO->EVENTS_DEVMATCH = 0;
        }
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
		uint8_t sample = NRF_RADIO->EDSAMPLE;
		NRF_RADIO->EVENTS_EDEND = 0;
		NRF_TIMER4->TASKS_CAPTURE[5] = 1UL;
		uint32_t now = NRF_TIMER4->CC[5];

		Controller *controller = Radio::instance->getController();
		controller->onEnergyDetection(now, sample);
		NRF_RADIO->TASKS_EDSTART = 1;

	}

    /* Process RSSI measure event (disabled by default). */
    if (NRF_RADIO->EVENTS_RSSIEND) {
        /* Set current RX descriptor RSSI. */
        if (Radio::instance->rxDesc != NULL) {
            Radio::instance->rxDesc->rssi = NRF_RADIO->RSSISAMPLE;
        }
        /* Enable RSSI measurement again (triggered by shorts). */
        NRF_RADIO->EVENTS_RSSIEND = 0;
    }

    if (NRF_RADIO->EVENTS_TXREADY) {
        /* Check if we need to remove or not the DISABLED_TXEN short. */
        NRF_RADIO->SHORTS &= ~(RADIO_SHORTS_DISABLED_TXEN_Msk | RADIO_SHORTS_DISABLED_RXEN_Msk);
        NRF_RADIO->SHORTS |= RADIO_SHORTS_DISABLED_RXEN_Msk;
        NRF_RADIO->EVENTS_TXREADY = 0;
    }

    if (NRF_RADIO->EVENTS_CRCOK) {
        if (Radio::instance->rxDesc != NULL) {
            Radio::instance->rxDesc->crc.validity = VALID_CRC;
        }
        NRF_RADIO->EVENTS_CRCOK = 0;
    }

    if (NRF_RADIO->EVENTS_CRCERROR) {
        if (Radio::instance->rxDesc != NULL) {
            Radio::instance->rxDesc->crc.validity = INVALID_CRC;
        } else {
            bsp_board_led_off(0);
        }
        NRF_RADIO->EVENTS_CRCERROR = 0;
    }

    if (NRF_RADIO->EVENTS_END) {
        Controller *controller = NULL;

        /* Process the received payload. */
        if (Radio::instance->getMode() == MODE_NORMAL) {
            switch (Radio::instance->getState()) {
                case RX:
                    {
                        radio_desc_t *p_pkt = Radio::instance->rxDesc;
                        if (Radio::instance->isAutoTXafterRXenabled() && Radio::instance->hasTxDesc()) {

                            /* Update PACKETPTR as fast as possible to make it point to the TX buffer. */
                            Radio::instance->txDesc = Radio::instance->popTxDesc();
                            if (Radio::instance->txDesc != NULL) {
                                NRF_RADIO->PACKETPTR = (uint32_t)(Radio::instance->txDesc->payload);

                                /* Switch to RX state, let hardware send the current TX buffer. */
                                Radio::instance->setState(TX);
                                bsp_board_led_on(0);
                            }

                            /* Put the current RX buffer in our RX list. */
                            Radio::instance->rxDesc = Radio::instance->popFreeDesc();

                        } else {
                            /* Give radio another RX descriptor to write into. */
                            radio_desc_t *p_next = Radio::instance->popFreeDesc();
                            NRF_RADIO->PACKETPTR = (uint32_t)(p_next->payload);
                        
                            /* Save current RX descriptor (payload written by Radio). */
                            Radio::instance->rxDesc = p_next;
                        }

                        /* From now, if the radio starts receiving a new packet it will be
                         * written into the new descriptor's buffer.
                         */

                        /* Retrieve the current timestamp. */
                        NRF_TIMER4->TASKS_CAPTURE[5] = 1UL;
                        uint32_t now = NRF_TIMER4->CC[5];
                       
                        /* Retrieve the contoller. */
                        controller = Radio::instance->getController();

                        /* Save RSSI, CRC info and save packet into RX queue. */
                        if (Radio::instance->isRssiEnabled()) {
                            p_pkt->rssi = NRF_RADIO->RSSISAMPLE;
                        }

                        /* Now we have some time to process incoming packets. */
                        if (p_pkt != NULL) {
                            /* Process RX packet. */
                            uint8_t bufferSize = 0;
                            if (Radio::instance->getPhy() == DOT15D4_NATIVE)  {
                                bufferSize = 128;
                            }
                            else {
                                if (Radio::instance->getHeader().s0 != 0) {
                                    bufferSize += 1;
                                }
                                if (Radio::instance->getHeader().s1 != 0) {
                                    bufferSize += 1;
                                }
                                if (Radio::instance->getHeader().length != 0) {
                                    bufferSize += 1+(Radio::instance->getHeader().s0 == 0 ? p_pkt->payload[0] : p_pkt->payload[1]);
                                }
                                else {
                                    bufferSize += Radio::instance->getPayloadLength();
                                }
                            }

                            /* Process received frame (payload) and add to RX queue. */
                            if (bufferSize <= 2+Radio::instance->getPayloadLength()) {
                                p_pkt->size = bufferSize;
                                Phy p = Radio::instance->getPhy();

                                if (p == DOT15D4_NATIVE) {
                                    Radio::instance->currentTimestamp = now - ((bufferSize + 5) * 8 * 4) - 100;
                                }
                                else {
                                    Radio::instance->currentTimestamp = now - (Radio::instance->getPreamble().size+bufferSize)  * 4 * (p == BLE_2MBITS || p == ESB_2MBITS ? 1 : 2) - 100;
                                }

                            }
                        
                            /* Notify the controller we received a packet. */
                            if ((controller != NULL) /*&& (p_pkt->crc.validity == VALID_CRC)*/) {
                                /* Forward received frame to controller. */
                                controller->onReceive(Radio::instance->currentTimestamp, p_pkt->size, p_pkt->payload, p_pkt->crc, p_pkt->rssi);
                            }

                            /* Free descriptor. */
                            Radio::instance->pushFreeDesc(p_pkt);
                        }
                    }
                    break;

                /* TX buffer sent, we must switch PACKETPTR to rxDesc. */
                case TX:
                    {
                        /* RX after TX, set PACKETPTR to our current empty RX descriptor. */
                        NRF_RADIO->PACKETPTR = (uint32_t)(Radio::instance->rxDesc);
                        Radio::instance->setState(RX);

                        /* No need to start RX, shorts will handle it. */
                        Radio::instance->pushFreeDesc(Radio::instance->txDesc);
                        Radio::instance->txDesc = NULL;

                        bsp_board_led_off(0);
                    }
                    break;

                default:
                    /* Nothing to do. */
                    break;
            }
        } else if (Radio::instance->getMode() == MODE_JAMMER) {
            /* Retrieve the current timestamp. */
            NRF_TIMER4->TASKS_CAPTURE[5] = 1UL;
            uint32_t now = NRF_TIMER4->CC[5];

            if (Radio::instance->getState() == JAM_RX) {
                NRF_RADIO->PACKETPTR = (uint32_t)jamBuffer;
                NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_RXEN_Msk | RADIO_SHORTS_ADDRESS_BCSTART_Msk;
                Radio::instance->setState(JAM_TX);

            }
            else if (Radio::instance->getState() == JAM_TX) {
                NRF_RADIO->PACKETPTR = (uint32_t)Radio::instance->rxBuffer;
                uint32_t jammingInterval = Radio::instance->getJammingInterval();
                if (jammingInterval == 0) {
                        NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_TXEN_Msk | RADIO_SHORTS_ADDRESS_BCSTART_Msk;
                        Radio::instance->setState(JAM_RX);
                        controller->onJam(now);
                }
                else {
                    NRF_RADIO->SHORTS = 0;
                    nrf_delay_us(jammingInterval);
                    controller->onJam(now);
                    Radio::instance->reload();
                }
            }
        }
        
        /* Ack event. */
        NRF_RADIO->EVENTS_END = 0;
	}
}

/**
 * Descriptors and list management.
 **/

radio_desc_t *Radio::popFromList(radio_desc_head_t *p_list) {
    radio_desc_head_t *p_desc = NULL;

    /* Return NULL if list is empty. */
    if (p_list->p_next != p_list->p_prev) {
        /* Pick the first item. */ 
        p_desc = p_list->p_next; 
        p_list->p_next = p_desc->p_next;
        p_desc->p_next = NULL;
        p_desc->p_prev = NULL;
    }

    /* Return descriptor as a pointer to a radio_desc_t structure. */
    return (radio_desc_t *)p_desc;
}

radio_desc_t *Radio::popFreeDesc(void) {
    return popFromList(&this->descFreeList);
}

radio_desc_t *Radio::popTxDesc(void) {
    return popFromList(&this->descTxList);
}

radio_desc_t *Radio::popRxDesc(void) {
    return popFromList(&this->descRxList);
}

void Radio::pushIntoList(radio_desc_head_t *p_list, radio_desc_t *p_desc) {
    /* prev <- p_desc */
    p_desc->header.p_prev = p_list->p_prev;

    /* p_desc -> prev.next */
    p_desc->header.p_next = p_list->p_prev->p_next;

    /* prev.next -> p_desc */
    p_list->p_prev->p_next = &p_desc->header;

    /*  p_desc <- tail */
    p_list->p_prev = &p_desc->header;
}

void Radio::pushTxDesc(radio_desc_t *p_desc) {
    pushIntoList(&this->descTxList, p_desc);
}

void Radio::pushRxDesc(radio_desc_t *p_desc) {
    pushIntoList(&this->descRxList, p_desc);
}

void Radio::pushFreeDesc(radio_desc_t *p_desc) {
    /* Clear descriptor. */
    p_desc->state = DESC_FREE;
    p_desc->size = 0;
    p_desc->crc.validity = UNKNOWN_CRC;
    memset(p_desc->payload, 0, 256);
    pushIntoList(&this->descFreeList, p_desc);
}

bool Radio::isListEmpty(radio_desc_head_t *p_list) {
    return (p_list->p_next == p_list->p_prev);
}

size_t Radio::countList(radio_desc_head_t *p_list) {
    radio_desc_head_t *p = p_list;
    size_t count = 0;
    while (p->p_next != p_list) {
        count++;
        p = p->p_next;
    }
    return count;
}

size_t Radio::countTxDesc(void) {
    return countList(&this->descTxList);
}

size_t Radio::countRxDesc(void) {
    return countList(&this->descRxList);
}

bool Radio::hasTxDesc(void) {
    return !isListEmpty(&this->descTxList);
}

bool Radio::hasRxDesc(void) {
    return !isListEmpty(&this->descRxList);
}

bool Radio::hasFreeDesc(void) {
    return !isListEmpty(&this->descFreeList);
}

