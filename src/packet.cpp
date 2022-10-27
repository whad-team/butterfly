#include "packet.h"

Packet::Packet(PacketType packetType,uint8_t *packetBuffer, size_t packetSize, uint32_t timestamp, uint8_t source, uint8_t channel, int8_t rssi, CrcValue crcValue) {
	this->packetType = packetType;
  this->payload = (uint8_t*)malloc(1+4+1+1+1+1+packetSize);
	this->payload[0] = (uint8_t)(this->packetType);

	this->timestamp = timestamp;
	this->payload[1] = (uint8_t)(timestamp & 0x000000FF);
	this->payload[2] = (uint8_t)((timestamp & 0x0000FF00) >> 8);
	this->payload[3] = (uint8_t)((timestamp & 0x00FF0000) >> 16);
	this->payload[4] = (uint8_t)((timestamp & 0xFF000000) >> 24);

	this->source = source;
	this->payload[5] = source;

	this->channel = channel;
	this->payload[6] = channel;

	this->rssi = rssi;
	this->payload[7] = rssi;

	this->crcValue = crcValue;
	this->payload[8] = (uint8_t)crcValue.validity;

	if (packetBuffer != NULL) {
		for (size_t i=0;i<packetSize;i++) {
			this->payload[9+i] = packetBuffer[i];
		}
	}
	this->packetPointer = &(this->payload[9]);
	this->packetSize = packetSize;
}

PacketType Packet::getPacketType() {
	return this->packetType;
}

uint8_t Packet::getChannel() {
	return this->channel;
}
int8_t Packet::getRssi() {
	return -1 * this->rssi;
}

uint8_t *Packet::getPacketBuffer() {
	return this->packetPointer;
}
size_t Packet::getPacketSize() {
	return this->packetSize;
}
uint32_t Packet::getTimestamp() {
	return this->timestamp;
}

uint8_t Packet::getSource() {
	return this->source;
}

void Packet::updateSource(uint8_t source) {
	this->source = source;
	this->payload[5] = source;
}
bool Packet::isCrcValid() {
	return this->crcValue.validity == VALID_CRC;
}

Packet::~Packet() {
	free(this->payload);
}

bool BLEPacket::needResponse(uint8_t *payload, size_t size) {
	if ((payload[0] & 3) == 1) { // LLID = 1: continue
		return false;
	}
	else if ((payload[0] & 3) == 2) { // LLID = 2: L2CAP
		if (size >= 6 && (payload[4] | (payload[5] << 8)) == 0x0004) { // cid = 4: attribute
			uint8_t opcode = payload[6];
			if (opcode == 0x2 ||
					opcode == 0x4 ||
					opcode == 0x6 ||
					opcode == 0x8 ||
					opcode == 0x8 ||
					opcode == 0xa ||
					opcode == 0xc ||
					opcode == 0xe ||
					opcode == 0x10 ||
					opcode == 0x12 ||
					opcode == 0x16 ||
					opcode == 0x18 ||
					opcode == 0x1b ||
					opcode == 0x1d) {
						return true;
					}
					else {
						return false;
					}
		}
	}
	return false;
}

void BLEPacket::forgeConnectionRequest(uint8_t **payload,size_t *size, uint8_t *initiator, bool initiatorRandom,  uint8_t *responder, bool responderRandom, uint32_t accessAddress,  uint32_t crcInit, uint8_t windowSize, uint16_t windowOffset, uint16_t hopInterval, uint16_t slaveLatency, uint16_t timeout, uint8_t *channelMap, uint8_t sca, uint8_t hopIncrement) {
	bool selectAlgorithm2 = false;

	*size=36;
	*payload = (uint8_t *)malloc(sizeof(uint8_t)*(*size));
	// Header

	// RxAdd | TxAdd | ChSel | PDU_type = 5
	(*payload)[0] = 0x05 | (((int)selectAlgorithm2) << 5) | (((int)initiatorRandom) << 6) | (((int)responderRandom) << 7);
	// Length
	(*payload)[1] = 34;

	memcpy(&((*payload)[2]), initiator, 6);
	//memcpy(&((*payload)[8]), responder, 6);
	(*payload)[8] = responder[5];
	(*payload)[9] = responder[4];
	(*payload)[10] = responder[3];
	(*payload)[11] = responder[2];
	(*payload)[12] = responder[1];
	(*payload)[13] = responder[0];
	(*payload)[14] = (accessAddress & 0xFF000000) >> 24;
	(*payload)[15] = (accessAddress & 0x00FF0000) >> 16;
	(*payload)[16] = (accessAddress & 0x0000FF00) >> 8;
	(*payload)[17] = (accessAddress & 0x000000FF);
	(*payload)[18] = (crcInit & 0x00FF0000) >> 16;
	(*payload)[19] = (crcInit & 0x0000FF00) >> 8;
	(*payload)[20] = (crcInit & 0x000000FF);
	(*payload)[21] = windowSize;
	(*payload)[22] = (windowOffset & 0xFF00) >> 8;
	(*payload)[23] = (windowOffset & 0x00FF);
	(*payload)[24] = (hopInterval & 0xFF00) >> 8;
	(*payload)[25] = (hopInterval & 0x00FF);
	(*payload)[26] = (slaveLatency & 0xFF00) >> 8;
	(*payload)[27] = (slaveLatency & 0x00FF);
	(*payload)[28] = (timeout & 0xFF00) >> 8;
	(*payload)[29] = (timeout & 0x00FF);
	(*payload)[30] = channelMap[0];
	(*payload)[31] = channelMap[1];
	(*payload)[32] = channelMap[2];
	(*payload)[33] = channelMap[3];
	(*payload)[34] = channelMap[4];
	(*payload)[35] = ((sca & 0x7) << 5) | (hopInterval & 0x1f);
}

void BLEPacket::forgeTerminateInd(uint8_t **payload,size_t *size, uint8_t code) {
	*size=4;
	*payload = (uint8_t *)malloc(sizeof(uint8_t)*(*size));
	(*payload)[0] = 0x03;
	(*payload)[1] = 0x02;
	(*payload)[2] = 0x02;
	(*payload)[3] = code;
}
void BLEPacket::forgeConnectionUpdateRequest(uint8_t **payload,size_t *size, uint8_t winSize, uint16_t winOffset, uint16_t interval, uint16_t latency, uint16_t timeout, uint16_t instant) {
	*size=14;
	*payload = (uint8_t *)malloc(sizeof(uint8_t)*(*size));
	(*payload)[0] = 0x03;
	(*payload)[1] = 0x0c;
	(*payload)[2] = 0x00;
	(*payload)[3] = winSize;
	(*payload)[4] = (uint8_t)(winOffset & 0x00FF);
	(*payload)[5] = (uint8_t)((winOffset & 0xFF00) >> 8);
	(*payload)[6] = (uint8_t)(interval & 0x00FF);
	(*payload)[7] = (uint8_t)((interval & 0xFF00) >> 8);
	(*payload)[8] = (uint8_t)(latency & 0x00FF);
	(*payload)[9] = (uint8_t)((latency & 0xFF00) >> 8);
	(*payload)[10] = (uint8_t)(timeout & 0x00FF);
	(*payload)[11] = (uint8_t)((timeout & 0xFF00) >> 8);
	(*payload)[12] = (uint8_t)(instant & 0x00FF);
	(*payload)[13] = (uint8_t)((instant & 0xFF00) >> 8);
}
void BLEPacket::forgeChannelMapRequest(uint8_t **payload,size_t *size,uint16_t instant, uint8_t *channelMap) {
	*size=10;
	*payload = (uint8_t *)malloc(sizeof(uint8_t)*(*size));
	(*payload)[0] = 0x03;
	(*payload)[1] = 0x08;
	(*payload)[2] = 0x01;
	(*payload)[3] = channelMap[0];
	(*payload)[4] = channelMap[1];
	(*payload)[5] = channelMap[2];
	(*payload)[6] = channelMap[3];
	(*payload)[7] = channelMap[4];
	(*payload)[8] = (uint8_t)(instant & 0x00FF);
	(*payload)[9] = (uint8_t)((instant & 0xFF00) >> 8);
}


BLEPacket::BLEPacket(uint32_t accessAddress,uint8_t *packetBuffer, size_t packetSize, uint32_t timestamp,  uint32_t timestampRelative, uint8_t source, uint8_t channel,int8_t rssi, CrcValue crcValue) : Packet(BLE_PACKET_TYPE, NULL, 4+4+packetSize+3, timestamp,source,channel,rssi,crcValue) {
	this->timestampRelative = timestampRelative;
	this->connectionHandle = 0;
	this->payload[9] = (uint8_t)(timestampRelative & 0x000000FF);
	this->payload[10] = (uint8_t)((timestampRelative & 0x0000FF00) >> 8);
	this->payload[11] = (uint8_t)((timestampRelative & 0x00FF0000) >> 16);
	this->payload[12] = (uint8_t)((timestampRelative & 0xFF000000) >> 24);
	this->packetPointer = &(this->payload[13]);
	this->accessAddress = accessAddress;
	this->packetPointer[0] = (accessAddress & 0x000000FF);
	this->packetPointer[1] = (accessAddress & 0x0000FF00) >> 8;
	this->packetPointer[2] = (accessAddress & 0x00FF0000) >> 16;
	this->packetPointer[3] = (accessAddress & 0xFF000000) >> 24;

	for (size_t i=0;i<packetSize;i++) {
		this->packetPointer[4+i] = packetBuffer[i];
	}
	this->packetPointer[4+packetSize] = bytewise_bit_swap((crcValue.value & 0xFF0000) >> 16);
	this->packetPointer[4+packetSize+1] = bytewise_bit_swap((crcValue.value & 0x00FF00) >> 8);
	this->packetPointer[4+packetSize+2] = bytewise_bit_swap(crcValue.value & 0x0000FF);
}


int BLEPacket::getConnectionHandle() {
	return this->connectionHandle;
}

void BLEPacket::setConnectionHandle(int connectionHandle) {
	this->connectionHandle = connectionHandle;
}

uint32_t BLEPacket::getCrc() {
		return bytewise_bit_swap(this->crcValue.value);
}
uint32_t BLEPacket::getRelativeTimestamp() {
	return this->timestampRelative;
}

bool BLEPacket::checkAdvertiserAddress(BLEAddress address) {
	if (this->isAdvertisement()) {
		if (address.bytes[0] == 0xFF && address.bytes[1] == 0xFF && address.bytes[2] == 0xFF && address.bytes[3] == 0xFF && address.bytes[4] == 0xFF && address.bytes[5] == 0xFF) {
			return true;
		}
		else if (this->extractAdvertisementType() == ADV_IND || this->extractAdvertisementType() == ADV_DIRECT_IND || this->extractAdvertisementType() == SCAN_RSP) {
			return (this->packetPointer[6] == address.bytes[5] &&
			this->packetPointer[7] == address.bytes[4] &&
			this->packetPointer[8] == address.bytes[3] &&
			this->packetPointer[9] == address.bytes[2] &&
			this->packetPointer[10] == address.bytes[1] &&
			this->packetPointer[11] == address.bytes[0] );
		}
		else if (this->extractAdvertisementType() == CONNECT_REQ || this->extractAdvertisementType() == SCAN_REQ) {
			return (this->packetPointer[12] == address.bytes[5] &&
			this->packetPointer[13] == address.bytes[4] &&
			this->packetPointer[14] == address.bytes[3] &&
			this->packetPointer[15] == address.bytes[2] &&
			this->packetPointer[16] == address.bytes[1] &&
			this->packetPointer[17] == address.bytes[0] );
		}
		else return false;
	}
	return false;
}

bool BLEPacket::isAdvertisement() {
	return ((this->packetPointer[0] == 0xd6) && (this->packetPointer[1] == 0xbe) && (this->packetPointer[2] == 0x89) && (this->packetPointer[3] == 0x8e));
}

bool BLEPacket::isReadRequest() {
	if (this->extractPayloadLength() >= 6) {
		return ((this->packetPointer[4] & 3) == 2) && ((this->packetPointer[8] | (this->packetPointer[9] << 8)) == 0x0004) && (this->packetPointer[10] == 0xa);
	}
	else {
		return false;
	}
}
bool BLEPacket::isPairingRequest() {
	if (this->extractPayloadLength() >= 6) {
		return ((this->packetPointer[4] & 3) == 2) && ((this->packetPointer[8] | (this->packetPointer[9] << 8)) == 0x0006) && (this->packetPointer[10] == 0x1);
	}
	else {
		return false;
	}
}
bool BLEPacket::isEncryptionRequest() {
	if (this->extractPayloadLength() >= 6) {
		return ((this->packetPointer[4] & 3) == 3) && (this->packetPointer[6] == 0x3);
	}
	else {
		return false;
	}
}

bool BLEPacket::isLinkLayerConnectionUpdateRequest() {
	return (!this->isAdvertisement() && this->packetSize > 7 && this->packetPointer[6] == 0x00);
}

bool BLEPacket::isLinkLayerTerminateInd() {
	return (!this->isAdvertisement() && this->packetSize > 7 && this->packetPointer[6] == 0x02);
}

bool BLEPacket::isLinkLayerChannelMapRequest() {
	return (!this->isAdvertisement() && this->packetSize > 7 && this->packetPointer[5] == 0x08 && this->packetPointer[6] == 0x01);
}

uint32_t BLEPacket::extractAccessAddress() {
	return *((uint32_t *)&this->packetPointer[18]);
}

uint32_t BLEPacket::extractCrcInit() {
	return (this->packetPointer[22] | (this->packetPointer[23] << 8) | (this->packetPointer[24] << 16));
}
uint16_t BLEPacket::extractLatency() {
	if (this->isAdvertisement() && this->extractAdvertisementType() == CONNECT_REQ) {
		return (this->packetPointer[30] | (this->packetPointer[31] << 8));
	}
	else if (this->isLinkLayerConnectionUpdateRequest()) {
		return (this->packetPointer[12] | (this->packetPointer[13] << 8));
	}
	else return 0x0000;
}
uint16_t BLEPacket::extractHopInterval() {
	if (this->isAdvertisement() && this->extractAdvertisementType() == CONNECT_REQ) {
		return (this->packetPointer[28] | (this->packetPointer[29] << 8));
	}
	else if (this->isLinkLayerConnectionUpdateRequest()) {
		return (this->packetPointer[10] | (this->packetPointer[11] << 8));
	}
	else return 0x0000;
}
uint8_t BLEPacket::extractHopIncrement() {
	if (this->isAdvertisement() && this->extractAdvertisementType() == CONNECT_REQ) {
		return this->packetPointer[39] & 0x1F;
	}
	else return 0x00;
}

int BLEPacket::extractSCA() {
	if (this->isAdvertisement() && this->extractAdvertisementType() == CONNECT_REQ) {
		uint8_t sca = (this->packetPointer[39] & 0xE0) >> 5;
		int ppm = 0;
		switch (sca) {
			case 0:
				ppm = 500; // 251
				break;
			case 1:
				ppm = 250; // 151
				break;
			case 2:
				ppm = 150; // 101
				break;
			case 3:
				ppm = 100; // 76
				break;
			case 4:
				ppm = 75; // 51
				break;
			case 5:
				ppm = 50; // 31
				break;
			case 6:
				ppm = 30; // 21
				break;
			case 7:
				ppm = 20; // 0
				break;
			default:
				ppm = 0;
		}
		return ppm;
	}
	else return 0;
}

uint8_t* BLEPacket::extractChannelMap() {
	if (this->isAdvertisement() && this->extractAdvertisementType() == CONNECT_REQ) {
		return &this->packetPointer[34];
	}
	else if (this->isLinkLayerChannelMapRequest()) {
		return &this->packetPointer[7];
	}
	else return NULL;
}

uint16_t BLEPacket::extractInstant() {
	if (this->isLinkLayerChannelMapRequest()) {
		return (this->packetPointer[12] | (this->packetPointer[13] << 8));
	}
	else if (this->isLinkLayerConnectionUpdateRequest()) {
		return (this->packetPointer[16] | (this->packetPointer[17] << 8));
	}
	else return 0x0000;
}

uint8_t BLEPacket::extractWindowSize() {
	if (this->isLinkLayerConnectionUpdateRequest()) {
		return this->packetPointer[7];
	}
	else return 0x00;
}

uint16_t BLEPacket::extractWindowOffset() {
	if (this->isLinkLayerConnectionUpdateRequest()) {
		return (this->packetPointer[8] | (this->packetPointer[9] << 8));
	}
	else return 0x0000;
}


uint8_t BLEPacket::extractPayloadLength() {
	return this->packetPointer[5];
}


uint8_t BLEPacket::extractSN() {
	if (!this->isAdvertisement()) {
		return (this->packetPointer[4] & 0x08) >> 3;
	}
	else return 0x00;
}

uint8_t BLEPacket::extractNESN() {
	if (!this->isAdvertisement()) {
		return (this->packetPointer[4] & 0x04) >> 2;
	}
	else return 0x00;
}

uint8_t BLEPacket::extractMD() {
	if (!this->isAdvertisement()) {
		return (this->packetPointer[4] & 0x10) >> 4;
	}
	else return 0x00;
}

uint32_t BLEPacket::getAccessAddress() {
	return (((this->packetPointer[3]) << 24) & 0xFF000000) | (((this->packetPointer[2]) << 16) & 0x00FF0000) | (((this->packetPointer[1]) << 8) & 0x0000FF00) | (((this->packetPointer[0]) & 0x000000FF));
}

BLEAdvertisementType BLEPacket::extractAdvertisementType() {
	BLEAdvertisementType type;
	if ((this->packetPointer[4] & 0x0F) == 0) type = ADV_IND;
	else if ((this->packetPointer[4] & 0x0F) == 1) type = ADV_DIRECT_IND;
	else if ((this->packetPointer[4] & 0x0F) == 2) type = ADV_NONCONN_IND;
	else if ((this->packetPointer[4] & 0x0F) == 3) type = SCAN_REQ;
	else if ((this->packetPointer[4] & 0x0F) == 4) type = SCAN_RSP;
	else if ((this->packetPointer[4] & 0x0F) == 5 && this->packetPointer[5] == 0x22) type = CONNECT_REQ;
	else if ((this->packetPointer[4] & 0x0F) == 6) type = ADV_SCAN_IND;
	else type = ADV_UNKNOWN;
	return type;
}

Dot15d4Packet::Dot15d4Packet(uint8_t *packetBuffer, size_t packetSize, uint32_t timestamp, uint8_t source, uint8_t channel, int8_t rssi, CrcValue crcValue, uint8_t lqi) : Packet(DOT15D4_PACKET_TYPE, packetBuffer, packetSize+2, timestamp, source, channel, rssi, crcValue) {
	for (size_t i=0;i<packetSize;i++) {
		this->packetPointer[i] = packetBuffer[i];
	}
	this->packetPointer[packetSize] = (uint8_t)bytewise_bit_swap((crcValue.value & 0xFF00) >> 8);
	this->packetPointer[packetSize+1] = (uint8_t)bytewise_bit_swap(crcValue.value & 0xFF);
	this->lqi = lqi;
}

uint8_t Dot15d4Packet::getLQI() {
	return this->lqi;
}

bool Dot15d4Packet::extractAcknowledgmentRequest() {
	return this->packetPointer[1] & (1 << 5);
}

Dot15d4AddressMode Dot15d4Packet::extractDestinationAddressMode() {
	uint8_t mode = ((this->packetPointer[2] & 0x0C) >> 2);
	if (mode == 2) {
		return ADDR_SHORT;
	}
	else if (mode == 3) {
		return ADDR_EXTENDED;
	}
	else {
		return ADDR_NONE;
	}
}

uint16_t Dot15d4Packet::extractShortDestinationAddress() {
	return (this->packetPointer[6] | (this->packetPointer[7] << 8));
}

uint64_t Dot15d4Packet::extractExtendedDestinationAddress() {
	return (
					(uint64_t)(this->packetPointer[6]) |
					((uint64_t)(this->packetPointer[7]) << 8) |
					((uint64_t)(this->packetPointer[8]) << 16) |
					((uint64_t)(this->packetPointer[9]) << 24) |
					((uint64_t)(this->packetPointer[10]) << 32) |
					((uint64_t)(this->packetPointer[11]) << 40) |
					((uint64_t)(this->packetPointer[12]) << 48) |
					((uint64_t)(this->packetPointer[13]) << 56)
	);
}

uint8_t Dot15d4Packet::extractSequenceNumber() {
	return this->packetPointer[3];
}

uint32_t Dot15d4Packet::getFCS() {
	return bytewise_bit_swap(this->crcValue.value);
}

ESBPacket::ESBPacket(uint8_t *packetBuffer, size_t packetSize, uint32_t timestamp, uint8_t source, uint8_t channel, int8_t rssi, CrcValue crcValue, bool unifying) : Packet(ESB_PACKET_TYPE, packetBuffer, packetSize, timestamp, source, channel, rssi, crcValue){
	this->unifying = unifying;
}
uint16_t ESBPacket::updateCrc(uint16_t crc, uint8_t byte, uint8_t bits) {
    crc = crc ^ (byte << 8);
    while(bits--)
        if((crc & 0x8000) == 0x8000) crc = (crc << 1) ^ 0x1021;
        else crc = crc << 1;
    crc = crc & 0xFFFF;
    return crc;
}
uint16_t ESBPacket::calculateCrc(uint8_t *data, uint8_t totalSize) {
	uint16_t crc = 0xFFFF;
	for (int i=0;i<totalSize;i++) {
		crc = updateCrc(crc, data[i], 8);
	}
	crc = updateCrc(crc, data[totalSize] & 0x80, 1);

  crc = (crc << 8) | (crc >> 8);
	return crc;
}

uint16_t ESBPacket::getCrc() {
	uint8_t totalSize = this->getSize() + 5 + 1;
	uint16_t crcGiven = (this->packetPointer[totalSize] << 9) | ((this->packetPointer[1+totalSize]) << 1);
	crcGiven = (crcGiven << 8) | (crcGiven >> 8);
	if(this->packetPointer[totalSize+2] & 0x80) crcGiven |= 0x100;
	return crcGiven;
}
uint8_t* ESBPacket::getAddress() {
	return this->packetPointer;
}

bool ESBPacket::checkCrc() {
	return this->getCrc() == calculateCrc(this->packetPointer,this->getSize()+5+1);
}

uint8_t ESBPacket::getSize() {
		return this->packetPointer[5] >> 2;
}

uint8_t ESBPacket::getPID() {
		return this->packetPointer[5] & 0x03;
}

bool ESBPacket::isUnifying() {
	return this->unifying;
}
MosartPacket::MosartPacket(uint8_t *packetBuffer, size_t packetSize, uint32_t timestamp, uint8_t source, uint8_t channel, int8_t rssi, CrcValue crcValue) : Packet(MOSART_PACKET_TYPE, packetBuffer, packetSize, timestamp, source, channel, rssi, crcValue) {}

uint8_t* MosartPacket::getAddress() {
	return this->packetPointer+2;
}

ANTPacket::ANTPacket(uint8_t *packetBuffer, size_t packetSize, uint32_t timestamp, uint8_t source, uint8_t channel, int8_t rssi, CrcValue crcValue, uint16_t preamble) : Packet(ANT_PACKET_TYPE, packetBuffer, packetSize+4, timestamp, source, channel, rssi, crcValue) {
	this->packetPointer[0] = (uint8_t)(preamble & 0xFF);
	this->packetPointer[1] = (uint8_t)((preamble & 0xFF00) >> 8);

	for (size_t i=0;i<packetSize;i++) {
		this->packetPointer[2+i] = packetBuffer[i];
	}
	this->packetPointer[2+packetSize] = (uint8_t)((crcValue.value & 0xFF00) >> 8);
	this->packetPointer[2+packetSize+1] = (uint8_t)(crcValue.value & 0xFF);

}


uint16_t ANTPacket::getDeviceNumber() {
	return ((this->packetPointer[3] << 8) | this->packetPointer[2]);
}

uint8_t ANTPacket::getDeviceType() {
	return this->packetPointer[4];
}

GenericPacket::GenericPacket(uint8_t *packetBuffer, size_t packetSize, uint32_t timestamp, uint8_t source, uint8_t channel, int8_t rssi, CrcValue crcValue, uint8_t *preamble, size_t preambleSize) : Packet(GENERIC_PACKET_TYPE, packetBuffer, packetSize+preambleSize, timestamp, source, channel, rssi, crcValue) {
	for (size_t i=0;i<preambleSize;i++) {
		this->packetPointer[i] = preamble[i];
	}

	for (size_t i=0;i<packetSize;i++) {
		this->packetPointer[preambleSize+i] = packetBuffer[i];
	}
}
