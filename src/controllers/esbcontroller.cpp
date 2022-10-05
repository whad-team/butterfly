#include "esbcontroller.h"
#include "../core.h"

ESBController::ESBController(Radio *radio) : Controller(radio) {
  this->timerModule = Core::instance->getTimerModule();
  this->scanTimer = NULL;
  this->timeoutTimer = NULL;
  this->showAcknowledgements = false;
}


void ESBController::onMatch(uint8_t *buffer, size_t size) {

}

int ESBController::getChannel() {
  return this->channel;
}

void ESBController::sendPing() {
  uint8_t payload[6] = {0x04, 0x00, 0x0f,0x0f, 0x0f, 0x0f};
  this->radio->send(payload,6,this->channel, 0x00);
  bsp_board_led_invert(0);
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
	 this->timeoutTimer->update(500000, timestamp);
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
  if (this->attackStatus.attack == ESB_ATTACK_SNIFF_LOGITECH_PAIRING && !this->attackStatus.successful) {
    this->nextPairingChannel();
  }
  else {
    bsp_board_led_invert(0);
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
    this->setPromiscuousConfiguration();
  }
  else {
    if (this->channel == 0xFF) {
      this->channel = 0;
      this->setFollowMode(true);
      this->setAutofind(true);
    }
    this->setFollowConfiguration(this->filter.bytes);
  }
}


void ESBController::enableAcknowledgementsSniffing() {
  this->showAcknowledgements = true;
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
    bsp_board_led_invert(0);

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

      this->radio->send(buffer,payload_size+2,this->channel, 0x00);
      nrf_delay_us(100);

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
    return new ESBPacket(packet,totalSize, timestamp, 0x00, this->channel, rssi, crcValue);
}


void ESBController::sendAck(uint8_t pid) {

  uint8_t payload[2] = {0x00, (uint8_t)((pid & 0x3)<<1 | 1)};
  //this->radio->updateTXBuffer(payload, 2);
  nrf_delay_us(110);
  this->radio->send(payload,2,this->channel, 0x00);
  nrf_delay_us(10);
  //bsp_board_led_invert(0);
}

void ESBController::onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
  if (this->mode == ESB_PROMISCUOUS) {
    int channel = this->channel;
    int i=0;
    while (buffer[0] == 0xAA && i < 55*8) {
      shift_buffer(buffer, 1);
      i++;
    }
    ESBPacket *pkt = new ESBPacket(buffer,size,timestamp,0x00,channel,rssi,crcValue);

    if (pkt != NULL && pkt->checkCrc()) {
      ESBPacket *croppedPkt = new ESBPacket(buffer,pkt->getSize()+5+2+2,timestamp,0x00,channel,rssi,crcValue);
      this->addPacket(croppedPkt);
      delete croppedPkt;
      //nrf_delay_us(100);
    }
    delete pkt;

  }
  else if (this->mode == ESB_FOLLOW) {
    if (crcValue.validity == VALID_CRC) {

      if (this->autofind) {
        this->expandTimeout(timestamp);
      }
      if (this->syncing) {
        this->syncing = false;
        this->stopScanning();
      }
      if (this->sendAcknowledgements) {
        this->sendAck(buffer[1] >> 1);
      }
    }
      /*
      if (this->attackStatus.attack == ESB_ATTACK_SNIFF_LOGITECH_PAIRING) {
        if (size-2 == 0 && !this->attackStatus.successful) {
          this->setAutofind(false);
          this->startScanning();
          if (this->attackStatus.index < 24) {
            this->attackStatus.timestamps[this->attackStatus.index] = timestamp;
            this->attackStatus.channels[this->attackStatus.index] = channel;
            this->attackStatus.index++;
          }
          else {
            this->stopScanning();
            Core::instance->sendDebug((uint8_t*)this->attackStatus.timestamps, 24*4);
          }
        }
        else {
         this->stopScanning();
         this->activeScanning = false;
         this->attackStatus.successful = true;

       }
      }*/
      if (size > 2 || this->showAcknowledgements) {
        bool retransmission = false;
        if (size != this->lastReceivedPacket.size) {
          retransmission = false;
        }
        else {
          retransmission = true;
          for (int i=2;i<size;i++) {
            if (buffer[i] != this->lastReceivedPacket.buffer[i]) {
              retransmission = false;
              break;
            }
          }
        }
        if (!retransmission) {
          ESBPacket *pkt = this->buildPseudoPacketFromPayload(timestamp, size,buffer,crcValue, rssi);
          memcpy(this->lastReceivedPacket.buffer, buffer, size);
          this->lastReceivedPacket.size = size;
          this->addPacket(pkt);
          delete pkt;
        }
      }

      /*if (this->attackStatus.attack == ESB_ATTACK_SNIFF_LOGITECH_PAIRING && buffer[3] == 0x5F && buffer[4] == 0x01) {
        this->setFilter(buffer[5],buffer[6],buffer[7],buffer[8],buffer[9]);
        this->setFollowConfiguration(this->filter.bytes);
      }*/
      //free(pkt);

  }
}

void ESBController::onJam(uint32_t timestamp) {
		if (this->attackStatus.attack == ESB_ATTACK_JAMMING) {
			this->attackStatus.successful = true;
			this->sendJammingReport(timestamp);
		}
}

void ESBController::onEnergyDetection(uint32_t timestamp, uint8_t value) {}
