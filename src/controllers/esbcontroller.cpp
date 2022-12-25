#include "esbcontroller.h"
#include "../core.h"

ESBController::ESBController(Radio *radio) : Controller(radio) {
  this->timerModule = Core::instance->getTimerModule();
  this->scanTimer = NULL;
  this->timeoutTimer = NULL;
  this->pairingTimer = NULL;
  this->showAcknowledgements = false;
}


void ESBController::onMatch(uint8_t *buffer, size_t size) {

}

int ESBController::getChannel() {
  return this->channel;
}

void ESBController::sendPing() {
  uint8_t payload[3] = {0x01, 0x00, 0x0f};
  this->lastTransmissionAcknowledged = false;
  this->lastTransmissionTimestamp = TimerModule::instance->getTimestamp();
  this->radio->send(payload,3,this->channel, 0x00);
}



void ESBController::setChannel(int channel) {
  this->channel = channel;
  if (this->channel != 0xFF) {
	   this->radio->fastFrequencyChange(channel,channel);
  }

}

void ESBController::nextPairingChannel() {
  int channel = this->getChannel();
  if (channel == 5) {
    this->setChannel(32);
  }
  else if (channel == 32) {
    this->setChannel(62);
  }
  else if (channel == 62) {
    this->setChannel(35);
  }
  else if (channel == 35) {
    this->setChannel(65);
  }
  else if (channel == 65) {
    this->setChannel(14);
  }
  else if (channel == 14) {
    this->setChannel(41);
  }
  else if (channel == 41) {
    this->setChannel(71);
  }

  else if (channel == 71) {
    this->setChannel(17);
  }
  else if (channel == 17) {
      this->setChannel(44);
    }
  else if (channel == 44) {
    this->setChannel(74);
  }
  else if (channel == 74) {
    this->setChannel(5);
  }
  else {
    this->setChannel(5);
  }
}

bool ESBController::timeout() {
  this->syncing = true;
  this->startScanning();
  return true;

}

void ESBController::setAutofind(bool autofind) {
  if (autofind) {
    this->autofind = true;
    this->startTimeout();
  }
  else {
    this->autofind = false;
    this->stopTimeout();
  }
}

void ESBController::pairing() {
  if (!this->stopTransmitting) {
    if (!this->lastTransmissionAcknowledged) {
        this->nextPairingChannel();
    }
    //this->sendPing();
    this->sendPing();
    nrf_delay_us(80);
  }
}

void ESBController::startPairingSniffing() {
  this->stopTransmitting = false;
  if (this->pairingTimer == NULL) {
    this->pairingTimer = this->timerModule->getTimer();
  }
	this->pairingTimer->setMode(REPEATED);
	this->pairingTimer->setCallback((ControllerCallback)&ESBController::pairing, this);
	this->pairingTimer->update(10000);
	this->pairingTimer->start();

}

void ESBController::stopPairingSniffing() {
  if (this->pairingTimer != NULL) {
    this->pairingTimer->stop();
    this->pairingTimer->release();
    this->pairingTimer = NULL;
  }
}

void ESBController::startTimeout() {
  if (this->timeoutTimer == NULL) {
    this->timeoutTimer = this->timerModule->getTimer();
  }
	this->timeoutTimer->setMode(SINGLE_SHOT);
	this->timeoutTimer->setCallback((ControllerCallback)&ESBController::timeout, this);
	this->timeoutTimer->update(500000);
	this->timeoutTimer->start();
}

void ESBController::stopTimeout() {
  if (this->timeoutTimer != NULL) {
    this->timeoutTimer->stop();
    this->timeoutTimer->release();
    this->timeoutTimer = NULL;
  }
}

void ESBController::expandTimeout(int timestamp) {
  if (this->timeoutTimer != NULL) {
	 this->timeoutTimer->update(2000000, timestamp);
  }
}
void ESBController::setFilter(uint8_t a,uint8_t b,uint8_t c,uint8_t d,uint8_t e) {
	this->filter.bytes[0] = a;
	this->filter.bytes[1] = b;
	this->filter.bytes[2] = c;
	this->filter.bytes[3] = d;
	this->filter.bytes[4] = e;
}

void ESBController::setFollowMode(bool follow) {
	this->follow = follow;
  this->activeScanning = true;
  this->setFollowConfiguration(this->filter.bytes);
}


bool ESBController::goToNextChannel() {
  if ((this->attackStatus.attack == ESB_ATTACK_SNIFF_LOGITECH_PAIRING && !this->attackStatus.successful) || this->unifying) {
    this->nextPairingChannel();
  }
  else {
    this->setChannel((this->getChannel() + 1) % 100);
  }
  if (this->activeScanning && this->mode == ESB_FOLLOW) {
    this->sendPing();
  }
  return true;
}

void ESBController::sendJammingReport(uint32_t timestamp) {
	//Core::instance->pushMessageToQueue(new JammingReportNotification(timestamp));
}

void ESBController::startAttack(ESBAttack attack) {
	this->attackStatus.attack = attack;
	if (attack == ESB_ATTACK_NONE) {
		this->setPromiscuousConfiguration();
    this->stopScanning();
		this->attackStatus.running = false;
		this->attackStatus.successful = false;
	}
	else if (attack == ESB_ATTACK_SCANNING){
		this->setPromiscuousConfiguration();
    this->startScanning();
		this->attackStatus.running = true;
		this->attackStatus.successful = false;
	}
  else if (attack == ESB_ATTACK_SNIFF_LOGITECH_PAIRING) {
    this->activeScanning = true;
    this->attackStatus.index = 0;
		this->attackStatus.running = true;
		this->attackStatus.successful = false;
    this->setFilter(0xBB, 0x0A, 0xDC, 0xA5, 0x75);
    this->setFollowMode(true);
    this->setAutofind(true);
  }
  else if (attack == ESB_ATTACK_JAMMING) {
		this->attackStatus.running = true;
		this->attackStatus.successful = false;
    this->setJammerConfiguration();
  }
}

void ESBController::startScanning() {
  if (this->scanTimer == NULL) {
    this->scanTimer = this->timerModule->getTimer();
  }
	this->scanTimer->setMode(REPEATED);
	this->scanTimer->setCallback((ControllerCallback)&ESBController::goToNextChannel, this);
	this->scanTimer->update(10000);
	this->scanTimer->start();
}

void ESBController::stopScanning() {
  if (this->scanTimer != NULL) {
    this->scanTimer->stop();
    this->scanTimer->release();
    this->scanTimer = NULL;
  }
}

void ESBController::start() {
  this->stopScanning();

  if (
    this->filter.bytes[0] == 0xFF &&
    this->filter.bytes[1] == 0xFF &&
    this->filter.bytes[2] == 0xFF &&
    this->filter.bytes[3] == 0xFF &&
    this->filter.bytes[4] == 0xFF
  ) {

    if (this->channel == 0xFF) {
      this->channel = 0;
      this->startScanning();
    }
    else {
      this->stopScanning();
    }
    this->setPromiscuousConfiguration();
  }
  else {
    if (this->channel == 0xFF) {
      this->channel = 0;
      this->setFollowMode(true);
      this->setAutofind(true);
    }
    else {
      this->setFollowMode(false);
      this->setAutofind(false);
    }

    if (
      this->filter.bytes[0] == 0xBB &&
      this->filter.bytes[1] == 0x0A &&
      this->filter.bytes[2] == 0xDC &&
      this->filter.bytes[3] == 0xA5 &&
      this->filter.bytes[4] == 0x75
    ) {
      this->channel = 5;
      this->startPairingSniffing();
    }

    this->setFollowConfiguration(this->filter.bytes);
  }
}


void ESBController::enableAcknowledgementsSniffing() {
  this->showAcknowledgements = true;
  this->preparedAck.available = false;
}
void ESBController::disableAcknowledgementsSniffing() {
  this->showAcknowledgements = false;
}

void ESBController::enableAcknowledgementsTransmission() {
  this->sendAcknowledgements = true;

  /*this->radio->enableAutoTXafterRX();
  this->radio->reload();*/
}
void ESBController::disableAcknowledgementsTransmission() {
  this->sendAcknowledgements = false;
  /*this->radio->disableAutoTXafterRX();
  this->radio->reload();*/
}

void ESBController::stop() {
  this->stopScanning();
  this->disableAcknowledgementsTransmission();
  this->disableAcknowledgementsSniffing();
  this->setFollowMode(false);
  this->setAutofind(false);
  this->radio->disable();

}

void ESBController::setPromiscuousConfiguration() {
  this->mode = ESB_PROMISCUOUS;
  uint8_t preamble[] = {0xaa, 0xaa};
  this->radio->setPreamble(preamble,2);
  this->radio->setPrefixes(0xaa,0x1f,0x9f);
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

void ESBController::enableUnifying() {
  this->unifying = true;
}

void ESBController::disableUnifying() {
  this->unifying = false;
}

bool ESBController::isUnifyingEnabled() {
  return this->unifying;
}

void ESBController::setFollowConfiguration(uint8_t address[5]) {
  this->mode = ESB_FOLLOW;

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

void ESBController::send(uint8_t *data, size_t size) {
    if (this->sendAcknowledgements) {
      size_t payload_size = (data[6] >> 2);
      this->preparedAck.size = payload_size + 2;
      this->preparedAck.buffer[0] = data[6] >> 2;
      this->preparedAck.buffer[1] = ((data[6] & 0x03) << 1) | (data[7] >> 7);
      for (size_t i=0;i<payload_size;i++) {
        this->preparedAck.buffer[2+i] = (data[7+i] << 1) | (data[7+i+1] >> 7);
      }
      this->preparedAck.available = true;

    }
    else {
      if (this->mode == ESB_PROMISCUOUS) {
  			this->radio->send(data,size,this->channel, 0x00);
        nrf_delay_us(100);
      }
      else if (this->mode == ESB_FOLLOW) {
          size_t payload_size = (data[6] >> 2);
          uint8_t buffer[2+payload_size];
          buffer[0] = data[6] >> 2;
          buffer[1] = ((data[6] & 0x03) << 1) | (data[7] >> 7);
          for (size_t i=0;i<payload_size;i++) {
            buffer[2+i] = (data[7+i] << 1) | (data[7+i+1] >> 7);
          }
          //Core::instance->sendDebug(buffer,payload_size+2);
          this->lastTransmissionAcknowledged = false;
          this->lastTransmissionTimestamp = TimerModule::instance->getTimestamp();
          for (int i=0;i<6;i++) {
            this->radio->send(buffer,payload_size+2,this->channel, 0x00);
            nrf_delay_us(400);
            if (this->lastTransmissionAcknowledged) {
              break;
            }
          }

      }
    }
}

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
    return new ESBPacket(packet,totalSize, timestamp, 0x00, this->channel, rssi, crcValue, this->unifying);
}


void ESBController::sendAck(uint8_t pid) {
  if (this->preparedAck.available) {
    this->radio->send(this->preparedAck.buffer, this->preparedAck.size, this->channel, 0x00);
    nrf_delay_us(200);
    this->preparedAck.available = false;
  }
  else {
    uint8_t payload[2] = {0x00, (uint8_t)((pid & 0x3)<<1 | 1)};
    //this->radio->updateTXBuffer(payload, 2);
    nrf_delay_us(160);
    this->radio->send(payload,2,this->channel, 0x00);
    nrf_delay_us(50);
  }
}
void ESBController::onPromiscuousPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
    int channel = this->channel;
    int i=0;
    while (buffer[0] == 0xAA && i < 55*8) {
      shift_buffer(buffer, 1);
      i++;
    }
    ESBPacket *pkt = new ESBPacket(buffer,size,timestamp,0x00,channel,rssi,crcValue, this->unifying);

    if (pkt != NULL && pkt->checkCrc()) {
      ESBPacket *croppedPkt = new ESBPacket(buffer,pkt->getSize()+5+2+2,timestamp,0x00,channel,rssi,crcValue, this->unifying);
      this->addPacket(croppedPkt);
      delete croppedPkt;
    }
    delete pkt;
}

void ESBController::onFollowPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
    if (crcValue.validity == VALID_CRC) {
      if (this->autofind) {
        this->expandTimeout(timestamp);
      }

      if (size > 2) {
        this->onPTXPacketProcessing(timestamp, size, buffer, crcValue, rssi);
      }
      else {
        this->onPRXPacketProcessing(timestamp, size, buffer, crcValue, rssi);
      }


    }
}

void ESBController::onPRXPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
  bool ownAck = false;
  this->pairingTimer->update(1000);

  if ((timestamp - this->lastReceivedPacket.timestamp) < 350) {
    this->lastReceivedPacket.acked = true;
  }
  if ((timestamp - this->lastTransmissionTimestamp) < 350) {
    this->lastTransmissionAcknowledged = true;
    if (this->syncing) {
      this->syncing = false;
      this->stopScanning();
    }
    ownAck = true;
  }
  if (this->showAcknowledgements || (ownAck && this->filter.bytes[0] != 0xBB && this->filter.bytes[1] != 0x0A && this->filter.bytes[2] != 0xDC && this->filter.bytes[3] !=  0xA5 && this->filter.bytes[4] != 0x75)) {
    ESBPacket *pkt = this->buildPseudoPacketFromPayload(0, size,buffer,crcValue, rssi);
    this->addPacket(pkt);
    delete pkt;
  }

}

void ESBController::onPTXPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
  // If we act as a PRX, reply with an acknowledgement
  if (this->sendAcknowledgements) {
    this->sendAck(buffer[1] >> 1);
  }
  this->stopTransmitting = true;

  bool retransmission = false;
  // Check if the packet has been acknowledged to know if it is a retransmission
  if (!this->lastReceivedPacket.acked) {
    if (size != this->lastReceivedPacket.size) {
      retransmission = false;
    }
    else {
      // Check if similar payload has been received before
      retransmission = true;
      for (int i=2;i<size;i++) {
        if (buffer[i] != this->lastReceivedPacket.buffer[i]) {
          retransmission = false;
          break;
        }
      }
    }
  }

  // If the packet is not a retransmission, we update lastReceivedPacket and transmit pkt to the host
  if (!retransmission) {
    ESBPacket *pkt = this->buildPseudoPacketFromPayload(timestamp, size,buffer,crcValue, rssi);
    memcpy(this->lastReceivedPacket.buffer, buffer, size);
    this->lastReceivedPacket.timestamp = timestamp;
    this->lastReceivedPacket.size = size;
    this->addPacket(pkt);
    this->lastReceivedPacket.acked = false;
    delete pkt;
  }
  else {
    // It is a retransmission, but we have to update the timestamp of the last received packet structure to make sure we detect a lately ack
    this->lastReceivedPacket.timestamp = timestamp;
  }


  if (buffer[3] == 0x1f && buffer[4] == 0x01) {
    this->setFilter(buffer[5], buffer[6], buffer[7], buffer[8], buffer[9]);
    this->setFollowConfiguration(this->filter.bytes);
  }

}

void ESBController::onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
  if (this->mode == ESB_PROMISCUOUS) {
    this->onPromiscuousPacketProcessing(timestamp, size, buffer, crcValue, rssi);
  }
  else if (this->mode == ESB_FOLLOW) {
    this->onFollowPacketProcessing(timestamp, size, buffer, crcValue, rssi);
  }
}

void ESBController::onJam(uint32_t timestamp) {
		if (this->attackStatus.attack == ESB_ATTACK_JAMMING) {
			this->attackStatus.successful = true;
			this->sendJammingReport(timestamp);
		}
}

void ESBController::onEnergyDetection(uint32_t timestamp, uint8_t value) {}
