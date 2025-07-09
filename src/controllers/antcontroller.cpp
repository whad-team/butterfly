#include "antcontroller.h"
#include "../core.h"

ANTController::ANTController(Radio *radio) : Controller(radio) {
    this->rfChannel = 57;
    this->preamble = PREAMBLE_ANT_FS;
    
    this->deviceNumber = 0;
    this->deviceType = 0;
    this->transmissionType = 0;
	this->timerModule = Core::instance->getTimerModule();
}
    
void ANTController::start() {

    this->setHardwareConfiguration();
}


void ANTController::stop() {
    this->radio->disable();
}


int ANTController::getRFChannel() {
    return this->rfChannel;
}

void ANTController::setRFChannel(int rfChannel) {
  this->rfChannel = rfChannel;
  this->radio->fastFrequencyChange(rfChannel, rfChannel);
}

uint16_t ANTController::getDeviceNumber() {
    return this->deviceNumber;
}

void ANTController::setDeviceNumber(uint16_t deviceNumber) {
  this->deviceNumber = deviceNumber; 
}


uint8_t ANTController::getDeviceType() {
    return this->deviceType;
}

void ANTController::setDeviceType(uint8_t deviceType) {
  this->deviceType = deviceType; 
}


uint8_t ANTController::getTransmissionType() {
    return this->transmissionType;
}

void ANTController::setTransmissionType(uint8_t transmissionType) {
  this->transmissionType = transmissionType; 
}

bool ANTController::setNetworkKey(uint8_t networkIndex, uint8_t *networkKey) {
    if (networkIndex >= MAX_NETWORKS) {
        return false;
    }
    if (ant_is_valid_netkey(networkKey)) {
        this->networks[networkIndex].preamble = ant_gen_netkey_preamble(networkKey);
        memcpy(this->networks[networkIndex].networkKey, networkKey, NETWORK_KEY_SIZE);
        return true;
    }
    return false;
}

bool ANTController::useNetwork(uint8_t networkIndex) {
    if (networkIndex >= MAX_NETWORKS) {
        return false;
    }
    this->selectedNetwork = networkIndex;
    return true;
}

void ANTController::setHardwareConfiguration() {
    uint8_t preamble[] = {
            (uint8_t)(this->networks[this->selectedNetwork].preamble & 0xFF),
            (uint8_t)((this->networks[this->selectedNetwork].preamble & 0xFF00) >> 8)
    };
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
    this->radio->setFrequency(this->rfChannel);
    this->radio->reload();
}


/* ******************** Events Callbacks ******************** */

void ANTController::onMatch(uint8_t *buffer, size_t size) {}


void ANTController::onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
    ANTPacket *pkt = new ANTPacket(
        buffer,
        size,
        timestamp,
        0x00,
        this->rfChannel,
        rssi,
        crcValue,
        this->networks[this->selectedNetwork].preamble
    );
    if (crcValue.validity == VALID_CRC/*&& this->checkFilter(pkt)*/) {
        this->addPacket(pkt);
    }
    delete pkt;
}

void ANTController::onJam(uint32_t timestamp) {}

void ANTController::onEnergyDetection(uint32_t timestamp, uint8_t value) {}
