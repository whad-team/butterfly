#include "genericcontroller.h"
#include "../core.h"

GenericController::GenericController(Radio* radio)  : Controller(radio) {
  uint8_t preamble[] = {0x1A, 0x2B, 0x3C, 0x4D};
  this->configure(preamble, 4, 32, GENERIC_PHY_1MBPS_ESB, GENERIC_ENDIANNESS_LITTLE);
}

void GenericController::start() {
  this->setHardwareConfiguration();
}

void GenericController::stop() {
  this->radio->disable();
}

GenericPhy GenericController::getPhy() {
  return this->phy;
}

void GenericController::onMatch(uint8_t *buffer, size_t size) {

}

bool GenericController::setTxPower(GenericTxPower txPower) {
  this->txPower = txPower;
  return true;
}

bool GenericController::setPreamble(uint8_t *preamble, size_t preambleSize) {
  if (preambleSize > 0 && preambleSize <= 4) {
    memcpy(this->preamble, preamble, preambleSize);
    this->preambleSize = preambleSize;
    return true;
  }
  return false;
}

bool GenericController::setPacketSize(size_t packetSize) {
  this->packetSize = packetSize;
  return true;
}

bool GenericController::setEndianness(GenericEndianness endianness) {
  this->endianness = endianness;
  return true;
}

bool GenericController::setPhy(GenericPhy phy) {
  this->phy = phy;
  return true;
}

bool GenericController::configure(uint8_t *preamble, size_t preambleSize, size_t packetSize, GenericPhy phy, GenericEndianness endianness) {
  if (preambleSize > 0 && preambleSize <= 4) {
    memcpy(this->preamble, preamble, preambleSize);
    this->preambleSize = preambleSize;
    this->packetSize = packetSize;
    this->phy = phy;
    this->endianness = endianness;
    //this->setHardwareConfiguration();
    return true;
  }
  return false;
}

int GenericController::getChannel() {
  return this->channel;
}

void GenericController::setChannel(int channel) {
  this->channel = channel;
  this->radio->fastFrequencyChange(channel,channel);
}

void GenericController::startAttack(GenericAttack attack) {
  this->attackStatus.attack = attack;
	if (attack == GENERIC_ATTACK_NONE) {
		this->setHardwareConfiguration();
		this->attackStatus.running = false;
		this->attackStatus.successful = false;
	}
	else if (attack == GENERIC_ATTACK_JAMMING) {
		this->setJammerConfiguration();
		this->attackStatus.running = true;
		this->attackStatus.successful = false;
	}
  else if (attack == GENERIC_ATTACK_ENERGY_DETECTION) {
    this->setEnergyDetectionConfiguration();
    this->attackStatus.samplesCount = 5000;
    this->attackStatus.running = true;
    this->attackStatus.successful = false;
  }
}

void GenericController::setJammerConfiguration() {
  this->radio->setPreamble(this->preamble,this->preambleSize);
	this->radio->setPrefixes();
  this->radio->setMode(MODE_JAMMER);
  this->radio->setFastRampUpTime(false);
  if (this->endianness == GENERIC_ENDIANNESS_BIG) {
    this->radio->setEndianness(BIG);
  }
  else {
    this->radio->setEndianness(LITTLE);
  }

  if (this->txPower == HIGH) {
    this->radio->setTxPower(POS8_DBM);
  }
  else if (this->txPower == MEDIUM) {
    this->radio->setTxPower(POS0_DBM);
  }
  else if (this->txPower == LOW) {
    this->radio->setTxPower(NEG8_DBM);
  }
  else {
    this->radio->setTxPower(POS0_DBM);
  }
  this->radio->enableRssi();

  if (this->phy == GENERIC_PHY_1MBPS_ESB) {
    this->radio->setPhy(ESB_1MBITS);
  }
  else if (this->phy == GENERIC_PHY_2MBPS_ESB) {
    this->radio->setPhy(ESB_2MBITS);
  }
  else if (this->phy == GENERIC_PHY_1MBPS_BLE) {
    this->radio->setPhy(BLE_1MBITS);
  }
  else if (this->phy == GENERIC_PHY_2MBPS_BLE) {
    this->radio->setPhy(BLE_2MBITS);
  }

  this->radio->setHeader(0,0,0);
  this->radio->setWhitening(NO_WHITENING);
  this->radio->setWhiteningDataIv(0);
	this->radio->disableJammingPatterns();
  this->radio->setCrc(NO_CRC);
  this->radio->setCrcSkipAddress(false);
  this->radio->setCrcSize(2);
  this->radio->setCrcInit(0xFFFF);
  this->radio->setCrcPoly(0x1021);
  this->radio->setPayloadLength(0);
  this->radio->setInterFrameSpacing(0);
  this->radio->setExpandPayloadLength(0);
  this->radio->setFrequency(this->channel);
  this->radio->reload();
}

void GenericController::setHardwareConfiguration() {
  this->radio->setPreamble(this->preamble,this->preambleSize);
	this->radio->setPrefixes();
  this->radio->setMode(MODE_NORMAL);
  this->radio->setFastRampUpTime(true);
  if (this->endianness == GENERIC_ENDIANNESS_BIG) {
    this->radio->setEndianness(BIG);
  }
  else {
    this->radio->setEndianness(LITTLE);
  }

  if (this->txPower == HIGH) {
    this->radio->setTxPower(POS8_DBM);
  }
  else if (this->txPower == MEDIUM) {
    this->radio->setTxPower(POS0_DBM);
  }
  else if (this->txPower == LOW) {
    this->radio->setTxPower(NEG8_DBM);
  }
  else {
    this->radio->setTxPower(POS0_DBM);
  }
  this->radio->enableRssi();

  if (this->phy == GENERIC_PHY_1MBPS_ESB) {
    this->radio->setPhy(ESB_1MBITS);
  }
  else if (this->phy == GENERIC_PHY_2MBPS_ESB) {
    this->radio->setPhy(ESB_2MBITS);
  }
  else if (this->phy == GENERIC_PHY_1MBPS_BLE) {
    this->radio->setPhy(BLE_1MBITS);
  }
  else if (this->phy == GENERIC_PHY_2MBPS_BLE) {
    this->radio->setPhy(BLE_2MBITS);
  }

  this->radio->setHeader(0,0,0);
  this->radio->setWhitening(NO_WHITENING);
  this->radio->setWhiteningDataIv(0);
	this->radio->disableJammingPatterns();
  this->radio->setCrc(NO_CRC);
  this->radio->setCrcSkipAddress(false);
  this->radio->setCrcSize(2);
  this->radio->setCrcInit(0xFFFF);
  this->radio->setCrcPoly(0x1021);
  this->radio->setPayloadLength(this->packetSize);
  this->radio->setInterFrameSpacing(0);
  this->radio->setExpandPayloadLength(this->packetSize);
  this->radio->setFrequency(this->channel);
  this->radio->reload();

}


void GenericController::setEnergyDetectionConfiguration() {
  this->radio->setPreamble(this->preamble,this->preambleSize);
	this->radio->setPrefixes();
  this->radio->setMode(MODE_ENERGY_DETECTION);
  this->radio->setFastRampUpTime(true);
  if (this->endianness == GENERIC_ENDIANNESS_BIG) {
    this->radio->setEndianness(BIG);
  }
  else {
    this->radio->setEndianness(LITTLE);
  }

  if (this->txPower == HIGH) {
    this->radio->setTxPower(POS8_DBM);
  }
  else if (this->txPower == MEDIUM) {
    this->radio->setTxPower(POS0_DBM);
  }
  else if (this->txPower == LOW) {
    this->radio->setTxPower(NEG8_DBM);
  }
  else {
    this->radio->setTxPower(POS0_DBM);
  }
  this->radio->enableRssi();

  if (this->phy == GENERIC_PHY_1MBPS_ESB) {
    this->radio->setPhy(ESB_1MBITS);
  }
  else if (this->phy == GENERIC_PHY_2MBPS_ESB) {
    this->radio->setPhy(ESB_2MBITS);
  }
  else if (this->phy == GENERIC_PHY_1MBPS_BLE) {
    this->radio->setPhy(BLE_1MBITS);
  }
  else if (this->phy == GENERIC_PHY_2MBPS_BLE) {
    this->radio->setPhy(BLE_2MBITS);
  }

  this->radio->setHeader(0,0,0);
  this->radio->setWhitening(NO_WHITENING);
  this->radio->setWhiteningDataIv(0);
	this->radio->disableJammingPatterns();
  this->radio->setCrc(NO_CRC);
  this->radio->setCrcSkipAddress(false);
  this->radio->setCrcSize(2);
  this->radio->setCrcInit(0xFFFF);
  this->radio->setCrcPoly(0x1021);
  this->radio->setPayloadLength(this->packetSize);
  this->radio->setInterFrameSpacing(0);
  this->radio->setExpandPayloadLength(this->packetSize);
  this->radio->setFrequency(this->channel);
  this->radio->reload();

}

void GenericController::sendJammingReport(uint32_t timestamp) {
  //Core::instance->pushMessageToQueue(new JammingReportNotification(timestamp));
}

void GenericController::sendEnergyDetectionReport(uint32_t timestamp, uint8_t sample) {
  //Core::instance->pushMessageToQueue(new EnergyDetectionReportNotification(timestamp, sample));
}

void GenericController::send(uint8_t* data, size_t size) {
  this->radio->send(data,size,this->channel, 0x00);
}

// Reception callback
void GenericController::onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
  GenericPacket *pkt = new GenericPacket(buffer,size,timestamp,0x00,channel,rssi,crcValue, this->preamble, this->preambleSize);
  this->addPacket(pkt);
  delete pkt;
}

void GenericController::onJam(uint32_t timestamp) {
  if (this->attackStatus.attack == GENERIC_ATTACK_JAMMING) {
    this->attackStatus.successful = true;
    this->sendJammingReport(timestamp);

  }
}

void GenericController::onEnergyDetection(uint32_t timestamp, uint8_t value) {
  if (this->attackStatus.attack == GENERIC_ATTACK_ENERGY_DETECTION) {
    if (this->attackStatus.samplesCount > 0) {
      this->sendEnergyDetectionReport(timestamp, value);
      this->attackStatus.samplesCount--;
    }
    else {
      this->attackStatus.successful = true;
      this->setHardwareConfiguration();
    }
  }
}
