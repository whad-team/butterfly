#include "esbcontroller.h"
#include "../core.h"

ESBController::ESBController(Radio *radio) : Controller(radio) {
  this->timerModule = Core::instance->getTimerModule();
  this->applicativeLayer = RAW_ESB_APPLICATIVE_LAYER;
  this->channel = 0;
  this->showAcknowledgements = false;
  this->sendAcknowledgements = false;
}

// Filter configuration
void ESBController::setFilter(uint8_t a,uint8_t b,uint8_t c,uint8_t d,uint8_t e) {
	this->filter.bytes[0] = a;
	this->filter.bytes[1] = b;
	this->filter.bytes[2] = c;
	this->filter.bytes[3] = d;
	this->filter.bytes[4] = e;
}

void ESBController::start() {
  if (
    this->filter.bytes[0] == 0xFF &&
    this->filter.bytes[1] == 0xFF &&
    this->filter.bytes[2] == 0xFF &&
    this->filter.bytes[3] == 0xFF &&
    this->filter.bytes[4] == 0xFF
  ) {
    this->setPromiscuousConfiguration();
  }
  else {
    this->setSnifferConfiguration(this->filter.bytes);
  }
}

void ESBController::stop() {
  this->disableAcknowledgementsTransmission();
  this->disableAcknowledgementsSniffing();
  this->radio->disable();
}

// Channel control
uint8_t ESBController::getChannel() {
  return this->channel;
}

void ESBController::setChannel(uint8_t channel) {
  if (this->channel >= 0 && this->channel <= 100) {
    this->channel = channel;
    this->radio->fastFrequencyChange(channel,channel);
  }
}

// Acknowledgement control
void ESBController::enableAcknowledgementsSniffing() {
  this->showAcknowledgements = true;
}

void ESBController::disableAcknowledgementsSniffing() {
  this->showAcknowledgements = false;
}

void ESBController::enableAcknowledgementsTransmission() {
  this->sendAcknowledgements = true;
}
void ESBController::disableAcknowledgementsTransmission() {
  this->sendAcknowledgements = false;
}

// Radio Configuration functions
void ESBController::setPromiscuousConfiguration() {
  this->mode = ESB_PROMISCUOUS;
  uint8_t preamble[] = {0xaa, 0xaa};
  this->radio->setPreamble(preamble,2);
  this->radio->setPrefixes(0xa8,0x1f,0x9f,0xaf, 0xa9,0x00,0xFF);
  this->radio->setMode(MODE_NORMAL);
  this->radio->setFastRampUpTime(true);
  this->radio->setEndianness(BIG);
  this->radio->setTxPower(POS8_DBM);
  this->radio->enableRssi();
  this->radio->setPhy(ESB_2MBITS);
  this->radio->setHeader(0,0,0);
  this->radio->setWhitening(NO_WHITENING);
  this->radio->setWhiteningDataIv(0);
  this->radio->disableJammingPatterns();
  this->radio->setCrc(NO_CRC);
  this->radio->setCrcSkipAddress(true);
  this->radio->setCrcSize(3);
  this->radio->setCrcInit(0x000000);
  this->radio->setCrcPoly(0x000000);
  this->radio->setPayloadLength(250);
  this->radio->setInterFrameSpacing(0);
  this->radio->setExpandPayloadLength(250);
  this->radio->setFrequency(this->channel);
  this->radio->reload();
}

void ESBController::setSnifferConfiguration(uint8_t address[5]) {
  this->mode = ESB_SNIFF;
  this->radio->setPreamble(address,5);
  this->radio->setPrefixes();
  this->radio->setMode(MODE_NORMAL);
  this->radio->setFastRampUpTime(true);
  this->radio->setEndianness(BIG);
  this->radio->setTxPower(POS8_DBM);
  this->radio->enableRssi();
  this->radio->setPhy(ESB_2MBITS);
  this->radio->setHeader(0,6,3);
  this->radio->setWhitening(NO_WHITENING);
  this->radio->setWhiteningDataIv(0);
  this->radio->disableJammingPatterns();
  this->radio->setCrc(HARDWARE_CRC);
  this->radio->setCrcSkipAddress(false);
  this->radio->setCrcSize(2);
  this->radio->setCrcInit(0xFFFF);
  this->radio->setCrcPoly(0x11021);
  this->radio->setPayloadLength(40);
  this->radio->setInterFrameSpacing(0);
  this->radio->setExpandPayloadLength(0);
  this->radio->setFrequency(this->channel);
  this->radio->reload();
}


void ESBController::setJammerConfiguration() {
  this->mode = ESB_JAM;
  this->radio->setPreamble(this->filter.bytes,3);
  this->radio->setPrefixes();
  this->radio->setMode(MODE_JAMMER);
  this->radio->setFastRampUpTime(true);
  this->radio->setEndianness(BIG);
  this->radio->setTxPower(POS8_DBM);
  this->radio->disableRssi();
  this->radio->setPhy(ESB_2MBITS);
  this->radio->setHeader(0,0,0);
  this->radio->setWhitening(NO_WHITENING);
  this->radio->setWhiteningDataIv(0);
  this->radio->disableJammingPatterns();
  this->radio->setCrc(NO_CRC);
  this->radio->setCrcSkipAddress(false);
  this->radio->setCrcSize(2);
  this->radio->setCrcInit(0xFFFF);
  this->radio->setCrcPoly(0x11021);
  this->radio->setPayloadLength(0);
  this->radio->setInterFrameSpacing(0);
  this->radio->setExpandPayloadLength(0);
  this->radio->setFrequency(this->channel);
  this->radio->reload();
}

ESBApplicativeLayer ESBController::getApplicativeLayer() {
  return this->applicativeLayer;
}

void ESBController::setApplicativeLayer(ESBApplicativeLayer applicativeLayer) {
  this->applicativeLayer = applicativeLayer;
}

void ESBController::onPromiscuousPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
    // Extract any valid ESB packet from bitstream
    ESBPacket *pkt = NULL;
    ESBPacket *croppedPkt = NULL;
    for (uint8_t bitshift=0; bitshift<8; bitshift++) {
    shift_buffer(buffer, 60);
    for (uint8_t byteshift=1; byteshift < (60 - 32); byteshift++) {
      uint8_t* candidate = buffer+byteshift;

      uint8_t* check_ptr = (buffer+byteshift-1);
      if (check_ptr[0] == 0xAA || check_ptr[0] == 0x55) {
          pkt = new ESBPacket(candidate,size,timestamp,0x00,channel,rssi,crcValue, this->getApplicativeLayer());
          if (pkt->getSize() < 32 && pkt->checkCrc()) {
            croppedPkt = new ESBPacket(candidate,pkt->getSize()+5+2+2,timestamp,0x00,channel,rssi,crcValue, this->getApplicativeLayer());
            this->addPacket(croppedPkt);
            delete croppedPkt;
            break;
          }
          delete pkt;
        }
      }
      if (croppedPkt != NULL) break;
    }
}

// Build a pseudo raw packet from sniffed payload
ESBPacket* ESBController::buildPseudoPacketFromPayload(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
    uint8_t totalSize = 5+2+2+size-2+1;
    uint8_t packet[totalSize];
    memset(packet,0x00,totalSize);

    memcpy(packet,this->filter.bytes, 5);
    uint8_t pid = buffer[1] >> 1;
    uint8_t no_ack = buffer[1] & 0x01;
    packet[5] = (size-2) << 2 | (pid & 0x03);
    packet[6] = no_ack << 7;
    for (int i=0;i<size-2;i++) {
      packet[6+i] |= (buffer[i+2] >> 1);
      packet[6+i+1] = (buffer[i+2] & 0x01) << 7;
    }
    uint8_t crc[2] = {(uint8_t)((crcValue.value & 0xFF00) >> 8), (uint8_t)(crcValue.value & 0xFF)};
    packet[6+size-2] |= (crc[0] >> 1);
    packet[6+size-2+1] = (crc[0] << 7) | (crc[1] >> 1);
    packet[6+size-2+2] = (crc[1] << 7);
    return new ESBPacket(packet,totalSize, timestamp, 0x00, this->channel, rssi, crcValue, this->getApplicativeLayer());
}

bool ESBController::buildPayloadFromPacket(uint8_t *packet, size_t packet_size, uint8_t *buffer, size_t* payload_size) {
  if (packet_size >= 9) {
    *payload_size  = (packet[6] >> 2);
    buffer[0] = packet[6] >> 2;
    buffer[1] = ((packet[6] & 0x03) << 1) | (packet[7] >> 7);
    for (size_t i=0;i<*payload_size;i++) {
      buffer[2+i] = (packet[7+i] << 1) | (packet[7+i+1] >> 7);
    }
    return true;
  }
  return false;
}

void ESBController::sendAcknowledgement(uint8_t pid) {
  // if a prepared acknowledgement is available, send it
  if (this->preparedAck.available) {
    this->radio->send(this->preparedAck.buffer, this->preparedAck.size, this->channel, 0x00);
    nrf_delay_us(200);
    this->preparedAck.available = false;
  }
  // if no prepared ack, transmit an empty PDU
  else {
    uint8_t payload[2];
    payload[0] = 0x00;
    payload[1] = (uint8_t)((pid & 0x3)<<1 | 1);
    nrf_delay_us(160);
    this->radio->send(payload,2,this->channel, 0x00);
    nrf_delay_us(50);
  }
}

bool ESBController::send(uint8_t *data, size_t size, int retransmission_count) {
    if (this->sendAcknowledgements) {
      size_t payload_size = 0;
      this->buildPayloadFromPacket(data, size,this->preparedAck.buffer, &payload_size);
      this->preparedAck.size = payload_size + 2;
      this->preparedAck.available = true;
      return true;
    }
    else {
      if (this->mode == ESB_PROMISCUOUS) {
        for (int i=0;i<retransmission_count;i++) {
  			     this->radio->send(data,size,this->channel, 0x00);
             nrf_delay_us(700);
        }
        return true;
      }
      else if (this->mode == ESB_SNIFF) {
          size_t payload_size = 0;
          uint8_t transmission_buffer[255];
          this->buildPayloadFromPacket(data, size, transmission_buffer, &payload_size);

          this->lastTransmission.acknowledged = false;
          for (int i=0;i<retransmission_count;i++) {
            this->lastTransmission.timestamp = TimerModule::instance->getTimestamp();
            this->radio->send(transmission_buffer,payload_size+2,this->channel, 0x00);
            nrf_delay_us(750);
            if (this->lastTransmission.acknowledged) {
              return true;
            }
          }
          return false;
      }
    }
    return false;
}


void ESBController::onPRXPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {

  if (this->showAcknowledgements || this->lastTransmission.acknowledged) {
    ESBPacket *pkt = this->buildPseudoPacketFromPayload(timestamp, size,buffer,crcValue, rssi);
    this->addPacket(pkt);
    delete pkt;
  }
}

void ESBController::onPTXPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
  // If we act as a PRX, reply with an acknowledgement
  if (this->sendAcknowledgements) {
    this->sendAcknowledgement(buffer[1] >> 1);
  }
  // Check if received packet is a retransmission
  uint8_t pid = (buffer[1] >> 1) & 0x03;
  uint32_t crc = crcValue.value;

  if (pid == this->lastPacket.pid && crc == this->lastPacket.crc && timestamp < (this->lastPacket.timestamp + 100000)) {
    // If this condition matches, the packet is a retransmission, drops it
    return;
  }
  this->lastPacket.pid = pid;
  this->lastPacket.crc = crc;
  this->lastPacket.timestamp = timestamp;


  // Transmit the packet to Host
  ESBPacket *pkt = this->buildPseudoPacketFromPayload(timestamp, size,buffer,crcValue, rssi);
  this->addPacket(pkt);
  delete pkt;
}

void ESBController::onSniffPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
  if (crcValue.validity == VALID_CRC) {
    // Check if this packet acknowledges our last transmission
    if ((timestamp - this->lastTransmission.timestamp) < 750) {
      this->lastTransmission.acknowledged = true;
      this->onPRXPacketProcessing(timestamp, size, buffer, crcValue, rssi);
    }
    else if (size > 2) {
      this->onPTXPacketProcessing(timestamp, size, buffer, crcValue, rssi);
    }
    else {
      this->onPRXPacketProcessing(timestamp, size, buffer, crcValue, rssi);
    }
  }
}

// Reception callback
void ESBController::onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
  // If we are in promiscuous mode, forward to onPromiscuousPacketProcessing
  if (this->mode == ESB_PROMISCUOUS) {
    this->onPromiscuousPacketProcessing(timestamp, size, buffer, crcValue, rssi);
  }
  // if we are in sniffer mode, forward to onFollowPacketProcessing
  else if (this->mode == ESB_SNIFF) {
    this->onSniffPacketProcessing(timestamp, size, buffer, crcValue, rssi);
  }
}

// Pattern Matching callback
void ESBController::onMatch(uint8_t *buffer, size_t size) {
}

// Jamming callback
void ESBController::onJam(uint32_t timestamp) {}

// Energy Detection callback
void ESBController::onEnergyDetection(uint32_t timestamp, uint8_t value) {}
