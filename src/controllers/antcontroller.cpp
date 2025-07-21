#include "antcontroller.h"
#include "../core.h"

ANTController::ANTController(Radio *radio) : Controller(radio) {
    for (int i=0;i<MAX_CHANNELS;i++) {
        this->channels[i].enabled = false;
        this->channels[i].mode = UNKNOWN_MODE;
        this->channels[i].type = UNKNOWN_TYPE;
        this->channels[i].rfChannel = 57;
        this->channels[i].deviceNumber = 0;
        this->channels[i].deviceType = 0;
        this->channels[i].transmissionType = 0;
        this->channels[i].networkIndex = UNASSIGNED;
        this->channels[i].masterTimer = NULL;
        this->channels[i].incomingBurst = false;
        this->channels[i].outgoingBurst = false;

	}
    for (int i=0; i < MAX_NETWORKS; i++) {
        memset(this->networks[i].networkKey, 0, NETWORK_KEY_SIZE);
        this->networks[i].preamble = 0x0000;
    }
    this->rfChannel = 57;
	this->timerModule = Core::instance->getTimerModule();
	this->burstTimer = NULL;
}


void ANTController::sendChannelEvent(uint8_t channelIndex, whad::ant::ChannelEventCode event) {
    /* Craft an injection report notification. */
    whad::NanoPbMsg *notification = new whad::ant::ChannelEvent(channelIndex, event);

    /* Add notification to our message queue. */
	Core::instance->pushMessageToQueue(notification);

    /* Free notification wrapper. */
    delete notification;

}

bool ANTController::burstTimerCallback() {
	if (this->isBurstReady(this->activeChannel)) {
		TXPacket next = this->getPacketFromBurstQueue(this->activeChannel);
		this->radio->send(next.packet + 2, 14, this->rfChannel, this->rfChannel);
		return true;
	}
	else if (this->channels[this->activeChannel].incomingBurst) {
		this->radio->send(this->channels[this->activeChannel].latestAck.packet + 2, 14, this->rfChannel, this->rfChannel);
		return true;
	}
	return false;
}

void ANTController::startBurstTimer() {
    if (this->burstTimer == NULL) {
        this->burstTimer = TimerModule::instance->getTimer();
    }
    this->burstTimer->setMode(SINGLE_SHOT);
	this->burstTimer->setCallback((ControllerCallback)&ANTController::burstTimerCallback, this);
    this->burstTimer->update(1500);
    this->burstTimer->start();


}

void ANTController::releaseBurstTimer() {
	if (this->burstTimer != NULL) {
		this->burstTimer->stop();
		this->burstTimer->release();
		this->burstTimer = NULL;
	}
}
bool ANTController::addPacketToBurstQueue(uint8_t channelIndex, uint8_t *packet) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }
    TXPacket pkt;
    memcpy(pkt.packet, packet, 16);
    this->channels[channelIndex].burstQueue.push(pkt);
	if ((packet[6] & 0x20) == 0x20) {
		this->channels[channelIndex].outgoingBurst = true;
        this->sendChannelEvent(channelIndex, whad::ant::TransferTxStart);
	}
    return true;
}

bool ANTController::isBurstReady(uint8_t channelIndex) {
	return this->channels[channelIndex].outgoingBurst && !this->channels[channelIndex].burstQueue.empty();
}

TXPacket ANTController::getPacketFromBurstQueue(uint8_t channelIndex) {
    TXPacket pkt = this->channels[channelIndex].burstQueue.front();
    this->channels[channelIndex].burstQueue.pop();
    return pkt;
}


bool ANTController::addPacketToTransmitQueue(uint8_t channelIndex, uint8_t *packet) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }
    TXPacket pkt;
    memcpy(pkt.packet, packet, 16);
	if ((packet[6] & 0x80) != 0) {
		// This is an ack/burst
		if ((packet[6] & 0x20) == 0x20) {
			// This is an end packet
			if (this->channels[channelIndex].burstQueue.empty()) {
				// No outgoing burst pending, it's a normal ack, add it to the transmit queue 
				this->channels[channelIndex].transmitQueue.push(pkt);
			}
			else {
				// This is the end of a burst, add it to the burst queue
				this->addPacketToBurstQueue(channelIndex, packet);
			}
		}
		else {
			// This is part of a burst, add it to the burst queue
			this->addPacketToBurstQueue(channelIndex, packet);
		}
	}
	else {
		// This is a broadcast packet, add it to the transmit queue
		this->channels[channelIndex].transmitQueue.push(pkt);
	}
    return true;
}

bool ANTController::availablePacketsToTransmit(uint8_t channelIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }
    return !this->channels[channelIndex].transmitQueue.empty();
}

TXPacket ANTController::getPacketFromTransmitQueue(uint8_t channelIndex) {
    TXPacket pkt = this->channels[channelIndex].transmitQueue.front();
    this->channels[channelIndex].transmitQueue.pop();
    return pkt;
}

bool ANTController::channelManagementCallback(uint8_t channelIndex) {
    // Get the timestamp
    uint32_t now = this->timerModule->getTimestamp(); 

    if (channelIndex >= MAX_CHANNELS) return false;
    // Do nothing if channel is not enabled
    if (!this->channels[channelIndex].enabled) {
        return false;
    }
    // if so, reconfigure radio to match our configuration
    this->setActiveChannel(channelIndex);
    this->setHardwareConfiguration();


    if (this->channels[channelIndex].mode == SNIFFER) {} // do nothing
    else if (this->channels[channelIndex].mode == MASTER) {
        // then, transmit our main packet
		if (this->isBurstReady(channelIndex)) {
			// A burst is ready to transmit, go
            TXPacket burstPacket = this->getPacketFromBurstQueue(channelIndex);
			this->radio->send(burstPacket.packet + 2, 14, this->rfChannel, this->rfChannel);

		}
		else if (this->availablePacketsToTransmit(channelIndex)) {
            // first case: we have pending packets to transmit
            TXPacket packet = this->getPacketFromTransmitQueue(channelIndex);
			// This is a broadcast packet, update latestBroadcast and send it
			if ((packet.packet[6] & 0x80) == 0) {
				memcpy(this->channels[channelIndex].latestBroadcast.packet, packet.packet, 16);
				this->radio->send(packet.packet + 2, 14, this->rfChannel, this->rfChannel);
			}
			else {
				// This is an ack: transmit it and update the next broadcast
				memcpy(this->channels[channelIndex].latestAck.packet, packet.packet, 16);
				memcpy(this->channels[channelIndex].latestBroadcast.packet, packet.packet, 16);
				this->channels[channelIndex].latestBroadcast.packet[6] &= ~0x80;
				this->radio->send(packet.packet + 2, 14, this->rfChannel, this->rfChannel);
			}
		}
		else {
            // second case: we have no pending packets, transmit latest broadcast
            this->radio->send(this->channels[this->activeChannel].latestBroadcast.packet + 2, 14, this->rfChannel, this->rfChannel);
        }
    }
	
    this->channels[activeChannel].incomingBurst = false;
    if (this->channels[activeChannel].outgoingBurst && this->channels[activeChannel].burstQueue.empty()) {
        this->sendChannelEvent(activeChannel, whad::ant::TransferTxFailed);
        this->channels[activeChannel].outgoingBurst = false;
    }
    return true;
}

bool ANTController::channel0Callback() {
    return this->channelManagementCallback(0);
}

bool ANTController::channel1Callback() {
    return this->channelManagementCallback(1);
}

bool ANTController::channel2Callback() {
    return this->channelManagementCallback(2);
}
bool ANTController::channel3Callback() {
    return this->channelManagementCallback(3);
}

void ANTController::start() {
    for (int i=0; i < MAX_CHANNELS; i++) {
        if (this->channels[i].enabled) {
            this->startChannelTimer(i);
            this->setActiveChannel(i);
        }
        nrf_delay_us(50000);
    }
}


void ANTController::stop() {
    this->releaseTimers();
    this->radio->disable();
}

bool ANTController::startChannelTimer(uint8_t channelIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }

    if (this->channels[channelIndex].masterTimer == NULL) {
        this->channels[channelIndex].masterTimer = TimerModule::instance->getTimer();
    }
    this->channels[channelIndex].masterTimer->setMode(REPEATED);
    if (channelIndex == 0) {
        this->channels[channelIndex].masterTimer->setCallback((ControllerCallback)&ANTController::channel0Callback, this);
    }
    else if (channelIndex == 1) {
        this->channels[channelIndex].masterTimer->setCallback((ControllerCallback)&ANTController::channel1Callback, this);
    }
    else if (channelIndex == 2) {
        this->channels[channelIndex].masterTimer->setCallback((ControllerCallback)&ANTController::channel2Callback, this);
    }
    else if (channelIndex == 3) {
        this->channels[channelIndex].masterTimer->setCallback((ControllerCallback)&ANTController::channel3Callback, this);
    }
    this->channels[channelIndex].masterTimer->update((int)((this->channels[channelIndex].channelPeriod * 1000000.0)/32768.0));
    this->channels[channelIndex].masterTimer->start();
    return true;
}


void ANTController::releaseTimers() {
    for (int i=0 ; i < MAX_CHANNELS; i++) {
        if (this->channels[i].masterTimer != NULL) {
            this->channels[i].masterTimer->stop();
		    this->channels[i].masterTimer->release();
	    	this->channels[i].masterTimer = NULL;

        }
    }
}

int ANTController::getActiveRFChannel() {
    return this->rfChannel;
}

void ANTController::setActiveRFChannel(int rfChannel) {
  this->rfChannel = rfChannel;
  this->radio->fastFrequencyChange(rfChannel, rfChannel);
}

bool ANTController::setNextSync(uint8_t channelIndex, uint32_t nextSync) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }

    this->channels[channelIndex].nextSync = nextSync;
    return true;
}




uint32_t ANTController::getNextSync(uint8_t channelIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return 0;
    }
    
    return this->channels[channelIndex].nextSync;
}

bool ANTController::setChannelPeriod(uint8_t channelIndex, uint32_t channelPeriod) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }

    this->channels[channelIndex].channelPeriod = channelPeriod;
    return true;
}




uint32_t ANTController::getChannelPeriod(uint8_t channelIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return 0;
    }
    
    return this->channels[channelIndex].channelPeriod;
}

uint16_t ANTController::getDeviceNumber(uint8_t channelIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return 0;
    }
    
    return this->channels[channelIndex].deviceNumber;
}

bool ANTController::setDeviceNumber(uint8_t channelIndex, uint16_t deviceNumber) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }
    
    this->channels[channelIndex].deviceNumber = deviceNumber;
    return true;
}


uint8_t ANTController::getDeviceType(uint8_t channelIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return 0;
    }
    
    return this->channels[channelIndex].deviceType;
}

bool ANTController::setDeviceType(uint8_t channelIndex, uint8_t deviceType) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }
    
    this->channels[channelIndex].deviceType = deviceType;
    return true;
}

uint8_t ANTController::getTransmissionType(uint8_t channelIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return 0;
    }
    
    return this->channels[channelIndex].transmissionType;
}

bool ANTController::setTransmissionType(uint8_t channelIndex, uint8_t transmissionType) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }
    
    this->channels[channelIndex].transmissionType = transmissionType;
    return true;
}

ANTMode ANTController::getMode(uint8_t channelIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return UNKNOWN_MODE;
    }
    return this->channels[channelIndex].mode;
}


bool ANTController::setMode(uint8_t channelIndex, ANTMode mode) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }
    this->channels[channelIndex].mode = mode;
    return true;
}

ANTChannelType ANTController::getChannelType(uint8_t channelIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return UNKNOWN_TYPE;
    }
    return this->channels[channelIndex].type;
}


bool ANTController::setRFChannel(uint8_t channelIndex, int rfChannel) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }
    this->channels[channelIndex].rfChannel = rfChannel;
    return true;
}

int ANTController::getRFChannel(uint8_t channelIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return 0xFFFFFFFF;
    }
    return this->channels[channelIndex].rfChannel;
}


bool ANTController::setChannelType(uint8_t channelIndex, ANTChannelType type) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }
    this->channels[channelIndex].type = type;
    return true;
}

bool ANTController::assignNetwork(uint8_t channelIndex, uint8_t networkIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }
    if (networkIndex >= MAX_NETWORKS) {
        return false;
    }
    this->channels[channelIndex].networkIndex = networkIndex;
    return true;
}

bool ANTController::unassignNetwork(uint8_t channelIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }
    
    this->channels[channelIndex].networkIndex = UNASSIGNED;
    return true;
}

bool ANTController::openChannel(uint8_t channelIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }
    if (this->channels[channelIndex].enabled) {
        return false;
    }
    this->channels[channelIndex].enabled = true;
    return true;
}

bool ANTController::closeChannel(uint8_t channelIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }
    if (!this->channels[channelIndex].enabled) {
        return false;
    }
    this->channels[channelIndex].enabled = false;
    return true;
}

bool ANTController::isChannelOpen(uint8_t channelIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }
    return this->channels[channelIndex].enabled;
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


bool ANTController::setActiveChannel(uint8_t channelIndex) {
    if (channelIndex >= MAX_CHANNELS) {
        return false;
    }
    this->activeChannel = channelIndex;
    this->setActiveRFChannel(this->channels[channelIndex].rfChannel);
    return true;
}

uint16_t ANTController::getActivePreamble() {
    uint8_t activeNetwork = this->channels[this->activeChannel].networkIndex;
    return this->networks[activeNetwork].preamble;   
}

uint16_t ANTController::getActiveDeviceNumber() {
    return this->channels[this->activeChannel].deviceNumber;
}

uint8_t ANTController::getActiveDeviceType() {
    return this->channels[this->activeChannel].deviceType;
}

uint8_t ANTController::getActiveTransmissionType() {
    return this->channels[this->activeChannel].transmissionType;
}

bool ANTController::checkFilter(ANTPacket *packet) {
    uint16_t activeDeviceNumber = this->getActiveDeviceNumber();
    uint8_t activeDeviceType = this->getActiveDeviceType();
    uint8_t activeTransmissionType = this->getActiveTransmissionType();
    
    return (
        (activeDeviceNumber == 0 || activeDeviceNumber == packet->getDeviceNumber()) &&
        (activeDeviceType == 0 || activeDeviceType == packet->getDeviceType()) &&
        (activeTransmissionType == 0 || activeTransmissionType == packet->getTransmissionType())
    );
}

void ANTController::setHardwareConfiguration() {

    uint8_t activeNetwork = this->channels[this->activeChannel].networkIndex;

    uint8_t preamble[] = {
            (uint8_t)(this->networks[activeNetwork].preamble & 0xFF),
            (uint8_t)((this->networks[activeNetwork].preamble & 0xFF00) >> 8)
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
        this->getActivePreamble()
    );
    if (crcValue.validity == VALID_CRC && this->checkFilter(pkt)) {

        if (this->channels[this->activeChannel].mode == MASTER) {
			if (!pkt->isBroadcast()) {
				
				if (!this->channels[this->activeChannel].outgoingBurst) {
					memcpy(this->channels[this->activeChannel].latestAck.packet, this->channels[this->activeChannel].latestBroadcast.packet, 16);
					this->channels[this->activeChannel].latestAck.packet[6] = (
						(1 << 7) | // type ack/burst
						(1 << 6) | // ack=True
						(pkt->isEnd() << 5) | // end=True
						((1 - pkt->getCount()) << 4) | // count=1
						(0 << 3) | // slot=False
						2
					);
					this->channels[this->activeChannel].incomingBurst = true;
                    if (!pkt->isAck()) {
					    this->startBurstTimer();
                    }
				}
				else {
					if (this->channels[this->activeChannel].outgoingBurst) {
                        if (!pkt->isEnd()) {
                            this->channels[this->activeChannel].incomingBurst = false;
                            this->channels[this->activeChannel].outgoingBurst = true;
                            this->startBurstTimer();
                        }
                        else {
                            this->channels[this->activeChannel].incomingBurst = false;
                            this->channels[this->activeChannel].outgoingBurst = false;
                            this->sendChannelEvent(this->activeChannel, whad::ant::TransferTxCompleted);
                        }
					}
				}
			}
        }
        this->addPacket(pkt);
    }
    delete pkt;
}

void ANTController::onJam(uint32_t timestamp) {}

void ANTController::onEnergyDetection(uint32_t timestamp, uint8_t value) {}
