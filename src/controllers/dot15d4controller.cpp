#include "dot15d4controller.h"
#include "../core.h"

static uint8_t SYMBOL_TO_CHIP_MAPPING[16][4] = {
	{0x60, 0x77,0xae, 0x6c},
	{0x4e,0x07,0x7a,0xe6},
	{0x2c,0xe0,0x77,0xae},
	{0x26,0xce,0x07,0x7a},
	{0x6e,0x6c,0xe0,0x77},
	{0x3a,0xe6,0xce,0x07},
	{0x77,0xae,0x6c,0xe0},
	{0x07,0x7a,0xe6,0xce},
	{0x1f,0x88,0x51,0x93},
	{0x31,0xf8,0x85,0x19},
	{0x53,0x1f,0x88,0x51},
	{0x59,0x31,0xf8,0x85},
	{0x11,0x93,0x1f,0x88},
	{0x45,0x19,0x31,0xf8},
	{0x08,0x51,0x93,0x1f},
	{0x78,0x85,0x19,0x31},
};

int Dot15d4Controller::channelToFrequency(int channel) {
	return 5+5*(channel-11);
}

void Dot15d4Controller::onMatch(uint8_t *buffer, size_t size) {

}

Dot15d4Controller::Dot15d4Controller(Radio *radio) : Controller(radio) {
	this->channel = 11;
	this->autoAcknowledgement = false;
	this->controllerState = RECEIVING;
	this->shortAddress = 0xFFFF;
	this->extendedAddress = 0x1122334455667788;
}

void Dot15d4Controller::setShortAddress(uint16_t shortAddress) {
	this->shortAddress = shortAddress;
}

void Dot15d4Controller::setExtendedAddress(uint64_t extendedAddress) {
	this->extendedAddress = extendedAddress;
}

void Dot15d4Controller::setAutoAcknowledgement(bool enable) {
	this->autoAcknowledgement = enable;
}

int Dot15d4Controller::getChannel() {
    return this->channel;
}

void Dot15d4Controller::setChannel(int channel) {
    this->channel = channel;
    this->radio->fastFrequencyChange(Dot15d4Controller::channelToFrequency(channel),channel);
}

void Dot15d4Controller::send(uint8_t *data, size_t size, bool raw) {
			if (this->attackStatus.attack == DOT15D4_ATTACK_CORRECTION) {
				//Core::instance->getLinkModule()->sendSignalToSlave(STOP_SLAVE_RADIO);
				this->setNativeConfiguration();
				this->radio->send(data,size,Dot15d4Controller::channelToFrequency(this->channel), 0x00);
				nrf_delay_us((size+6)*8*1000/250);
				this->setWazabeeConfiguration();
				//Core::instance->getLinkModule()->sendSignalToSlave(START_SLAVE_RADIO);
			}
			else if (raw) {
				this->setRawConfiguration();
				this->radio->send(data,size,Dot15d4Controller::channelToFrequency(this->channel), 0x00);
				nrf_delay_us((size+6)*8*1000/250);
				this->setNativeConfiguration();

			}
			else {
				this->setNativeConfiguration();
				this->radio->send(data,size,Dot15d4Controller::channelToFrequency(this->channel), 0x00);
				nrf_delay_us((size+6)*8*1000/250);
			}
}

void Dot15d4Controller::setJammerConfiguration() {
	uint8_t preamble[] = {0xe0, 0x77, 0xae, 0x6c};
  this->radio->setPreamble(preamble,4);
	this->radio->setPrefixes();
  this->radio->setMode(MODE_JAMMER);
  this->radio->setFastRampUpTime(false);
  this->radio->setEndianness(BIG);
  this->radio->setTxPower(POS8_DBM);
  this->radio->disableRssi();
  this->radio->setPhy(BLE_2MBITS);
  this->radio->setHeader(0,0,0);
  this->radio->setWhitening(NO_WHITENING);
  this->radio->setWhiteningDataIv(0);
	this->radio->disableJammingPatterns();
  this->radio->setCrc(NO_CRC);
  this->radio->setCrcSkipAddress(true);
  this->radio->setCrcSize(3);
  this->radio->setCrcInit(0x000000);
  this->radio->setCrcPoly(0x000000);
  this->radio->setPayloadLength(0);
  this->radio->setInterFrameSpacing(0);
  this->radio->setExpandPayloadLength(0);
	this->radio->setJammingInterval(30000); // prevent jamming the same packet multiple times
  this->radio->setFrequency(Dot15d4Controller::channelToFrequency(this->channel));
  this->radio->reload();
}

void Dot15d4Controller::setWazabeeConfiguration() {
  this->radio->setPreamble(SYMBOL_TO_CHIP_MAPPING[0],4);
	this->radio->setPrefixes();
  this->radio->setMode(MODE_NORMAL);
  this->radio->setFastRampUpTime(true);
  this->radio->setEndianness(BIG);
  this->radio->setTxPower(POS8_DBM);
  this->radio->disableRssi();
  this->radio->setPhy(DOT15D4_WAZABEE);
  this->radio->setHeader(0,0,0);
  this->radio->setWhitening(NO_WHITENING);
  this->radio->setWhiteningDataIv(0);
	this->radio->disableJammingPatterns();
  this->radio->setCrc(NO_CRC);
  this->radio->setCrcSkipAddress(true);
  this->radio->setCrcSize(3);
  this->radio->setCrcInit(0x000000);
  this->radio->setCrcPoly(0x000000);
  this->radio->setPayloadLength(255);
  this->radio->setInterFrameSpacing(0);
  this->radio->setExpandPayloadLength(255);
  this->radio->setFrequency(Dot15d4Controller::channelToFrequency(this->channel));
  this->radio->reload();
}

void Dot15d4Controller::setNativeConfiguration() {
	this->radio->setPrefixes();
  this->radio->setMode(MODE_NORMAL);
  this->radio->setFastRampUpTime(true);
  this->radio->setTxPower(POS0_DBM);
  this->radio->enableRssi();
	this->radio->setEndianness(LITTLE);
  this->radio->setPhy(DOT15D4_NATIVE);
	this->radio->disableJammingPatterns();
  this->radio->setCrc(HARDWARE_CRC);
  this->radio->setCrcSkipAddress(false);
  this->radio->setCrcSize(2);
  this->radio->setCrcInit(0x0000);
  this->radio->setCrcPoly(0x11021);
  this->radio->setPayloadLength(255);
  //this->radio->setInterFrameSpacing(0);
  //this->radio->setExpandPayloadLength(255);
  this->radio->setFrequency(Dot15d4Controller::channelToFrequency(this->channel));
  this->radio->reload();
}


void Dot15d4Controller::setEnergyDetectionConfiguration() {
	this->radio->setPrefixes();
  this->radio->setMode(MODE_ENERGY_DETECTION);
  this->radio->setFastRampUpTime(true);
  this->radio->setTxPower(POS0_DBM);
  this->radio->disableRssi();
	this->radio->setEndianness(LITTLE);
  this->radio->setPhy(DOT15D4_NATIVE);
	this->radio->disableJammingPatterns();
  this->radio->setCrc(NO_CRC);
  this->radio->setCrcSkipAddress(false);
  this->radio->setCrcSize(2);
  this->radio->setCrcInit(0x0000);
  this->radio->setCrcPoly(0x11021);
  this->radio->setPayloadLength(0);
  this->radio->setInterFrameSpacing(0);
  this->radio->setExpandPayloadLength(0);
  this->radio->setFrequency(Dot15d4Controller::channelToFrequency(this->channel));
  this->radio->reload();

}

void Dot15d4Controller::setRawConfiguration() {
	this->radio->setPrefixes();
  this->radio->setMode(MODE_NORMAL);
  this->radio->setFastRampUpTime(true);
  this->radio->setTxPower(POS0_DBM);
  this->radio->disableRssi();
	this->radio->setEndianness(LITTLE);
  this->radio->setPhy(DOT15D4_NATIVE);
	this->radio->disableJammingPatterns();
  this->radio->setCrc(NO_CRC);
  this->radio->setCrcSkipAddress(false);
  this->radio->setCrcSize(2);
  this->radio->setCrcInit(0x0000);
  this->radio->setCrcPoly(0x11021);
  this->radio->setPayloadLength(255);
  //this->radio->setInterFrameSpacing(0);
  //this->radio->setExpandPayloadLength(255);
  this->radio->setFrequency(Dot15d4Controller::channelToFrequency(this->channel));
  this->radio->reload();
}

void Dot15d4Controller::start() {
	this->started = true;
	if (this->controllerState == RECEIVING) {
		this->setNativeConfiguration();
	}
	else if (this->controllerState == ENERGY_DETECTION_SCANNING) {
		this->setEnergyDetectionConfiguration();
	}
}
void Dot15d4Controller::enterReceptionMode() {
	this->controllerState = RECEIVING;
	if (this->started) this->setNativeConfiguration();
}

void Dot15d4Controller::enterEDScanMode() {
	this->controllerState = ENERGY_DETECTION_SCANNING;
	if (this->started) this->setEnergyDetectionConfiguration();
}

void Dot15d4Controller::stop() {
		this->started = false;
    this->radio->disable();
}


void Dot15d4Controller::startAttack(Dot15d4Attack attack) {
	this->attackStatus.attack = attack;
	if (attack == DOT15D4_ATTACK_NONE) {
		this->setNativeConfiguration();
		this->attackStatus.running = false;
		this->attackStatus.successful = false;
	}
	else if (attack == DOT15D4_ATTACK_JAMMING) {
		this->setJammerConfiguration();
		this->attackStatus.running = true;
		this->attackStatus.successful = false;
	}
	else if (attack == DOT15D4_ATTACK_CORRECTION) {
		this->setWazabeeConfiguration();
		this->attackStatus.running = true;
		this->attackStatus.correctorMode = true;
		this->attackStatus.successful = false;
	}
}


Dot15d4Packet* Dot15d4Controller::wazabeeDecoder(uint8_t *buffer, uint8_t size,uint32_t timestamp, CrcValue crcValue, uint8_t rssi) {
	Dot15d4Source source = RECEIVER;
	// buffer of Zigbee symbols
	uint8_t output_buffer[50];
	// index of the current Zigbee symbol
	int index = 0;
	// indicator of current 4 bits position (1 = 4 MSB, 0 = 4 LSB)
	int part = 0;
	// indicator of start frame delimiter
	int sfd = 0;
	// Hamming distance
	int hamming_dist = 0;
	// Thresold Hamming distance
	int hamming_thresold = 6;

	// Reset output buffer
	for (int i=0;i<50;i++) output_buffer[i] = 0;
	// Align the buffer with the SFD
	hamming_dist = 32;
	while (hamming_dist > hamming_thresold) {
		hamming_dist = hamming(buffer,SYMBOL_TO_CHIP_MAPPING[0]);
		if (hamming_dist > hamming_thresold) {
			shift_buffer(buffer,size);
		}
	}
	hamming_dist = 0;
	int minimum = 0;
	int minimum_sym = 0;
	int tolerance = 3;
	while (hamming_dist <= hamming_thresold) {
		int symbol = -1;
		// Compute the hamming distance for every zigbee symbol
		minimum = 32;
		minimum_sym = -1;
		for (int i=0;i<16;i++) {
			hamming_dist = hamming(buffer,SYMBOL_TO_CHIP_MAPPING[i]);
			if (hamming_dist <= minimum) {
				minimum_sym = i;
				minimum = hamming_dist;
			}
		}
		if (minimum_sym != -1) {
			if (minimum <= hamming_thresold) {
				symbol = minimum_sym;
				hamming_dist = minimum;
			}
			else if (tolerance > 0 || (index <= 2)) {
				tolerance--;
				symbol = minimum_sym;
				hamming_dist = 0;
			}
			else {
				symbol = -1;
				hamming_dist = 32;
			}
		}

		// If a zigbee symbol has been found ...
		if (symbol != -1) {
			// If it matches the SFD next symbol, start the frame decoding
			if (sfd == 0 && symbol != 0) {
				sfd = 1;
			}

			// If we are in the frame decoding state ...
			if (sfd == 1) {
				// Fill the output buffer with the selected symbol
				output_buffer[index] |= (symbol & 0x0F) << 4*part;

				// Select the next 4 bits free space in the output buffer
				part = part == 1 ? 0 : 1;
				if (part == 0) index++;
			}
			// Shift the buffer (31 bits shift)
			for (int i=0;i<32;i++) shift_buffer(buffer,size);
		}
	}
	CrcValue fcsValue;
	if (check_fcs_dot15d4(output_buffer,output_buffer[1]+2) && (output_buffer[1]+2 == index)) {
		fcsValue.validity = VALID_CRC;
		fcsValue.value = (bytewise_bit_swap(output_buffer[output_buffer[1]]) << 8) | bytewise_bit_swap(output_buffer[output_buffer[1]+1]);
	}
	else {
		fcsValue.validity = INVALID_CRC;
	}
	if (this->attackStatus.correctorMode && fcsValue.validity == INVALID_CRC) {
		// Enter correction mode
		if (output_buffer[0] != 0xA7) output_buffer[0] = 0xA7;
		// Correct size
		bool corrected = false;
		for (int i=0;i<index;i++) {
			output_buffer[1] = i;
			if (calculate_fcs_dot15d4(output_buffer,i+2) == ((output_buffer[i]) | (output_buffer[i+1]<<8))) {
				corrected = true;
				break;
			}
		}
		if (corrected) {
			fcsValue.validity = VALID_CRC;
			fcsValue.value = (bytewise_bit_swap(output_buffer[output_buffer[1]]) << 8) | bytewise_bit_swap(output_buffer[output_buffer[1]+1]);
			source = CORRECTOR;
		}
	}

	return new Dot15d4Packet(output_buffer+1,output_buffer[1]+1-2,timestamp,source,this->channel, rssi, fcsValue, rssi);
}

void Dot15d4Controller::sendJammingReport(uint32_t timestamp) {
	//Core::instance->pushMessageToQueue(new JammingReportNotification(timestamp));
}

void Dot15d4Controller::onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
	Dot15d4Packet* pkt = NULL;
	if (this->attackStatus.attack == DOT15D4_ATTACK_CORRECTION && this->attackStatus.running) {
		pkt = this->wazabeeDecoder(buffer,size,timestamp, crcValue, rssi);
	}
	else {
		uint8_t lqi = buffer[buffer[0]-1];

		pkt = new Dot15d4Packet(buffer,1+buffer[0]-2,timestamp,RECEIVER,this->channel,rssi,crcValue, (uint8_t)(lqi > 63 ? 255 : lqi*4));
	}

	if (pkt != NULL) {
		this->addPacket(pkt);

		if (pkt->extractAcknowledgmentRequest() && this->autoAcknowledgement) {
			Dot15d4AddressMode mode = pkt->extractDestinationAddressMode();
			if (
					(mode == ADDR_SHORT && pkt->extractShortDestinationAddress() != 0xFFFF && pkt->extractShortDestinationAddress() == this->shortAddress) ||
					(mode == ADDR_EXTENDED && pkt->extractExtendedDestinationAddress() == this->extendedAddress)
				) {

					nrf_delay_us(5*(4*1000/250));
					uint8_t ack_packet[4] = {5, 0x02, 0x00, pkt->extractSequenceNumber()};
					this->radio->send(ack_packet,4,Dot15d4Controller::channelToFrequency(this->channel), 0x00);
					nrf_delay_us((4+6)*8*1000/250);

			}
		}
		delete pkt;
	}
}

void Dot15d4Controller::onJam(uint32_t timestamp) {
		if (this->attackStatus.attack == DOT15D4_ATTACK_JAMMING) {
			this->attackStatus.successful = true;
			this->sendJammingReport(timestamp);
		}
}

void Dot15d4Controller::onEnergyDetection(uint32_t timestamp, uint8_t value) {
	Message* msg = Whad::buildDot15d4EnergyDetectionSampleMessage(value, timestamp);
	Core::instance->pushMessageToQueue(msg);
}
