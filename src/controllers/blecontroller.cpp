#include "blecontroller.h"
#include "../core.h"
#include <whad.h>

// BLE packet counter
static inline void set_ccm_counter(EncryptionData *e, uint32_t c) {
	e->counter[0] = (uint8_t)(c);
	e->counter[1] = (uint8_t)(c >> 8);
	e->counter[2] = (uint8_t)(c >> 16);
	e->counter[3] = (uint8_t)(c >> 24);
	e->counter[4] = 0;
}

/**
 * Divide round helper.
 **/
#define DIVIDE_ROUND(N, D) ((N) + (D)/2) / (D)

/**
 * Channel Selection Algorithm #2 PRNG primitives.
 **/

uint16_t mam(uint16_t a, uint16_t b)
{
  return (17 * a + b) % (0x10000);
}

uint16_t permute(uint16_t v)
{
  v = (((v & 0xaaaa) >> 1) | ((v & 0x5555) << 1));
  v = (((v & 0xcccc) >> 2) | ((v & 0x3333) << 2));
  return (((v & 0xf0f0) >> 4) | ((v & 0x0f0f) << 4));
}

uint16_t prne(uint16_t counter, uint16_t chanid) {
  uint16_t prne;

  prne = counter ^ chanid;
  prne = mam(permute(prne), chanid);
  prne = mam(permute(prne), chanid);
  prne = mam(permute(prne), chanid);
  return prne ^ chanid;
}

uint16_t unmapped_event_channel_selection(uint16_t counter, uint16_t chanid)
{
  return prne(counter, chanid)%37;
}

/**
 * BLE controller impl.
 **/

int BLEController::channelToFrequency(int channel) {
	int freq = 0;
	if (channel == 37) freq = 2;
	else if (channel == 38) freq = 26;
	else if (channel == 39) freq = 80;
	else if (channel < 11) freq = 2*(channel+2);
	else freq = 2*(channel+3);
	return freq;
}

BLEController::BLEController(Radio *radio) : Controller(radio) {
	this->timerModule = Core::instance->getTimerModule();
	this->sequenceModule = Core::instance->getSequenceModule();
	this->sequenceModule->reset();
	this->connectionTimer = NULL;
	this->injectionTimer = NULL;
	this->masterTimer = NULL;
	this->discoveryTimer = NULL;
	this->initTimer = NULL;
	this->timeoutTimer = NULL;
	this->scanningTimer = NULL;
	this->advertisingTimer = NULL;

	this->remappingTable = NULL;

	this->controllerState = IDLE;
	this->advertisementsTransmitIndicator = true;
	this->softwareFilterEnabled = false;

	this->masterQueueHead = 0;
	this->masterQueueTail = 0;

    /* Set legacy channel selection algorithm. */
    this->csa = CSA1;
    this->csa2_chan_id = 0;

	// Configure arbitrary BD address for now
	uint8_t oaddr[] = {0x11,	0x22, 0x33, 0x44,	0x55, 0x66};
	this->setOwnAddress(oaddr, 6);

}

bool BLEController::deleteSequence(uint8_t id) {
	for (int i=0;i<MAX_SEQUENCES;i++) {
		if (
			this->sequenceModule->sequences[i] != NULL &&
			this->sequenceModule->sequences[i]->getIdentifier() == id
		) {
			this->sequenceModule->deleteSequence(this->sequenceModule->sequences[i]);
			return true;
		}
	}
	return false;
}

bool BLEController::checkManualTriggers(uint8_t id) {
	for (int i=0;i<MAX_SEQUENCES;i++) {
		if (
				this->sequenceModule->sequences[i] != NULL &&
				this->sequenceModule->sequences[i]->isReady() &&
				!(this->sequenceModule->sequences[i]->isTriggered()) &&
				this->sequenceModule->sequences[i]->getTrigger()->getType() == MANUAL_TRIGGER &&
				this->sequenceModule->sequences[i]->getIdentifier() == id
			) {
				ManualTrigger* trigger = (ManualTrigger*)this->sequenceModule->sequences[i]->getTrigger();
				trigger->trigger();
				this->sendTriggeredReport(this->sequenceModule->sequences[i]->getIdentifier());
				return true;
		}
	}
	return false;
}

void BLEController::checkSequenceReceptionTriggers(uint8_t *packet, size_t size) {
	for (int i=0;i<MAX_SEQUENCES;i++) {
		if (
				this->sequenceModule->sequences[i] != NULL &&
				this->sequenceModule->sequences[i]->isReady() &&
				!(this->sequenceModule->sequences[i]->isTriggered()) &&
				this->sequenceModule->sequences[i]->getTrigger()->getType() == RECEPTION_TRIGGER
			) {
				ReceptionTrigger* trigger = (ReceptionTrigger*)this->sequenceModule->sequences[i]->getTrigger();
				if (trigger->evaluate(packet, size)) {
					trigger->trigger();
					this->sendTriggeredReport(this->sequenceModule->sequences[i]->getIdentifier());
				}
		}
	}
}
void BLEController::checkSequenceConnectionEventTriggers(uint16_t connectionEvent) {
	for (int i=0;i<MAX_SEQUENCES;i++) {
		if (
				this->sequenceModule->sequences[i] != NULL &&
				this->sequenceModule->sequences[i]->isReady() &&
				!(this->sequenceModule->sequences[i]->isTriggered()) &&
				this->sequenceModule->sequences[i]->getTrigger()->getType() == CONNECTION_EVENT_TRIGGER
			) {
				ConnectionEventTrigger* trigger = (ConnectionEventTrigger*)this->sequenceModule->sequences[i]->getTrigger();
				if (trigger->evaluate(connectionEvent)) {
					trigger->trigger();
					this->sendTriggeredReport(this->sequenceModule->sequences[i]->getIdentifier());
				}
		}
	}
}

void BLEController::executeSequences() {
	for (int i=0;i<MAX_SEQUENCES;i++) {
		if (
				this->sequenceModule->sequences[i] != NULL &&
				this->sequenceModule->sequences[i]->isReady() &&
				this->sequenceModule->sequences[i]->isTriggered()
			) {
				if (!(this->sequenceModule->sequences[i]->isTerminated())) {
					if (
						this->sequenceModule->sequences[i]->getDirection() == BLE_TO_SLAVE &&
						(this->controllerState == SIMULATING_MASTER || this->controllerState == PERFORMING_MITM)
					) {

						uint8_t *packet;
						size_t packetSize;
						bool updateHeader; // ignored for now
						this->sequenceModule->sequences[i]->processPacket(&packet, &packetSize, &updateHeader);
						this->setMasterPayload(packet, packetSize);
						free(packet);
					}
					else if (
						this->sequenceModule->sequences[i]->getDirection() == BLE_TO_MASTER &&
						(this->controllerState == SIMULATING_SLAVE || this->controllerState == PERFORMING_MITM)
					) {
						uint8_t *packet;
						size_t packetSize;
						bool updateHeader;
						this->sequenceModule->sequences[i]->processPacket(&packet, &packetSize, &updateHeader);
						this->setSlavePayload(packet, packetSize);
						free(packet);
					}
					else if (
						this->sequenceModule->sequences[i]->getDirection() == BLE_TO_MASTER &&
						this->controllerState == SNIFFING_CONNECTION
					) {
						uint8_t *packet;
						size_t packetSize;
						bool updateHeader;
						this->sequenceModule->sequences[i]->processPacket(&packet, &packetSize, &updateHeader);
						this->setAttackPayload(packet, packetSize);
						this->startAttack(BLE_ATTACK_FRAME_INJECTION_TO_MASTER);
						free(packet);
					}
					else if (
						this->sequenceModule->sequences[i]->getDirection() == BLE_TO_SLAVE &&
						this->controllerState == SNIFFING_CONNECTION
					) {
						uint8_t *packet;
						size_t packetSize;
						bool updateHeader;
						this->sequenceModule->sequences[i]->processPacket(&packet, &packetSize, &updateHeader);
						this->setAttackPayload(packet, packetSize);
						free(packet);
						this->startAttack(BLE_ATTACK_FRAME_INJECTION_TO_SLAVE);
					}

				}
				else {
					this->sequenceModule->deleteSequence(this->sequenceModule->sequences[i]);
				}

			}
	}
}
void BLEController::setOwnAddress(uint8_t *address, bool random) {
	this->own.bytes[0] = address[5];
	this->own.bytes[1] = address[4];
	this->own.bytes[2] = address[3];
	this->own.bytes[3] = address[2];
	this->own.bytes[4] = address[1];
	this->own.bytes[5] = address[0];
	this->ownRandom = random;
}

bool BLEController::configureEncryption(uint8_t *key, uint8_t *iv, uint32_t counter) {
	memcpy(this->encryptionData.key, key, 16);
	memcpy(this->encryptionData.iv,  iv,  8);
	this->encryptionData.direction = 0;
	set_ccm_counter(&this->encryptionData, counter);
	return true;
}

bool BLEController::startEncryption() {
	this->encTxCounter = 0;
	this->encRxCounter = 0;
	this->radio->enableEncryption((uint32_t)&(this->encryptionData));
	return true;
}

bool BLEController::stopEncryption() {
	bsp_board_led_off(0);
	bsp_board_led_off(1);
	this->radio->disableEncryption();
	return true;
}


void BLEController::onMatch(uint8_t *buffer, size_t size) {
	if ((buffer[0] & 3) == 2 && (buffer[1]) == 7) // && buffer[6] == 0xa)
		return;
}

void BLEController::setFollowMode(bool follow) {
	this->follow = follow;
}

void BLEController::setEmptyTransmitIndicator(bool emptyTransmitIndicator) {
	this->emptyTransmitIndicator = emptyTransmitIndicator;
}

void BLEController::setAdvertisementsTransmitIndicator(bool advertisementsTransmitIndicator) {
	this->advertisementsTransmitIndicator = advertisementsTransmitIndicator;
}


int BLEController::getChannel() {
	return this->channel;
}
void BLEController::setChannel(int channel) {
	if (channel == 37 || channel == 38 || channel == 39) this->lastAdvertisingChannel = channel;
	this->channel = channel;
	/*
	if (this->controllerState == REACTIVE_JAMMING) {
		uint8_t whitened_pattern[this->reactiveJammingPattern.size];
		for (size_t i=0;i<this->reactiveJammingPattern.size;i++) {
			whitened_pattern[this->reactiveJammingPattern.size - 1 - i] = dewhiten_byte_ble(this->reactiveJammingPattern.pattern[i], this->reactiveJammingPattern.position+i, this->channel);
		}
	}
	else {
		Core::instance->getLinkModule()->sendSignalToSlave(this->channel);
	}*/
	this->radio->fastFrequencyChange(BLEController::channelToFrequency(channel),channel);
}

void BLEController::setAnchorPoint(uint32_t timestamp) {
		this->lastAnchorPoint = timestamp;
		this->connectionTimer->update(this->hopInterval*1250UL-350, timestamp);
		if (!this->connectionTimer->isStarted()) this->connectionTimer->start();

		if (this->injectionTimer != NULL && this->attackStatus.running && !this->attackStatus.successful) {
			int window = (int)(((double)(this->masterSCA+this->slaveSCA) / 1000000.0) * (this->hopInterval * 1250)) + 16;
			this->injectionTimer->update(this->hopInterval*1250UL - window, timestamp);
			if (!this->injectionTimer->isStarted()) {
				this->injectionTimer->start();
			}
	}
	if (this->masterTimer != NULL && (this->controllerState == SYNCHRONIZING_MASTER || this->controllerState == SYNCHRONIZING_MITM || this->controllerState == PERFORMING_MITM) ) {
		this->masterTimer->update(5*1250UL, timestamp);
		if (!this->masterTimer->isStarted()) this->masterTimer->start();
	}

	if (this->masterTimer != NULL && this->controllerState == SIMULATING_MASTER) {
		this->masterTimer->update(this->hopInterval*1250UL, timestamp);
		if (!this->masterTimer->isStarted()) this->masterTimer->start();
	}

}

void BLEController::updateHopInterval(uint16_t hopInterval) {
	// This method update the hop interval in use
	this->hopInterval = hopInterval;
}

void BLEController::updateHopIncrement(uint8_t hopIncrement) {
	// This method update the hop increment in use
	this->hopIncrement = hopIncrement;
}

void BLEController::setMonitoredChannels(uint8_t *channels) {
	this->activeConnectionRecovery.monitoredChannelsCount = 0;
	for (int i=0; i<5; i++) {
		for (int j=0; j<8; j++) {
			if ((8*i + j) < 37) {
				if (channels[i] & (1<<j)) {
					this->activeConnectionRecovery.monitoredChannels[8*i + j] = true;
					this->activeConnectionRecovery.monitoredChannelsCount++;
				}
				else {
					this->activeConnectionRecovery.monitoredChannels[8*i + j] = false;
				}
			}
		}
	}
}

void BLEController::updateChannelsInUse(uint8_t* channelMap) {
	memcpy(this->channelMap, channelMap, 5);
	// This method extracts the channels in use from the channel map and build the remapping Table
	this->numUsedChannels = 0;
	for (int i=0; i<5; i++) {
		for (int j=0; j<8; j++) {
			if ((8*i + j) < 37) {
				if (channelMap[i] & (1<<j)) {
					this->channelsInUse[8*i + j] = true;
					this->numUsedChannels++;
				}
				else {
					this->channelsInUse[8*i + j] = false;
				}
			}
		}
	}
	if (this->remappingTable != NULL) free(this->remappingTable);
	this->remappingTable = (int*)malloc(sizeof(int)* this->numUsedChannels);
	int j=0;
	for (int i=0;i<37;i++) {;
		if (this->channelsInUse[i]) {
			this->remappingTable[j] = i;
			j++;
		}
	}
}

void BLEController::generateLegacyHoppingSequence(uint8_t hopIncrement, uint8_t *sequence) {
	uint8_t channel = 0;
	for (int i=0;i<37;i++) {
		if (this->channelsInUse[channel]) {
			sequence[i] = channel;
		}
		else {
			sequence[i] = this->remappingTable[channel % this->numUsedChannels];
		}
		channel = (channel + hopIncrement) % 37;
	}
}

void BLEController::generateAllHoppingSequences() {
		for (uint8_t hopIncrement=0;hopIncrement<12;hopIncrement++) {
			this->activeConnectionRecovery.hoppingSequences[hopIncrement] = (uint8_t*)malloc(sizeof(uint8_t) * 37);
			this->generateLegacyHoppingSequence(hopIncrement+5, this->activeConnectionRecovery.hoppingSequences[hopIncrement]);
		}
}
int BLEController::findChannelIndexInHoppingSequence(uint8_t hopIncrement, uint8_t channel, uint8_t start) {
		for (int i=0; i<37; i++) {
			if (this->activeConnectionRecovery.hoppingSequences[hopIncrement][(start + i) % 37] == channel) {
				return (start + i) % 37;
			}
		}
		return -1;
}

int BLEController::computeDistanceBetweenChannels(uint8_t hopIncrement, uint8_t firstChannel, uint8_t secondChannel) {
		int firstIndex = this->findChannelIndexInHoppingSequence(hopIncrement, firstChannel, 0);
		int secondIndex = this->findChannelIndexInHoppingSequence(hopIncrement, secondChannel, firstIndex);
		return (secondIndex > firstIndex ? (secondIndex - firstIndex) : (secondIndex - firstIndex + 37));
}

void BLEController::generateLegacyHoppingLookUpTables(uint8_t firstChannel, uint8_t secondChannel) {
		for (int i=0;i<12;i++) {
			this->activeConnectionRecovery.lookUpTables[i] = this->computeDistanceBetweenChannels(i, firstChannel, secondChannel);
		}
}

void BLEController::findUniqueChannels(uint8_t *firstChannel, uint8_t* secondChannel) {
	for (uint8_t channel=0; channel<37;channel++) {
		if (this->channelsInUse[channel]) {
			// Check if channel is unique across all possible sequences
			int j=0;
			int count=0;
			for (uint8_t hopIncrement=0; hopIncrement<12;hopIncrement++) {
				count = 0;
				for (uint8_t i=0; i<37; i++) {
					if (this->activeConnectionRecovery.hoppingSequences[hopIncrement][i] == channel) {
						count += 1;
						if (count > 1) break;
					}
				}
				if (count > 1) j = 1;
			}
			// First channel is unique, let's find another one
			if (j==0) {
					*firstChannel = channel;

					for (j=0;j<37;j++) {
						if (this->channelsInUse[j]) {
							this->generateLegacyHoppingLookUpTables(*firstChannel, j);
							bool duplicates = false;
							for (uint8_t i=0;i<11;i++) {
								for (uint8_t k=i+1;k<12;k++) {
									if (this->activeConnectionRecovery.lookUpTables[i] == this->activeConnectionRecovery.lookUpTables[k]) {
										duplicates = true;
									}
								}
							}
							if (!duplicates) {
								*secondChannel = j;
								for (uint8_t i=0;i<37;i++) {
										this->activeConnectionRecovery.reverseLookUpTables[i] = 0;
								}
								for (uint8_t i=0;i<12;i++) {
									this->activeConnectionRecovery.reverseLookUpTables[this->activeConnectionRecovery.lookUpTables[i]] = (i + 5);
								}
							}
						}
					}
			}
		}
	}
}


int BLEController::nextChannel() {
    switch (this->csa) {

        case CSA2:
            {
                // Compute next unmapped channel.
                this->unmappedChannel = unmapped_event_channel_selection(this->connectionEventCount, this->csa2_chan_id);
                this->lastUnmappedChannel = this->unmappedChannel;

                // Remap channel if required.
                if (this->channelsInUse[this->unmappedChannel]) {
                    return this->unmappedChannel;
                } else {
                    // Remapping following Vol 6, Part B, Section 4.5.8.3.4
                    uint32_t remappingIndex = ((uint32_t)this->numUsedChannels * (uint32_t)prne(this->connectionEventCount, this->csa2_chan_id))/0x10000;
                    return this->remappingTable[remappingIndex];
                }
            }
            break;

        default:
        case CSA1:
            {
                // This method calculates the next channel using Channel Selection Algorithm #1
                this->unmappedChannel = (this->lastUnmappedChannel + this->hopIncrement) % 37;
                this->lastUnmappedChannel = this->unmappedChannel;
                if (this->channelsInUse[this->unmappedChannel]) {
                    return this->unmappedChannel;
                }
                else {
                    int remappingIndex = this->unmappedChannel % this->numUsedChannels;
                    return this->remappingTable[remappingIndex];
                }
            }
            break;
    }
}

void BLEController::clearConnectionUpdate() {
	this->connectionUpdate.type = UPDATE_TYPE_NONE;
	this->connectionUpdate.instant = 0;
	this->connectionUpdate.hopInterval = 0;
	this->connectionUpdate.windowSize = 0;
	this->connectionUpdate.windowOffset = 0;
	this->connectionUpdate.channelMap[0] = 0;
	this->connectionUpdate.channelMap[1] = 0;
	this->connectionUpdate.channelMap[2] = 0;
	this->connectionUpdate.channelMap[3] = 0;
	this->connectionUpdate.channelMap[4] = 0;
}

void BLEController::prepareConnectionUpdate(uint16_t instant, uint16_t hopInterval, uint8_t windowSize, uint8_t windowOffset,uint16_t latency) {
	this->connectionUpdate.type = UPDATE_TYPE_CONNECTION_UPDATE_REQUEST;
	this->connectionUpdate.instant = instant;
	this->connectionUpdate.hopInterval = hopInterval;
	this->connectionUpdate.windowSize = windowSize;
	this->connectionUpdate.windowOffset = windowOffset;
	this->connectionUpdate.channelMap[0] = 0;
	this->connectionUpdate.channelMap[1] = 0;
	this->connectionUpdate.channelMap[2] = 0;
	this->connectionUpdate.channelMap[3] = 0;
	this->connectionUpdate.channelMap[4] = 0;
	this->latency = latency;
}

void BLEController::prepareConnectionUpdate(uint16_t instant, uint8_t *channelMap) {
	this->connectionUpdate.type = UPDATE_TYPE_CHANNEL_MAP_REQUEST;
	this->connectionUpdate.instant = instant;
	this->connectionUpdate.hopInterval = 0;
	this->connectionUpdate.windowSize = 0;
	this->connectionUpdate.windowOffset = 0;
	for (int i=0;i<5;i++) this->connectionUpdate.channelMap[i] = channelMap[i];

}

void BLEController::clearPhyUpdate(void) {
    this->phyUpdate.type = PHY_UPDATE_NONE;
    this->phyUpdate.c2p = 0;
    this->phyUpdate.p2c = 0;
    this->phyUpdate.instant = 0;
}

void BLEController::applyPhyUpdate(void) {
	if (this->phyUpdate.type != PHY_UPDATE_NONE && this->connectionEventCount == this->phyUpdate.instant) {
        /* Update PHY. */
        switch (this->phyUpdate.c2p) {
            // BLE 1M
            case 1:
                {
                    if (this->radio->getPhy() != BLE_1MBITS) {
                        /* Update PHY in radio. */
                        this->radio->setPhy(BLE_1MBITS);
                    }
                }
                break;

            // BLE 2M
            case 2:
                {
                    if (this->radio->getPhy() != BLE_2MBITS) {
                        /* Update PHY in radio. */
                        this->radio->setPhy(BLE_2MBITS);
                    }
                }
                break;

            default:
                break;
        }

        // Clear PHY update.
        this->clearPhyUpdate();
	}
}

void BLEController::preparePhyUpdate(uint16_t instant, uint8_t c2p, uint8_t p2c) {
    this->phyUpdate.type = PHY_UPDATE_BOTH;
    this->phyUpdate.c2p = c2p;
    this->phyUpdate.p2c = p2c;
    this->phyUpdate.instant = instant;
}

void BLEController::updateMasterSequenceNumbers(uint8_t sn, uint8_t nesn) {
	this->masterSequenceNumbers.sn = sn;
	this->masterSequenceNumbers.nesn = nesn;
}

void BLEController::updateSlaveSequenceNumbers(uint8_t sn, uint8_t nesn) {
	this->slaveSequenceNumbers.sn = sn;
	this->slaveSequenceNumbers.nesn = nesn;
}

void BLEController::updateSimulatedMasterSequenceNumbers(uint8_t sn, uint8_t nesn) {
	this->simulatedMasterSequenceNumbers.sn = sn;
	this->simulatedMasterSequenceNumbers.nesn = nesn;
}

void BLEController::updateSimulatedSlaveSequenceNumbers(uint8_t sn, uint8_t nesn) {
	this->simulatedSlaveSequenceNumbers.sn = sn;
	this->simulatedSlaveSequenceNumbers.nesn = nesn;
}
void BLEController::updateInjectedSequenceNumbers(uint8_t sn, uint8_t nesn) {
	this->attackStatus.injectedSequenceNumbers.sn = sn;
	this->attackStatus.injectedSequenceNumbers.nesn = nesn;
}

void BLEController::applyConnectionUpdate() {
	if (this->connectionUpdate.type != UPDATE_TYPE_NONE && this->connectionEventCount == this->connectionUpdate.instant) {
		if (this->connectionUpdate.type == UPDATE_TYPE_CONNECTION_UPDATE_REQUEST) {
			this->updateHopInterval(this->connectionUpdate.hopInterval);
			this->setAnchorPoint(this->lastAnchorPoint);
			this->sync = false;
		}
		else if (this->connectionUpdate.type == UPDATE_TYPE_CHANNEL_MAP_REQUEST) {
			this->updateChannelsInUse(this->connectionUpdate.channelMap);
		}
		this->clearConnectionUpdate();
	}
}
void BLEController::setAttackPayload(uint8_t *payload, size_t size) {
	if (size > sizeof(this->attackStatus.payload)) {
		size = sizeof(this->attackStatus.payload);
	}
	for (size_t i=0;i<size;i++) {
		this->attackStatus.payload[i] = payload[i];
	}
	this->attackStatus.size = size;
}

void BLEController::setSlavePayload(uint8_t *payload, size_t size) {
	if (size > sizeof(this->slavePayload.payload)) {
		size = sizeof(this->slavePayload.payload);
	}
	for (size_t i=0;i<size;i++) {
		this->slavePayload.payload[i] = payload[i];
	}
	this->slavePayload.size = size;
	this->slavePayload.transmitted = false;
	this->slavePayload.responseReceived = false;
	this->slavePayload.lastTransmitInstant = 0;
}

void BLEController::setMasterPayload(uint8_t *payload, size_t size) {
	if (size > sizeof(this->masterPayload.payload)) {
		size = sizeof(this->masterPayload.payload);
	}
	uint8_t next = (this->masterQueueTail + 1) % MASTER_PAYLOAD_QUEUE_SIZE;
	if (next != this->masterQueueHead) {
		BLEPayload *slot = &this->masterPayloadQueue[this->masterQueueTail];
		memcpy(slot->payload, payload, size);
		slot->size = size;
		this->masterQueueTail = next;
	}
}

void BLEController::loadNextMasterPayload() {
	if (this->masterQueueHead == this->masterQueueTail) {
		return; // queue empty
	}
	BLEPayload *slot = &this->masterPayloadQueue[this->masterQueueHead];
	memcpy(this->masterPayload.payload, slot->payload, slot->size);
	this->masterPayload.size = slot->size;
	this->masterPayload.transmitted = false;
	this->masterPayload.responseReceived = false;
	this->masterPayload.lastTransmitInstant = 0;
	this->masterQueueHead = (this->masterQueueHead + 1) % MASTER_PAYLOAD_QUEUE_SIZE;
}

bool BLEController::stopConnection() {
	// Stop the timers
	this->releaseTimers();


	// We are not synchronized anymore
	this->sync = false;
	// We are not waiting for an update
	this->clearConnectionUpdate();
	this->clearPhyUpdate();
    // If we were simulating slave, exit slave mode
	if (this->controllerState == SIMULATING_SLAVE || this->controllerState == PERFORMING_MITM) this->exitSlaveMode();
	this->masterPayload.transmitted = true;
	this->slavePayload.transmitted = true;
	// Reconfigure radio to receive advertisements
	this->setHardwareConfiguration(0x8e89bed6,0x555555);

	// Reset the channel
	this->setChannel(this->lastAdvertisingChannel);

	this->controllerState = SNIFFING_ADVERTISEMENTS;

	//Core::instance->getLedModule()->off(LED2);
	return false;
}

bool BLEController::connectionLost() {
	//We are sending a notification to Host
	bsp_board_led_off(0);

	this->sendConnectionReport(DISCONNECTED);
	this->sendConnectionReport(CONNECTION_LOST);
	return this->stopConnection();
}

bool BLEController::goToNextChannel() {

	if (this->controllerState != SNIFFING_ADVERTISEMENTS && this->controllerState != SCANNING) {
		// If we have not observed at least one packet, increase desyncCounter by one, otherwise resets this counter
		this->desyncCounter = (this->packetCount == 0 ? this->desyncCounter + 1 : 0);



		if (this->controllerState != SIMULATING_MASTER && this->desyncCounter == 1) {

            switch (this->connectionUpdate.type) {

                // if we observed a channel map request, let's consider it has been applied and our counter was wrong
                case UPDATE_TYPE_CHANNEL_MAP_REQUEST:
                    {
                        this->connectionEventCount = this->connectionUpdate.instant;
                        this->updateChannelsInUse(this->connectionUpdate.channelMap);
                        this->clearConnectionUpdate();
                        int channel = this->nextChannel();
                        this->setChannel(channel);
                    }
                    break;



                /**
                 * If we missed a PDU and we had a pending connection update request, chances
                 * are high that the new parameters have been set. Therefore, current event
                 * counter should be set to the instant of this pending connection update
                 * and the connection interval updated as soon as possible. We also need
                 * to take into account that we may have waited too long (at least a complete
                 * connection event) and that multiple connection events may have happen
                 * since (especially if the new interval value is very small).
                 **/
                case UPDATE_TYPE_CONNECTION_UPDATE_REQUEST:
                    {
                        if (this->controllerState == SNIFFING_CONNECTION && this->csa == CSA1) {
                            /* Compute old and new hop interval in microseconds. */
                            uint32_t now = this->connectionTimer->getLastTimestamp();
                            uint32_t oldInterval = this->hopInterval * 1250UL;
                            uint32_t newInterval = this->connectionUpdate.hopInterval * 1250UL;

                            /*
                             * In SNIFFING_CONNECTION, connection parameters may have been recovered from an
                             * existing connection, so our local connectionEventCount is not guaranteed to be
                             * aligned with the real LL instant.  A missed packet with a pending connection
                             * update is therefore handled from elapsed time and channel-sequence phase only.
                             *
                             * desyncCounter == 1 means the previous timer tick already moved us one channel
                             * after the last packet we received.  lastAnchorPoint now points to that missed
                             * old-interval anchor, so recover the last received anchor by going back one old
                             * interval.
                             */
                            uint32_t lastReceivedAnchor = this->lastAnchorPoint;
                            if (oldInterval > 0 && this->lastAnchorPoint >= oldInterval) {
                                lastReceivedAnchor -= oldInterval;
                            }

                            /*
                             * Target the next new-interval connection event after now.  If it is too close,
                             * skip events until the radio has enough time to retune before the expected PDU.
                             */
                            uint32_t elapsed = now - lastReceivedAnchor;
                            uint32_t targetEventsElapsed = (elapsed / newInterval) + 1;
                            uint32_t targetAnchor = lastReceivedAnchor + targetEventsElapsed * newInterval;
                            const uint32_t radioReconfigurationMargin = 150;

                            while ((uint32_t)(targetAnchor - now) <= radioReconfigurationMargin) {
                                targetEventsElapsed++;
                                targetAnchor += newInterval;
                            }

                            /*
                             * The local channel sequence is already one step after the last received packet.
                             * Advance only the missing delta so that the selected channel matches the chosen
                             * future event.
                             */
                            uint32_t additionalSteps = (targetEventsElapsed > 1 ? targetEventsElapsed - 1 : 0);
                            uint16_t updateInstant = this->connectionUpdate.instant;

                            /* Update connection hop interval with the connection update value. */
                            this->updateHopInterval(this->connectionUpdate.hopInterval);

                            /* Pending connection update has been processed, clear it. */
                            this->clearConnectionUpdate();

                            /*
                             * Skip missed channels to reach the next expected channel based on the current channel
                             * map and hop increment.
                             */

                            if (additionalSteps > 0) {
                                int channel = this->channel;

                                for (uint32_t i=0; i < additionalSteps; i++) {
                                    channel = this->nextChannel();
                                }
                                this->setChannel(channel);
                            }

                            /*
                             * Re-anchor the event counter to the LL instant and the number of events elapsed
                             * with the new interval.  CSA1 does not need an absolute counter for hopping, but
                             * keeping it close to the real timeline avoids destabilizing trigger/update logic.
                             */
                            this->connectionEventCount = updateInstant + targetEventsElapsed;
                            this->connectionTimer->update(this->hopInterval*1250UL, now);

                            /*
                             * We selected a future event to catch.  Schedule the following timer from that
                             * future anchor and wait for the radio to receive/re-anchor on the selected event.
                             */
                            this->lastPacketCount = this->packetCount;
                            this->packetCount = 0;
                            this->sync = false;
                            return true;
                        }
                    }
                    break;

                default:
                case UPDATE_TYPE_NONE:
                    break;
            }
        }

		// If the desyncCounter is greater than three, the connection is considered lost
		if (this->desyncCounter > 5 && !this->attackStatus.running) {
            return this->connectionLost();
		}
		// If we are still following the connection
		else {

            uint32_t now = this->connectionTimer->getLastTimestamp();
			this->connectionTimer->update(this->hopInterval*1250UL, now);

            // If connection timer is set as single shot, we are waiting for our first
            // packet. Timer should be changed to repeated and started again.
			if (this->connectionTimer->getMode() == SINGLE_SHOT) {
                this->connectionTimer->setMode(REPEATED);
                this->connectionTimer->start();
            }
            
            // Compute and update lastAnchorPoint.
			this->lastAnchorPoint = this->lastAnchorPoint + this->hopInterval*1250UL;
			this->checkAttackSuccess();
			this->lastPacketCount = this->packetCount;
			this->packetCount = 0;
			this->connectionEventCount++;

			// Check if we have a connection update and apply it if necessary
			this->applyConnectionUpdate();

            // Check if we have a PHY update and apply it if necessary.
            this->applyPhyUpdate();

			// Go to the next channel
			int channel = this->nextChannel();
			this->setChannel(channel);


			this->checkSequenceConnectionEventTriggers(this->connectionEventCount);
			this->executeSequences();

			this->executeAttack();
			//bsp_board_led_invert(0);

		}
		return this->desyncCounter <= 5;
	}
	else if (this->controllerState == SCANNING) {
		if (this->channel == 37) {
			this->setChannel(38);
		}
		else if (this->channel == 38) {
			this->setChannel(39);
		}
		else if (this->channel == 39) {
			this->setChannel(37);
		}
		else {
			this->setChannel(37);
		}
		return true;
	}
	return false;
}

void BLEController::start() {
	if (this->controllerState == CONNECTION_INITIATION) return;


	this->lastAdvertisingChannel = 37;
	this->follow = true;

	if (this->controllerState == ADVERTISING) {
		this->setHardwareConfiguration(0x8e89bed6,0x555555);
		this->newAdvertisingTransmission();
	}
	if (this->controllerState == SNIFFING_ADVERTISEMENTS) {

		this->setHardwareConfiguration(0x8e89bed6,0x555555);
	}
	else if (this->controllerState == SCANNING) {
		this->setChannel(37);
		this->setHardwareConfiguration(0x8e89bed6, 0x555555);

        /* Disable hardware address filtering. */
        this->setFilter(true, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);

        /* Configure radio if active scanning is required. */
		if (this->activeScanning) {
			this->radio->enableAutoTXafterRX();
			this->radio->setInterFrameSpacing(145);
			this->radio->setFastRampUpTime(false);
			this->radio->reload();
		}

		if (this->scanningTimer == NULL) {
			this->scanningTimer = this->timerModule->getTimer();
			this->scanningTimer->setMode(REPEATED);
			this->scanningTimer->setCallback((ControllerCallback)&BLEController::goToNextChannel, this);
			this->scanningTimer->update(500000);
			this->scanningTimer->start();
		}
	}
	else if (this->controllerState == SNIFFING_ACCESS_ADDRESS) {
		this->activeConnectionRecovery.accessAddressPreamble = 0xAA;
		this->setAccessAddressDiscoveryConfiguration(0xAA);
		this->resetAccessAddressesCandidates();

		if (this->discoveryTimer == NULL) {
			this->discoveryTimer = this->timerModule->getTimer();
			this->discoveryTimer->setMode(REPEATED);
			this->discoveryTimer->setCallback((ControllerCallback)&BLEController::hopToNextDataChannel, this);
			this->discoveryTimer->update(4000000);
			if (!this->discoveryTimer->isStarted()) this->discoveryTimer->start();
		}
	}
	else if (this->controllerState == RECOVERING_CRC_INIT) {
		this->setCrcRecoveryConfiguration(this->accessAddress);

		if (this->discoveryTimer == NULL) {
			this->discoveryTimer = this->timerModule->getTimer();
			this->discoveryTimer->setMode(REPEATED);
			this->discoveryTimer->setCallback((ControllerCallback)&BLEController::hopToNextDataChannel, this);
			this->discoveryTimer->update(4000000);
			if (!this->discoveryTimer->isStarted()) this->discoveryTimer->start();
		}
	}
	else if (this->controllerState == RECOVERING_CHANNEL_MAP) {
		this->activeConnectionRecovery.validPacketOccurences = 0;
		this->setHardwareConfiguration(this->accessAddress, this->crcInit);
		if (this->discoveryTimer == NULL) {
			this->discoveryTimer = this->timerModule->getTimer();
			this->discoveryTimer->setMode(REPEATED);
			this->discoveryTimer->setCallback((ControllerCallback)&BLEController::hopToNextDataChannel, this);
			this->discoveryTimer->update(4000000);
			if (!this->discoveryTimer->isStarted()) this->discoveryTimer->start();
		}
	}

	else if (this->controllerState == RECOVERING_HOP_INTERVAL) {
		this->generateAllHoppingSequences();
		this->activeConnectionRecovery.validPacketOccurences = 0;

		this->activeConnectionRecovery.firstChannel = -1;
		this->activeConnectionRecovery.secondChannel = -1;

		this->findUniqueChannels(&(this->activeConnectionRecovery.firstChannel), &(this->activeConnectionRecovery.secondChannel));
		this->setChannel(this->activeConnectionRecovery.firstChannel);
		this->setHardwareConfiguration(this->accessAddress, this->crcInit);
		if (this->discoveryTimer != NULL) {
			this->discoveryTimer->stop();
			this->discoveryTimer->release();
			this->discoveryTimer = NULL;
		}
	}
	else if (this->controllerState == RECOVERING_HOP_INCREMENT) {

		//if (this->activeConnectionRecovery.firstChannel == -1 || this->activeConnectionRecovery.secondChannel == -1) {
			this->generateAllHoppingSequences();
			this->activeConnectionRecovery.validPacketOccurences = 0;
			this->findUniqueChannels(&(this->activeConnectionRecovery.firstChannel), &(this->activeConnectionRecovery.secondChannel));
			this->setChannel(this->activeConnectionRecovery.firstChannel);
			this->setHardwareConfiguration(this->accessAddress, this->crcInit);
			if (this->discoveryTimer != NULL) {
				this->discoveryTimer->stop();
				this->discoveryTimer->release();
				this->discoveryTimer = NULL;
			}
		}
		else if (this->controllerState == ATTACH_TO_EXISTING_CONNECTION) {
			if (this->discoveryTimer != NULL) {
				this->discoveryTimer->stop();
				this->discoveryTimer->release();
				this->discoveryTimer = NULL;
			}

            /* Pick a unique channel based on current channel map. */
            uint8_t chan1=37, chan2=37;
            this->findUniqueChannels(&chan1, &chan2);

            /* Set channel. */
            this->channel = chan1;
            this->setChannel(this->channel);

            /* Start following. */
            this->followConnection(
                CSA1,
                this->hopInterval,
                this->hopIncrement,
                this->channelMap,
                this->accessAddress,
                this->crcInit,
                20,
                6,
                0xffff, /* winOffset set to 0xffff, will wait a full hop sequence. */
                0xff
            );
	}
}

/**
 * Reset Access Address detection structure.
 **/

void BLEController::resetAccessAddressesCandidates() {
	this->activeConnectionRecovery.accessAddressCandidates.count = 0;
	for (int i=0;i<MAX_AA_CANDIDATES;i++) {
		this->activeConnectionRecovery.accessAddressCandidates.aa[i] = 0x00000000;
		this->activeConnectionRecovery.accessAddressCandidates.seen[i] = 0x00000000;
	}
}

/**
 * Check if a given Access Address is known (seen at least twice).
 **/

bool BLEController::isAccessAddressKnown(uint32_t accessAddress) {
	bool found = false;
	for (int i=0;i<MAX_AA_CANDIDATES;i++) {
		if (this->activeConnectionRecovery.accessAddressCandidates.aa[i] == accessAddress) {
            this->activeConnectionRecovery.accessAddressCandidates.seen[i]++;
            if (this->activeConnectionRecovery.accessAddressCandidates.seen[i] > 1) {
                found = true;
            }
            break;
        }
	}
	return found;
}


/**
 * Add a candidate Access Address to our list.
 *
 * This list is limited to 25 entries, when no slot is available this list
 * is sorted by candidate count and the bottom half candidates are discarded
 * before inserting a new one. This way, we keep track of the best candidates.
 **/

void BLEController::addCandidateAccessAddress(uint32_t accessAddress) {
    uint32_t x,z;
    int change;
    CandidateAccessAddresses *candidates = &this->activeConnectionRecovery.accessAddressCandidates;

    if (candidates->count < MAX_AA_CANDIDATES) {
        candidates->seen[candidates->count] = 1;
		candidates->aa[candidates->count++] = accessAddress;
    } else {
        /* No more space? Sort our list, remove half the values and add our AA. */
        do
        {
            change = 0;
            for (int i=0; i<(candidates->count - 1); i++)
            {
                for (int j=i+1; j<candidates->count; j++)
                {
                    if (candidates->seen[i] < candidates->seen[j])
                    {
                        x = candidates->seen[i];
                        candidates->seen[i] = candidates->seen[j];
                        candidates->seen[j] = x;
                        z = candidates->aa[i];
                        candidates->aa[i] = candidates->aa[j];
                        candidates->aa[j] = z;
                        change = 1;
                    }
                }
            }
        } while (change > 0);

        candidates->count /= 2;
        candidates->aa[candidates->count] = accessAddress;
        candidates->seen[candidates->count++] = 1;
    }
}


void BLEController::hopToNextDataChannel() {
	if (this->controllerState == SNIFFING_ACCESS_ADDRESS) {
		if (this->activeConnectionRecovery.accessAddressPreamble == 0xAA) {
			this->activeConnectionRecovery.accessAddressPreamble = 0x55;
			this->setAccessAddressDiscoveryConfiguration(0x55);
		}
		else {

			this->activeConnectionRecovery.accessAddressPreamble = 0xAA;


			int new_channel = (this->channel + 1) % 37;
			while (!this->activeConnectionRecovery.monitoredChannels[new_channel]) {
				new_channel = (new_channel + 1) % 37;
			}
			this->setChannel(new_channel);
			this->setAccessAddressDiscoveryConfiguration(0xAA);
		}
	}
	else if (this->controllerState == RECOVERING_CRC_INIT) {
		int new_channel = (this->channel + 1) % 37;
		while (!this->activeConnectionRecovery.monitoredChannels[new_channel]) {
			new_channel = (new_channel + 1) % 37;
		}
		this->setChannel(new_channel);
	}
	else if (this->controllerState == RECOVERING_CHANNEL_MAP) {
		if (this->activeConnectionRecovery.validPacketOccurences > 0) {
			this->activeConnectionRecovery.mappedChannels[this->channel] = MAPPED;
		}
		else {
			this->activeConnectionRecovery.mappedChannels[this->channel] = NOT_MAPPED;
		}
		int new_channel = -1;
		for (int i=0; i<37;i++) {
			if (this->activeConnectionRecovery.mappedChannels[i] == NOT_ANALYZED) {
				new_channel = i;
				break;
			}
		}
		if (new_channel != -1) {
			this->activeConnectionRecovery.validPacketOccurences = 0;
			this->setChannel(new_channel);
		}
		else {
			uint8_t channelMap[5] = {0};
			int i;

			for (i=0; i<5; i++) {
				channelMap[i] = 0;
			}

				for (i=0; i<37; i++)
				{
					channelMap[i/8] |= (this->activeConnectionRecovery.mappedChannels[i]==MAPPED)?(1 << (i%8)):0;
				}
				this->sendExistingConnectionReport(this->accessAddress, this->crcInit, channelMap, 0,0);
				this->recoverHopInterval(this->accessAddress, this->crcInit, channelMap);
				this->start();
		}
	}
}

bool BLEController::whitelistAdvAddress(bool enable,uint8_t a, uint8_t b, uint8_t c,uint8_t d, uint8_t e, uint8_t f) {
	if (this->controllerState == JAMMING_CONNECT_REQ) {
		uint8_t jammingPattern[] = {dewhiten_byte_ble(f,8,this->channel),
						dewhiten_byte_ble(e,9,this->channel),
						dewhiten_byte_ble(d,10,this->channel),
						dewhiten_byte_ble(c,11,this->channel),
						dewhiten_byte_ble(b,12,this->channel),
						dewhiten_byte_ble(a,13,this->channel)};

		uint8_t jammingMask[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
		if (enable) this->radio->addJammingPattern(jammingPattern,jammingMask,6,7);
		else this->radio->removeJammingPattern(jammingPattern,jammingMask,6,7);
		return true;
	}
	return false;
}

bool BLEController::whitelistInitAddress(bool enable,uint8_t a, uint8_t b, uint8_t c,uint8_t d, uint8_t e, uint8_t f) {
	if (this->controllerState == JAMMING_CONNECT_REQ) {
		uint8_t jammingPattern[] = {dewhiten_byte_ble(f,2,this->channel),
						dewhiten_byte_ble(e,3,this->channel),
						dewhiten_byte_ble(d,4,this->channel),
						dewhiten_byte_ble(c,5,this->channel),
						dewhiten_byte_ble(b,6,this->channel),
						dewhiten_byte_ble(a,7,this->channel)};

		uint8_t jammingMask[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
		if (enable) this->radio->addJammingPattern(jammingPattern,jammingMask,6,1);
		else this->radio->removeJammingPattern(jammingPattern,jammingMask,6,1);
		return true;
	}
	return false;
}


bool BLEController::whitelistConnection(bool enable,uint8_t a, uint8_t b, uint8_t c,uint8_t d, uint8_t e, uint8_t f,uint8_t ap, uint8_t bp, uint8_t cp,uint8_t dp, uint8_t ep, uint8_t fp) {
	if (this->controllerState == JAMMING_CONNECT_REQ) {
		uint8_t jammingPattern[] = {dewhiten_byte_ble(f,2,this->channel),
						dewhiten_byte_ble(e,3,this->channel),
						dewhiten_byte_ble(d,4,this->channel),
						dewhiten_byte_ble(c,5,this->channel),
						dewhiten_byte_ble(b,6,this->channel),
						dewhiten_byte_ble(a,7,this->channel),
						dewhiten_byte_ble(fp,8,this->channel),
						dewhiten_byte_ble(ep,9,this->channel),
						dewhiten_byte_ble(dp,10,this->channel),
						dewhiten_byte_ble(cp,11,this->channel),
						dewhiten_byte_ble(bp,12,this->channel),
						dewhiten_byte_ble(ap,13,this->channel)};

		uint8_t jammingMask[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
		if (enable) this->radio->addJammingPattern(jammingPattern,jammingMask,12,1);
		else this->radio->removeJammingPattern(jammingPattern,jammingMask,12,1);
		return true;
	}
	return false;
}

void BLEController::stop() {
	this->releaseTimers();
	this->sequenceModule->reset();
	this->radio->disable();
}

void BLEController::sniff() {
	this->controllerState = SNIFFING_ADVERTISEMENTS;
}

void BLEController::recoverCrcInit(uint32_t accessAddress) {
	this->controllerState = RECOVERING_CRC_INIT;
	this->activeConnectionRecovery.firstChannel = -1;
	this->activeConnectionRecovery.secondChannel = -1;

	this->accessAddress = accessAddress;
}

void BLEController::recoverChannelMap(uint32_t accessAddress, uint32_t crcInit) {
	this->controllerState = RECOVERING_CHANNEL_MAP;
	this->accessAddress = accessAddress;
	this->crcInit = crcInit;

	this->activeConnectionRecovery.firstChannel = -1;
	this->activeConnectionRecovery.secondChannel = -1;


	for (int i=0;i<37;i++) {
		if (this->activeConnectionRecovery.monitoredChannels[i]) {
			this->activeConnectionRecovery.mappedChannels[i] = NOT_ANALYZED;
		}
		else {
			this->activeConnectionRecovery.mappedChannels[i] = NOT_MONITORED;
		}
	}
}


void BLEController::recoverHopInterval(uint32_t accessAddress, uint32_t crcInit, uint8_t *channelMap) {
	this->controllerState = RECOVERING_HOP_INTERVAL;
	this->accessAddress = accessAddress;
	this->crcInit = crcInit;

	this->activeConnectionRecovery.firstChannel = -1;
	this->activeConnectionRecovery.secondChannel = -1;

	this->updateChannelsInUse(channelMap);

}

void BLEController::recoverHopIncrement(uint32_t accessAddress, uint32_t crcInit, uint8_t *channelMap, uint16_t hopInterval) {
	this->controllerState = RECOVERING_HOP_INCREMENT;
	this->accessAddress = accessAddress;
	this->crcInit = crcInit;
	this->updateChannelsInUse(channelMap);
	this->updateHopInterval(hopInterval);
}

void BLEController::attachToExistingConnection(uint32_t accessAddress, uint32_t crcInit, uint8_t *channelMap, uint16_t hopInterval, uint8_t hopIncrement) {
	this->controllerState = ATTACH_TO_EXISTING_CONNECTION;
	this->accessAddress = accessAddress;
	this->crcInit = crcInit;
	this->updateChannelsInUse(channelMap);
	this->updateHopInterval(hopInterval);
	this->updateHopIncrement(hopIncrement);
}

void BLEController::sniffAccessAddresses() {
	this->controllerState	= SNIFFING_ACCESS_ADDRESS;
}

void BLEController::setAccessAddressDiscoveryConfiguration(uint8_t preamble) {
  uint8_t two_bytes_preamble[] = {preamble, 0xff};
  this->radio->setPreamble(two_bytes_preamble, 2);
  this->radio->setPrefixes(0xa8,0x1f,0x9f,0xaf, 0xa9,0x00,0xFF);
  //this->radio->setPrefixes();
  this->radio->setMode(MODE_NORMAL);
  this->radio->setFastRampUpTime(true);
  this->radio->setEndianness(LITTLE);
  this->radio->setTxPower(POS8_DBM);
  this->radio->enableRssi();

  this->radio->setPhy(BLE_1MBITS);

  this->radio->setHeader(0,0,0);
  this->radio->setWhitening(NO_WHITENING);
  this->radio->setWhiteningDataIv(0);
	this->radio->disableJammingPatterns();
  this->radio->setCrc(NO_CRC);
  this->radio->setCrcSkipAddress(false);
  this->radio->setCrcSize(2);
  this->radio->setCrcInit(0xFFFF);
  this->radio->setCrcPoly(0x1021);
  this->radio->setPayloadLength(8);
  this->radio->setInterFrameSpacing(0);
  this->radio->setExpandPayloadLength(8);
  this->radio->setFrequency(BLEController::channelToFrequency(channel));
  this->radio->reload();
}
void BLEController::setReactiveJammerConfiguration(uint8_t *pattern, size_t size, int position) {
	this->controllerState = REACTIVE_JAMMING;
	memcpy(this->reactiveJammingPattern.pattern, pattern, size);
	this->reactiveJammingPattern.size = size;
	this->reactiveJammingPattern.position = position;
	uint8_t whitened_pattern[this->reactiveJammingPattern.size];
	for (size_t i=0;i<this->reactiveJammingPattern.size;i++) {
		whitened_pattern[this->reactiveJammingPattern.size - 1 - i] = dewhiten_byte_ble(this->reactiveJammingPattern.pattern[i], this->reactiveJammingPattern.position+i, this->channel);
	}
	this->radio->setPreamble(whitened_pattern,(size <= 4 ? size  : 4));
	this->radio->setPrefixes();
	this->radio->setMode(MODE_JAMMER);
	this->radio->setFastRampUpTime(true);
	this->radio->setEndianness(LITTLE);
	this->radio->setTxPower(POS8_DBM);
	this->radio->disableRssi();
	this->radio->setPhy(BLE_1MBITS);
	this->radio->setHeader(0,0,0);
	this->radio->setWhitening(NO_WHITENING);
	this->radio->setWhiteningDataIv(0);
	this->radio->disableJammingPatterns();
	this->radio->setCrc(NO_CRC);
	this->radio->setCrcSkipAddress(true);

	this->radio->setCrcSize(0);
	this->radio->setPayloadLength(4);
	this->radio->setInterFrameSpacing(0);
	this->radio->setExpandPayloadLength(4);

	//this->radio->setJammingInterval(30000); // prevent jamming the same packet multiple times
	this->radio->setFrequency(BLEController::channelToFrequency(this->channel));
	this->radio->reload();

}
void BLEController::setJammerConfiguration() {
	this->controllerState = JAMMING_CONNECT_REQ;
	uint8_t connectReq[4] = {dewhiten_byte_ble(0x05,0,this->channel),0x8e,0x89,0xbe};
	this->radio->setPrefixes(dewhiten_byte_ble(0x45,0,this->channel), dewhiten_byte_ble(0x85,0,this->channel), dewhiten_byte_ble(0xc5,0,this->channel));
	this->radio->setPreamble(connectReq,4);
	this->radio->setMode(MODE_JAMMER);
	this->radio->setFastRampUpTime(true);
	this->radio->setEndianness(LITTLE);
	this->radio->setTxPower(POS8_DBM);
	this->radio->disableRssi();
	this->radio->setPhy(BLE_1MBITS);
	this->radio->setHeader(0,0,0);
	this->radio->setWhitening(NO_WHITENING);
	this->radio->setJammingPatternsCounter(8+2*6*8);
	this->radio->enableJammingPatterns();
	this->radio->setCrc(NO_CRC);
	this->radio->setCrcSkipAddress(false);
	this->radio->setCrcSize(0);
	this->radio->setPayloadLength(20);
	this->radio->setInterFrameSpacing(0);
	this->radio->setExpandPayloadLength(20);
	this->radio->setFrequency(BLEController::channelToFrequency(channel));
	this->radio->reload();
}

void BLEController::setCrcRecoveryConfiguration(uint32_t accessAddress) {
	this->accessAddress = accessAddress;
	uint8_t accessAddressPreamble[4] = {(uint8_t)((accessAddress & 0xFF000000) >> 24),
						(uint8_t)((accessAddress & 0x00FF0000) >> 16),
						(uint8_t)((accessAddress & 0x0000FF00) >> 8),
						(uint8_t)(accessAddress & 0x000000FF)};
	this->radio->setPreamble(accessAddressPreamble,4);
	this->radio->setPrefixes();
	this->radio->setMode(MODE_NORMAL);
	this->radio->setFastRampUpTime(true);
	this->radio->setEndianness(LITTLE);
	this->radio->setTxPower(POS8_DBM);
	this->radio->disableRssi(); // prevent time overhead in connected mode
	this->radio->setPhy(BLE_1MBITS);
	this->radio->setHeader(0,0,0);
	this->radio->setWhitening(HARDWARE_WHITENING);
	this->radio->setWhiteningDataIv(this->channel);
	this->radio->disableJammingPatterns();
	this->radio->setCrc(NO_CRC);
	this->radio->setCrcSkipAddress(true);
	this->radio->setCrcSize(3);
	this->radio->setCrcInit(0);
	this->radio->setCrcPoly(0x100065B);
	this->radio->setPayloadLength(10);
	this->radio->setInterFrameSpacing(0);
	this->radio->setExpandPayloadLength(10);
	this->radio->setFrequency(BLEController::channelToFrequency(channel));
	this->radio->reload();
}

bool BLEController::rawInject(uint8_t* pdu, size_t size, int channel, uint32_t access_address) {
		uint32_t oldAccessAddress = this->accessAddress;
		uint32_t oldCrcInit = this->crcInit;
		int oldChannel = this->channel;
		this->channel = channel;
		this->setHardwareConfiguration(access_address, access_address == 0x8e89bed6 ? 0x555555 : oldCrcInit);

		this->radio->send(pdu, size, BLEController::channelToFrequency(channel),channel);
		nrf_delay_us(100 + size * 8);
		// Restore previous configuration
		this->accessAddress = oldAccessAddress;
		this->crcInit = oldCrcInit;
		this->channel = oldChannel;
		this->setHardwareConfiguration(this->accessAddress, this->crcInit);
		return true;
}

void BLEController::setHardwareConfiguration(uint32_t accessAddress, uint32_t crcInit) {
	this->accessAddress = accessAddress;
	this->crcInit = crcInit;
	uint8_t accessAddressPreamble[4] = {(uint8_t)((accessAddress & 0xFF000000) >> 24),
						(uint8_t)((accessAddress & 0x00FF0000) >> 16),
						(uint8_t)((accessAddress & 0x0000FF00) >> 8),
						(uint8_t)(accessAddress & 0x000000FF)};
	this->radio->setPreamble(accessAddressPreamble,4);
	this->radio->setPrefixes();
	this->radio->setMode(MODE_NORMAL);
	this->radio->setFastRampUpTime(true);
	this->radio->setEndianness(LITTLE);
	this->radio->setTxPower(POS8_DBM);
	if (accessAddress == 0x8e89bed6) {
		this->radio->enableRssi();
	}
	else {
		this->radio->disableRssi(); // prevent time overhead in connected mode
		//this->radio->enableMatch(9 + 1*8);
	}
	this->radio->setPhy(BLE_1MBITS);
	this->radio->setHeader(1,8,0);
	this->radio->setWhitening(HARDWARE_WHITENING);
	this->radio->setWhiteningDataIv(this->channel);
	this->radio->disableJammingPatterns();
	this->radio->setCrc(HARDWARE_CRC);
	this->radio->setCrcSkipAddress(true);
	this->radio->setCrcSize(3);
	this->radio->setCrcInit(crcInit);
	this->radio->setCrcPoly(0x100065B);
	this->radio->setPayloadLength(40);
	this->radio->setInterFrameSpacing(0);
	this->radio->setExpandPayloadLength(0);
	this->radio->setFrequency(BLEController::channelToFrequency(channel));
	this->radio->reload();
}

void BLEController::startScanning(bool active) {
    /* Enable scanning. */
	this->controllerState = SCANNING;
	this->activeScanning = active;
}

void BLEController::followAdvertisingDevice(uint8_t a,uint8_t b,uint8_t c,uint8_t d,uint8_t e,uint8_t f) {
	this->controllerState = COLLECTING_ADVINTERVAL;
	this->setFilter(true, a,b,c,d,e,f);
	this->collectedIntervals = 0;
	this->setChannel(37);
}

void BLEController::calculateAdvertisingIntervals() {
	uint32_t intervals[ADV_REPORT_SIZE];
	for (int i=0;i<ADV_REPORT_SIZE;i++) {
		intervals[i] = this->timestampsThirdChannel[i] - this->timestampsFirstChannel[i];
	}
	sort_array(intervals,ADV_REPORT_SIZE);
	int firstQuartile = (int)((1/4)*(ADV_REPORT_SIZE+1));
	uint32_t interval = (uint32_t)(intervals[firstQuartile]/2);
	this->sendAdvIntervalReport(interval);
	this->setChannel(37);
	this->controllerState = SNIFFING_ADVERTISEMENTS;
	this->collectedIntervals = 0;
}
void BLEController::setFilter(bool hardwareFilter, uint8_t a,uint8_t b,uint8_t c,uint8_t d,uint8_t e,uint8_t f) {
	this->filter.bytes[0] = a;
	this->filter.bytes[1] = b;
	this->filter.bytes[2] = c;
	this->filter.bytes[3] = d;
	this->filter.bytes[4] = e;
	this->filter.bytes[5] = f;

	if (hardwareFilter) {
		this->softwareFilterEnabled = false;
		if (a == 0xFF && b == 0xFF && c == 0xFF && d == 0xFF && e == 0xFF && f == 0xFF) {
			//this->radio->disable();
			this->radio->disableFilter();
			this->radio->reload();
		}
		else {

			//this->radio->disable();
			this->radio->enableFilter(this->filter);
			this->radio->reload();
		}
	}
	else {
		this->softwareFilterEnabled = true;
		this->radio->disableFilter();
		this->radio->reload();
	}
}

void BLEController::advertise(uint8_t *advertisingData, size_t advertisingDataSize, uint8_t *scanData, size_t scanDataSize, bool connectable, uint32_t interval) {
	this->releaseTimers();
	memcpy(this->advertisingData.advertisingData, advertisingData, advertisingDataSize);
	memcpy(this->advertisingData.scanData, scanData, scanDataSize);
	this->advertisingData.advertisingDataSize = advertisingDataSize;
	this->advertisingData.scanDataSize = scanDataSize;
	this->advertisingData.connectable = connectable;
	this->advertisingData.advertisingInterval = interval;
	this->advertisingData.advertisingDelay = 0;
	this->controllerState = ADVERTISING;


}

bool BLEController::newAdvertisingTransmission() {
	if (this->advertisingTimer == NULL) {
		this->advertisingTimer = this->timerModule->getTimer();
	}
	this->advertisingTimer->setMode(REPEATED);
	this->advertisingTimer->setCallback((ControllerCallback)&BLEController::newAdvertisingTransmission, this);
	this->advertisingTimer->update(this->channel != 39 ? 5000 : (this->advertisingData.advertisingInterval * 625) - 5000*2);

	if (!this->advertisingTimer->isStarted()) this->advertisingTimer->start();

	if (this->channel == 37) {
		this->setChannel(38);
	}
	else if (this->channel == 38) {
		this->setChannel(39);
	}
	else if (this->channel == 39) {
		this->setChannel(37);
	}
	else {
		this->setChannel(37);
	}

	// Configure radio to monitor only advertisements from targeted device (hardware filter needed)
	this->setFilter(false, this->own.bytes[5], this->own.bytes[4],this->own.bytes[3], this->own.bytes[2], this->own.bytes[1],this->own.bytes[0]);

	this->advertisingData.lastAdvertisingEvent = this->timerModule->getTimestamp();
	uint8_t *adv_ind;
	size_t adv_ind_size;
	BLEPacket::forgeAdvInd(&adv_ind, &adv_ind_size, this->own.bytes, this->ownRandom, this->advertisingData.advertisingData, this->advertisingData.advertisingDataSize);
	this->radio->send(adv_ind, adv_ind_size, BLEController::channelToFrequency(this->channel),this->channel);
	bsp_board_led_invert(0);
	free(adv_ind);
	return true;
}


void BLEController::followConnection(
        ChanSelAlg csa, uint16_t hopInterval, uint8_t hopIncrement, uint8_t *channelMap,uint32_t accessAddress,
        uint32_t crcInit, int masterSCA, uint16_t latency, uint16_t winOffset, uint8_t winSize)
{
    // Transmit window starts at transmitDelay + transmitWindowOffset.
    unsigned long transmitWindow = 1250UL + (winOffset * 1250L);
    unsigned long transmitWindowEnd = transmitWindow + (winSize * 1250L);

    /* 
     * Special case: when winOffset is set to 0xFFFF, we listen on the current channel 
     * until we get a valid packet. Timeout is set to 37*hopInterval to allow sync check
     * once we've looped over the entire channel sequence.
     */
    if (winOffset == 0xffff) {
        transmitWindow = 1250UL;
        transmitWindowEnd = 37 * hopInterval * 1250;
    }

	// We update the parameters needed to follow the connection
	this->updateHopInterval(hopInterval);
	this->updateHopIncrement(hopIncrement);
	this->updateChannelsInUse(channelMap);

	this->controllerState = SNIFFING_CONNECTION;

	this->latency = latency;

	this->packetCount = 0;
	this->connectionEventCount = 0;

	this->mdCount = 0;

	// Reset attack status
	this->attackStatus.attack = BLE_ATTACK_NONE;
	this->attackStatus.running = false;
	this->attackStatus.injecting = false;
	this->attackStatus.successful = false;

	// Initially, we are not synchronized
	this->sync = false;

	// This counter indicates if we have lost the connection
	this->desyncCounter = 0;


    // Save current channel selection algorithm.
    this->csa = csa;

    // Initialize CSA2 counter and channel ID if selected.
    if (this->csa == CSA2) {
        this->csa2_chan_id = ((accessAddress & 0xffff0000)>>16) ^ (accessAddress & 0x0000ffff);
    }
    
	// We calculate the first channel (use the current channel when winSize is set 0xff)
    if (winSize != 0xFF) {
        this->lastUnmappedChannel = 0;
        this->channel = this->nextChannel();
    } else {
        this->lastUnmappedChannel = this->channel;
    }


	// No connection update is expected
	this->clearConnectionUpdate();
    this->clearPhyUpdate();

	/*
	uint8_t reject_ind[] = {0x03,0x02, 0x0d, 0x06};
	this->setAttackPayload(reject_ind, 4);
	this->startAttack(BLE_ATTACK_FRAME_INJECTION_TO_MASTER);
	*/
	// Radio configuration
	this->setHardwareConfiguration(accessAddress, crcInit);


	// Timers configuration
	if (this->connectionTimer == NULL) {
        // First packet will be transmitted no early than window delay (1.25ms) + window offset.
        // We need to update our timer once our connection synchronized (when in follow mode).
		this->connectionTimer = this->timerModule->getTimer();
		this->connectionTimer->setMode(SINGLE_SHOT);
		this->connectionTimer->setCallback((ControllerCallback)&BLEController::goToNextChannel, this);
		this->connectionTimer->update(transmitWindow - 350);
	}
	this->masterSCA = masterSCA;
	this->slaveSCA = 20;


	if (this->timeoutTimer == NULL) {
        // After transmitWindowEnd, connection is considered as not successfully initiated.
		this->timeoutTimer = this->timerModule->getTimer();
		this->timeoutTimer->setMode(SINGLE_SHOT);
		this->timeoutTimer->setCallback((ControllerCallback)&BLEController::checkSynchronization, this);
		this->timeoutTimer->update(transmitWindowEnd);
		this->timeoutTimer->start();
	}
}

bool BLEController::checkSynchronization() {
	if (this->sync) {
		this->sendConnectionReport(CONNECTION_STARTED);
	}
	else {
		this->releaseTimers();

		// We are not synchronized anymore
		this->sync = false;
	
        // Send a connection lost event.
		this->sendConnectionReport(CONNECTION_LOST);

        // We are not waiting for an update
		this->clearConnectionUpdate();
        this->clearPhyUpdate();

		if (this->controllerState == CONNECTION_INITIATION || this->controllerState == SIMULATING_MASTER) {
			this->connect(
				this->connectionInitiationData.responder.bytes,
				this->connectionInitiationData.responderRandom,
				this->connectionInitiationData.accessAddress,
				this->connectionInitiationData.crcInit,
				this->connectionInitiationData.windowSize,
				this->connectionInitiationData.windowOffset,
				this->connectionInitiationData.hopInterval,
				this->connectionInitiationData.slaveLatency,
				this->connectionInitiationData.timeout,
				this->connectionInitiationData.sca,
				this->connectionInitiationData.hopIncrement,
				this->connectionInitiationData.channelMap
			);
		}
		else {
			// Reconfigure radio to receive advertisements
			this->setHardwareConfiguration(0x8e89bed6,0x555555);
			
            // Reset the channel
			this->setChannel(this->lastAdvertisingChannel);

            // Back to advertisements sniffing
			this->controllerState = SNIFFING_ADVERTISEMENTS;

			//Core::instance->getLedModule()->off(LED2);
		}
	}
	return false;
}

void BLEController::prepareInjectionToMaster() {
	this->controllerState = INJECTING_TO_MASTER;
	this->attackStatus.injecting = true;
}

void BLEController::prepareInjectionToSlave() {
	this->controllerState = INJECTING_TO_SLAVE;
}

void BLEController::prepareSlaveHijacking() {
	// Reset the hijacking counter
	this->attackStatus.hijackingCounter = 0;

	// Forge and configure a terminate_ind packet for injection to slave
	uint8_t *terminate_ind;
	size_t terminate_ind_size;
	BLEPacket::forgeTerminateInd(&terminate_ind, &terminate_ind_size,0x13);
	this->setAttackPayload(terminate_ind,terminate_ind_size);

	// Release the memory
	free(terminate_ind);
}

void BLEController::prepareMasterRelatedHijacking() {
	// Configure the update instant as current connection event count + 100
	this->attackStatus.nextInstant = this->connectionEventCount + 100;

	// Forge and configure a connection update request packet for injection to slave
	uint8_t *connection_update;
	size_t connection_update_size;
	BLEPacket::forgeConnectionUpdateRequest(&connection_update,
						&connection_update_size,
						2 /* WinSize */,
						4 /* WinOffset */,
						this->hopInterval /* Hop Interval */,
						0 /* Latency */,
						600 /*timeout*/,
						this->attackStatus.nextInstant /* Instant */);
	this->setAttackPayload(connection_update, connection_update_size);

	// Release the memory
	free(connection_update);
}

void BLEController::startAttack(BLEAttack attack) {
	this->slavePayload.transmitted = true;
	this->masterPayload.transmitted = true;

	this->attackStatus.attack = attack;
	this->attackStatus.running = true;
	this->attackStatus.successful = false;
	this->attackStatus.injecting = false;
	this->attackStatus.injectionCounter = 0;
	if (attack == BLE_ATTACK_FRAME_INJECTION_TO_MASTER) {
		this->prepareInjectionToMaster();
	}
	else {
		this->prepareInjectionToSlave();
	}
	this->sendConnectionReport(ATTACK_STARTED);

	if (attack == BLE_ATTACK_SLAVE_HIJACKING) {
		this->prepareSlaveHijacking();
	}
	else if (attack == BLE_ATTACK_MASTER_HIJACKING || attack == BLE_ATTACK_MITM) {
		this->prepareMasterRelatedHijacking();
	}
}

bool BLEController::isSlavePayloadTransmitted() {
	return this->slavePayload.transmitted;
}
bool BLEController::isMasterPayloadTransmitted() {
	return this->masterPayload.transmitted;
}


void BLEController::executeInjectionToSlave() {
	// If needed, configure the injection timer
	if (this->injectionTimer == NULL) {
		this->injectionTimer = this->timerModule->getTimer();
		this->injectionTimer->setMode(SINGLE_SHOT);

		// The inject callback method will be triggered when the timer reach its thresold value
		this->injectionTimer->setCallback((ControllerCallback)&BLEController::inject, this);
	}
}

void BLEController::executeInjectionToMaster() {
	// Enter slave injection mode with an IFS of 145us  (we want to transmit before the legitimate slave)
	this->enterSlaveInjectionMode(145);

	// Forge an empty data packet with MD bit set to 1
	//uint8_t tx_buffer[2];

	/*tx_buffer[0] = (0x01 & 0xF3) | (this->simulatedSlaveSequenceNumbers.nesn  << 2) | (this->simulatedSlaveSequenceNumbers.sn << 3) | (1 << 4); // MD = 1
	tx_buffer[1] = 0x00;
	this->radio->updateTXBuffer(tx_buffer,2);
	*/
	 // Dirty version : working
	 this->attackStatus.payload[0] = (this->attackStatus.payload[0] & 0xF3) | (this->simulatedSlaveSequenceNumbers.nesn  << 2) | (this->simulatedSlaveSequenceNumbers.sn << 3); // MD = 1
	 this->radio->updateTXBuffer(this->attackStatus.payload,this->attackStatus.size);
	 this->simulatedSlaveSequenceNumbers.sn = (this->simulatedSlaveSequenceNumbers.sn + 1) % 2; // necessary ?
	 //this->simulatedSlaveSequenceNumbers.nesn = (this->simulatedSlaveSequenceNumbers.nesn + 1) % 2; // necessary ?
	//this->setEmptyTransmitIndicator(true);

}

void BLEController::executeMasterRelatedHijacking() {
	if (this->masterTimer == NULL) {
		this->masterTimer = TimerModule::instance->getTimer();
		this->masterTimer->setMode(REPEATED);
		this->masterTimer->setCallback((ControllerCallback)&BLEController::masterRoleCallback, this);
	}
	this->controllerState = (this->attackStatus.attack == BLE_ATTACK_MASTER_HIJACKING ? SYNCHRONIZING_MASTER : SYNCHRONIZING_MITM);
}

void BLEController::executeAttack() {
	if (this->attackStatus.running && !this->attackStatus.successful) {
		if (this->attackStatus.attack == BLE_ATTACK_FRAME_INJECTION_TO_MASTER) {
			if (this->attackStatus.injecting) {
				this->executeInjectionToMaster();
				this->attackStatus.injectionCounter++;
			}
		}
		else {
			this->executeInjectionToSlave();
		}
	}
	// If the injection has been successful and we have to hijack master or perform MiTM, start master callback (attack step 2)
	if (this->attackStatus.successful && this->controllerState == INJECTING_TO_SLAVE) {
		if (this->attackStatus.attack == BLE_ATTACK_MASTER_HIJACKING || this->attackStatus.attack == BLE_ATTACK_MITM) {
				this->executeMasterRelatedHijacking();
		}
	}
	// If we successfully performed the MiTM, we have to mimick the slave behaviour too
	else if (this->attackStatus.successful && this->controllerState == PERFORMING_MITM) {
		this->enterSlaveMode();
	}
}

bool BLEController::masterRoleCallback(BLEPacket *pkt) {

	// BLE CCM nonce for a central: encrypt our OUTGOING packet with direction=1
	this->encryptionData.direction = 1;
	set_ccm_counter(&this->encryptionData, this->encTxCounter);

	if (!this->masterPayload.transmitted) {

		if ((this->masterPayload.payload[0] & 0x10) != 0) {
			this->mdSequence = true;
			this->mdCount = 4;
		}
		else {
			this->mdCount = 0;
			this->mdSequence = false;
		}
		this->masterPayload.responseReceived = !BLEPacket::needResponse(this->masterPayload.payload, this->masterPayload.size);
		this->masterPayload.payload[0] = (this->masterPayload.payload[0] & 0xF3) | (this->simulatedMasterSequenceNumbers.nesn  << 2) | (this->simulatedMasterSequenceNumbers.sn << 3);
		this->radio->send(this->masterPayload.payload,this->masterPayload.size,BLEController::channelToFrequency(this->channel),this->channel);

	}
	else {
		this->temporaryPayload.payload[0] = (0x01 & 0xF3) | (this->simulatedMasterSequenceNumbers.nesn  << 2) | (this->simulatedMasterSequenceNumbers.sn << 3);
		this->temporaryPayload.payload[1] = 0x00;
		this->temporaryPayload.size = 2;
		this->radio->send(this->temporaryPayload.payload, this->temporaryPayload.size, BLEController::channelToFrequency(this->channel), this->channel);
	}

	this->encryptionData.direction = 0;
	set_ccm_counter(&this->encryptionData, this->encRxCounter);
	return true;
}

bool BLEController::slaveRoleCallback(BLEPacket *pkt) {
	this->updateSimulatedSlaveSequenceNumbers(this->masterSequenceNumbers.nesn, (this->simulatedSlaveSequenceNumbers.nesn + 1) % 2);

	if (!this->slavePayload.transmitted) {
		this->slavePayload.payload[0] = (this->slavePayload.payload[0] & 0xF3) | (this->simulatedSlaveSequenceNumbers.nesn  << 2) | (this->simulatedSlaveSequenceNumbers.sn << 3);
		this->radio->updateTXBuffer(this->slavePayload.payload,this->slavePayload.size);
		this->slavePayload.transmitted = true;
		//Core::instance->pushMessageToQueue(new SendPayloadResponse(0x02,true));
	}
	else {
		uint8_t tx_buffer[2];
		tx_buffer[0] = (0x01 & 0xF3) | (this->simulatedSlaveSequenceNumbers.nesn  << 2) | (this->simulatedSlaveSequenceNumbers.sn << 3);
		tx_buffer[1] = 0x00;
		this->radio->updateTXBuffer(tx_buffer,2);
	}
	return false;
}

void BLEController::checkAttackSuccess() {
	// We check if the injection was successful


	if (
			(this->attackStatus.attack == BLE_ATTACK_FRAME_INJECTION_TO_SLAVE  ||
			this->attackStatus.attack == BLE_ATTACK_MASTER_HIJACKING ||
			this->attackStatus.attack == BLE_ATTACK_MITM) &&
			this->attackStatus.injecting
		) {
			if (this->attackStatus.injectionTimestamp + BLE_IFS + ((BLE_1MBPS_PREAMBLE_SIZE+ACCESS_ADDRESS_SIZE+CRC_SIZE+this->attackStatus.size)*8) - 15 < this->lastSlaveTimestamp &&
			this->attackStatus.injectionTimestamp + BLE_IFS + ((BLE_1MBPS_PREAMBLE_SIZE+ACCESS_ADDRESS_SIZE+CRC_SIZE+this->attackStatus.size)*8) + 15 > this->lastSlaveTimestamp &&
			this->attackStatus.injectedSequenceNumbers.nesn == this->slaveSequenceNumbers.sn &&
			((this->attackStatus.injectedSequenceNumbers.sn + 1)%2) == this->slaveSequenceNumbers.nesn
			) {
				if (!this->attackStatus.successful) {
					Core::instance->getLedModule()->on(LED2);
					Core::instance->getLedModule()->setColor(GREEN);

					this->sendInjectionReport(true,this->attackStatus.injectionCounter);
					if (this->attackStatus.attack == BLE_ATTACK_FRAME_INJECTION_TO_SLAVE) {
						this->controllerState = SNIFFING_CONNECTION;
						this->attackStatus.attack = BLE_ATTACK_NONE;
					}
				}
				this->attackStatus.running = false;
				this->attackStatus.successful = true;
			}
			this->attackStatus.injecting = false;
	}
	if (
			(this->attackStatus.attack == BLE_ATTACK_FRAME_INJECTION_TO_MASTER) &&
			this->attackStatus.injecting
		) {
			if (this->attackStatus.injectionCounter > 1) {
				if (!this->attackStatus.successful) {
					this->exitSlaveMode();
					Core::instance->getLedModule()->on(LED2);
					Core::instance->getLedModule()->setColor(GREEN);

					this->sendInjectionReport(true,this->attackStatus.injectionCounter);
					this->attackStatus.injecting = false;
					this->controllerState = SNIFFING_CONNECTION;
					this->attackStatus.attack = BLE_ATTACK_NONE;
					this->attackStatus.injectionCounter = 0;
				}
				this->attackStatus.running = false;
				this->attackStatus.successful = true;

		}
	}
	// In the specific case of slave hijacking, we relies on an hijacking counter to check the attack success
	else if (this->attackStatus.attack == BLE_ATTACK_SLAVE_HIJACKING) {
		this->attackStatus.hijackingCounter = (this->packetCount <= 1 ? this->attackStatus.hijackingCounter + 1 : 0);
		if (this->attackStatus.hijackingCounter > (uint32_t)(this->latency)) {
			if (!this->attackStatus.successful)  {
				//We are sending a notification to Host
				this->sendConnectionReport(ATTACK_SUCCESS);
				Core::instance->getLedModule()->on(LED2);
				Core::instance->getLedModule()->setColor(GREEN);

				this->injectionTimer->stop();
				this->injectionTimer->release();
				this->injectionTimer = NULL;

				this->controllerState = SIMULATING_SLAVE;
				this->enterSlaveMode();
			}
			this->attackStatus.running = false;
			this->attackStatus.successful = true;
		}
		this->attackStatus.injecting = false;
	}
	else if (this->attackStatus.attack == BLE_ATTACK_MASTER_HIJACKING) {

		if (this->attackStatus.nextInstant + 10 == this->connectionEventCount) {
			if (this->controllerState == SIMULATING_MASTER) { // attack successful
				// Successful attack !
				//We are sending a notification to Host
				this->sendConnectionReport(ATTACK_SUCCESS);
				Core::instance->getLedModule()->on(LED2);
				Core::instance->getLedModule()->setColor(GREEN);
			}
			else {
				// Attack failed
				this->controllerState = SNIFFING_CONNECTION;
				this->attackStatus.attack = BLE_ATTACK_NONE;
				//We are sending a notification to Host
				this->sendConnectionReport(ATTACK_FAILURE);
				Core::instance->getLedModule()->on(LED2);
				Core::instance->getLedModule()->setColor(RED);
			}
		}
	}
	else if (this->attackStatus.attack == BLE_ATTACK_MITM) {

		if (this->attackStatus.nextInstant + 10 == this->connectionEventCount) {
			if (this->controllerState == PERFORMING_MITM) { // attack successful
				// Successful attack !
				//We are sending a notification to Host
				this->sendConnectionReport(ATTACK_SUCCESS);
				Core::instance->getLedModule()->on(LED2);
				Core::instance->getLedModule()->setColor(GREEN);
			}
			else {
				// Attack failed
				this->controllerState = SNIFFING_CONNECTION;
				this->attackStatus.attack = BLE_ATTACK_NONE;
				//We are sending a notification to Host
				this->sendConnectionReport(ATTACK_FAILURE);
				Core::instance->getLedModule()->on(LED2);
				Core::instance->getLedModule()->setColor(RED);
			}
		}
	}
}


void BLEController::enterSlaveMode() {
	this->radio->setFastRampUpTime(false);
	this->radio->setInterFrameSpacing(145);
	this->radio->enableAutoTXafterRX();
	this->radio->reload();
}

void BLEController::enterSlaveInjectionMode(int ifs) {
	this->radio->setFastRampUpTime(false);
	this->radio->setInterFrameSpacing(ifs);
	this->radio->enableAutoTXafterRX();
	this->radio->reload();
}

void BLEController::exitSlaveMode() {
	this->radio->setFastRampUpTime(true);
	this->radio->setInterFrameSpacing(0);
	this->radio->disableAutoTXafterRX();
	this->radio->reload();
}

bool BLEController::inject() {

	if (this->attackStatus.attack != BLE_ATTACK_NONE) {
		if (!this->attackStatus.injecting) {
			uint8_t *payload = (uint8_t *)malloc(sizeof(uint8_t) * this->attackStatus.size);
			for (size_t i=0;i<this->attackStatus.size;i++) payload[i] = this->attackStatus.payload[i];
			payload[0] = (payload[0] & 0xF3) | (((this->slaveSequenceNumbers.sn+1)%2) << 2)|(this->slaveSequenceNumbers.nesn << 3);
			this->attackStatus.injectionTimestamp = TimerModule::instance->getTimestamp();
			this->radio->send(payload,this->attackStatus.size,BLEController::channelToFrequency(this->channel),this->channel);
			Core::instance->getLedModule()->toggle(LED2);
			this->updateInjectedSequenceNumbers(this->slaveSequenceNumbers.nesn,(this->slaveSequenceNumbers.sn+1)%2);

			this->attackStatus.injectionCounter++;
			this->attackStatus.injecting = true;

			free(payload);
		}
	}
	return false;
}

BLEControllerState BLEController::getState() {
	return this->controllerState;
}

void BLEController::sendInjectionReport(bool status, uint32_t injectionCount) {
    /* Craft an injection report notification. */
    whad::NanoPbMsg *notification = new whad::ble::Injected(this->accessAddress, injectionCount, status);

    /* Add notification to our message queue. */
	Core::instance->pushMessageToQueue(notification);

    /* Free notification wrapper. */
    delete notification;
}

void BLEController::sendAdvIntervalReport(uint32_t interval) {
	//Core::instance->pushMessageToQueue(new AdvIntervalReportNotification(interval));
}
void BLEController::sendAccessAddressReport(uint32_t accessAddress, uint32_t timestamp, int32_t rssi) {
    /* Craft an access address report. */
    whad::NanoPbMsg *notification = new whad::ble::AccessAddressDiscovered(accessAddress, timestamp, rssi);

    /* Add notification to our message queue. */
	Core::instance->pushMessageToQueue(notification);

    /* Free notification wrapper. */
    delete notification;
}

void BLEController::sendExistingConnectionReport(uint32_t accessAddress, uint32_t crcInit, uint8_t *channelMap, uint16_t hopInterval, uint8_t hopIncrement) {
    whad::ble::ChannelMap *chanMap;

    /* Craft an existing connection report notification. */
    if (channelMap != NULL) {
        chanMap = new whad::ble::ChannelMap(channelMap);
    } else {
        chanMap = new whad::ble::ChannelMap();
    }
    whad::NanoPbMsg *notification = new whad::ble::Synchronized(accessAddress, crcInit, hopInterval, hopIncrement, *chanMap);

    /* Add notification to our message queue. */
	Core::instance->pushMessageToQueue(notification);

    /* Free notification wrapper. */
    delete notification;
}

void BLEController::sendConnectionReport(ConnectionStatus status) {
    whad::NanoPbMsg *notification = NULL;

    switch (status)
    {
        case CONNECTION_STARTED:
        {
            /* Craft a synchronization notification. */
            notification = new whad::ble::Synchronized(
                this->accessAddress, this->crcInit, this->hopInterval, this->hopIncrement,
                whad::ble::ChannelMap(this->channelMap)
            );
        }
        break;

        case DISCONNECTED:
        {
            /* Craft a disconnected notification. */
            notification = new whad::ble::Disconnected(0, 0x16);
        }
        break;

        case CONNECTION_LOST:
        {
            /* Craft a connection lost notification. */
            notification = new whad::ble::Desynchronized(this->accessAddress);
        }
        break;

        case ATTACK_SUCCESS:
        {
            if (this->attackStatus.attack == BLE_ATTACK_MITM ||
                this->attackStatus.attack == BLE_ATTACK_MASTER_HIJACKING  ||
                this->attackStatus.attack == BLE_ATTACK_SLAVE_HIJACKING)
            {
                notification = new whad::ble::Hijacked(this->accessAddress, true);
            }
        }
        break;

        case ATTACK_FAILURE:
        {
            if (this->attackStatus.attack == BLE_ATTACK_MITM ||
                this->attackStatus.attack == BLE_ATTACK_MASTER_HIJACKING  ||
                this->attackStatus.attack == BLE_ATTACK_SLAVE_HIJACKING)
            {
                notification = new whad::ble::Hijacked(this->accessAddress, false);
            }
        }
        break;

        default:
        {
            notification = NULL;
        }
        break;
    }

    /* If a notification has to be sent, process it. */
    if (notification != NULL)
    {
        /* Add notification to our message queue. */
        Core::instance->pushMessageToQueue(notification);

        /* Free notification wrapper. */
        delete notification;
    }
}


void BLEController::releaseTimers() {
	if (this->connectionTimer != NULL) {
		this->connectionTimer->stop();
		this->connectionTimer->release();
		this->connectionTimer = NULL;
	}
	if (this->injectionTimer != NULL) {
		this->injectionTimer->stop();
		this->injectionTimer->release();
		this->injectionTimer = NULL;
	}
	if (this->masterTimer != NULL) {
		this->masterTimer->stop();
		this->masterTimer->release();
		this->masterTimer = NULL;
	}
	if (this->discoveryTimer != NULL) {
		this->discoveryTimer->stop();
		this->discoveryTimer->release();
		this->discoveryTimer = NULL;
	}
	if (this->initTimer != NULL) {
		this->initTimer->stop();
		this->initTimer->release();
		this->initTimer = NULL;
	}
	if (this->timeoutTimer != NULL) {
		this->timeoutTimer->stop();
		this->timeoutTimer->release();
		this->timeoutTimer = NULL;
	}
	if (this->scanningTimer != NULL) {
		this->scanningTimer->stop();
		this->scanningTimer->release();
		this->scanningTimer = NULL;
	}
	if (this->advertisingTimer != NULL) {
		this->advertisingTimer->stop();
		this->advertisingTimer->release();
		this->advertisingTimer = NULL;
	}
}

void BLEController::sendTriggeredReport(uint8_t id) {
    /* Craft a triggered notification. */
    whad::NanoPbMsg *notification = new whad::ble::Triggered(id);

    /* Add notification to our message queue. */
	Core::instance->pushMessageToQueue(notification);

    /* Free notification wrapper. */
    delete notification;
}

void BLEController::sendConnectedReport() {
	uint8_t initiator[6] = {
		this->own.bytes[0],
		this->own.bytes[1],
		this->own.bytes[2],
		this->own.bytes[3],
		this->own.bytes[4],
		this->own.bytes[5]
	};
	uint8_t responder[6] = {
		this->connectionInitiationData.responder.bytes[5],
		this->connectionInitiationData.responder.bytes[4],
		this->connectionInitiationData.responder.bytes[3],
		this->connectionInitiationData.responder.bytes[2],
		this->connectionInitiationData.responder.bytes[1],
		this->connectionInitiationData.responder.bytes[0]

	};


    whad::ble::BDAddress initiatorAddr(
        this->ownRandom?whad::ble::AddressRandom:whad::ble::AddressPublic,
        initiator
    );
    whad::ble::BDAddress responderAddr(
        this->connectionInitiationData.responderRandom?whad::ble::AddressRandom:whad::ble::AddressPublic,
        responder
    );

    /* Craft a connected notification, enqueue and free wrapper. */
    whad::NanoPbMsg *notification = new whad::ble::Connected(0, responderAddr, initiatorAddr);
	Core::instance->pushMessageToQueue(notification);
    delete notification;
}

void BLEController::sendSlaveConnectedReport() {
	uint8_t responder[6] = {
		this->own.bytes[5],
		this->own.bytes[4],
		this->own.bytes[3],
		this->own.bytes[2],
		this->own.bytes[1],
		this->own.bytes[0]
	};

    whad::ble::BDAddress responderAddr(
        this->connectionInitiationData.responderRandom?whad::ble::AddressRandom:whad::ble::AddressPublic,
        responder
    );

	uint8_t initiator[6] = {
		this->connectionInitiationData.responder.bytes[5],
		this->connectionInitiationData.responder.bytes[4],
		this->connectionInitiationData.responder.bytes[3],
		this->connectionInitiationData.responder.bytes[2],
		this->connectionInitiationData.responder.bytes[1],
		this->connectionInitiationData.responder.bytes[0]

	};

    whad::ble::BDAddress initiatorAddr(
        this->ownRandom?whad::ble::AddressRandom:whad::ble::AddressPublic,
        initiator
    );

    /* Craft a connected notification. */
    whad::NanoPbMsg *notification = new whad::ble::Connected(
        0,              /* Connection handle */
        responderAddr,  /* Responder BD address */
        initiatorAddr   /* Initiator BD address */
    );

    /* Add notification to our message queue. */
    Core::instance->pushMessageToQueue(notification);

    /* Free notification wrapper. */
    delete notification;
}

void BLEController::connect(uint8_t *address, bool random) {
	uint8_t channelMap[5] = {
		0xFF,
		0xFF,
		0xFF,
		0xFF,
		0x1F
	};
	this->connect(address, random, 0x23a3d487, 0x049095, 3,9,56,0,42,1,9,channelMap);
}

bool BLEController::goToNextInitiationChannel() {
	if (this->sync) {
		return false;
	}
	else {
		if (this->channel == 37) {
			this->setChannel(38);
		}
		else if (this->channel == 38){
			this->setChannel(39);
		}
		else {
			this->setChannel(37);
		}
		this->connect(
			this->connectionInitiationData.responder.bytes,
			this->connectionInitiationData.responderRandom,
			this->connectionInitiationData.accessAddress,
			this->connectionInitiationData.crcInit,
			this->connectionInitiationData.windowSize,
			this->connectionInitiationData.windowOffset,
			this->connectionInitiationData.hopInterval,
			this->connectionInitiationData.slaveLatency,
			this->connectionInitiationData.timeout,
			this->connectionInitiationData.sca,
			this->connectionInitiationData.hopIncrement,
			this->connectionInitiationData.channelMap
		);
		return true;
	}
}

void BLEController::connect(uint8_t *address, bool random,  uint32_t accessAddress,  uint32_t crcInit, uint8_t windowSize, uint16_t windowOffset, uint16_t hopInterval, uint16_t slaveLatency, uint16_t timeout, uint8_t sca, uint8_t hopIncrement, uint8_t *channelMap) {
	// Configure connection parameters according to provided arguments
	memcpy(this->connectionInitiationData.responder.bytes, address, 6);
	this->connectionInitiationData.responderRandom = random;
	this->connectionInitiationData.accessAddress = accessAddress;
	this->connectionInitiationData.crcInit = crcInit;
	this->connectionInitiationData.windowSize = windowSize;
	this->connectionInitiationData.windowOffset = windowOffset;
	this->connectionInitiationData.hopInterval = hopInterval;
	this->connectionInitiationData.slaveLatency = slaveLatency;
	this->connectionInitiationData.timeout = timeout;
	this->connectionInitiationData.sca = sca;
	this->connectionInitiationData.hopIncrement = hopIncrement;
	memcpy(this->connectionInitiationData.channelMap, channelMap, 5);
	this->releaseTimers();
	if (this->scanningTimer == NULL) {
		this->scanningTimer = this->timerModule->getTimer();
	}
	this->scanningTimer->setMode(REPEATED);
	this->scanningTimer->setCallback((ControllerCallback)&BLEController::goToNextInitiationChannel, this);
	this->scanningTimer->update(500000);
	this->scanningTimer->start();

	// Enter Connection Initiation mode
	this->controllerState = CONNECTION_INITIATION;

	// Configure encryption counter
	set_ccm_counter(&this->encryptionData, 0);

	// Configure Hardware to monitor advertisements
	this->setHardwareConfiguration(0x8e89bed6,0x555555);


	this->radio->setFastRampUpTime(false);
	this->radio->setInterFrameSpacing(145);
	this->radio->enableAutoTXafterRX();

	// Build connection request
	size_t connection_request_size;
	uint8_t *connection_request;

	BLEPacket::forgeConnectionRequest(
			&connection_request,
			&connection_request_size,
			this->own.bytes,
			this->ownRandom,
			this->connectionInitiationData.responder.bytes,
			this->connectionInitiationData.responderRandom,
			this->connectionInitiationData.accessAddress,
			this->connectionInitiationData.crcInit,
			this->connectionInitiationData.windowSize ,
			this->connectionInitiationData.windowOffset,
			this->connectionInitiationData.hopInterval,
			this->connectionInitiationData.slaveLatency,
			this->connectionInitiationData.timeout,
			this->connectionInitiationData.sca,
			this->connectionInitiationData.hopIncrement,
			this->connectionInitiationData.channelMap
	);


	// Send the packet
	this->radio->updateTXBuffer(connection_request, connection_request_size);
	free(connection_request);
	// Configure radio to monitor only advertisements from targeted device (hardware filter needed)
	this->setFilter(true, address[0], address[1], address[2], address[3], address[4], address[5]);

	// Reload Radio configuration (to take into account radio custom parameters)
	this->radio->reload();
}



void BLEController::initializeConnection() {
	// We update the parameters needed to follow the connection
	this->updateHopInterval(this->connectionInitiationData.hopInterval);
	this->updateHopIncrement(this->connectionInitiationData.hopIncrement);
	this->updateChannelsInUse(this->connectionInitiationData.channelMap);

	this->emptyTransmitIndicator = true;

	this->radio->disableFilter();
	this->radio->disableAutoTXafterRX();
	this->radio->setFastRampUpTime(true);

	this->latency = this->connectionInitiationData.slaveLatency;

	this->packetCount = 0;
	this->connectionEventCount = 0;

	// Reset attack status
	this->attackStatus.attack = BLE_ATTACK_NONE;
	this->attackStatus.running = false;
	this->attackStatus.injecting = false;
	this->attackStatus.successful = false;

	// Initially, we are not synchronized
	this->sync = false;

	// This counter indicates if we have lost the connection
	this->desyncCounter = 0;


	// We calculate the first channel
	this->lastUnmappedChannel = 0;
	this->channel = this->nextChannel();

	this->masterPayload.transmitted = true;
	// Drop any master payloads left queued from a previous connection.
	this->masterQueueHead = 0;
	this->masterQueueTail = 0;
	this->mdSequence = false;
	this->mdCount = 0;

	this->clearConnectionUpdate();
    this->clearPhyUpdate();

	// Timers configuration
	if (this->connectionTimer == NULL) {
		this->connectionTimer = this->timerModule->getTimer();
		this->connectionTimer->setMode(REPEATED);
		this->connectionTimer->setCallback((ControllerCallback)&BLEController::goToNextChannel, this);
		this->connectionTimer->update(this->hopInterval * 1250UL - 250);
		this->connectionTimer->start();
	}

	if (this->masterTimer == NULL) {
		this->masterTimer = TimerModule::instance->getTimer();
		this->masterTimer->setMode(REPEATED);
		this->masterTimer->setCallback((ControllerCallback)&BLEController::masterRoleCallback, this);
		this->masterTimer->update(this->hopInterval * 1250UL);
		this->masterTimer->start();
	}

	this->setAnchorPoint(TimerModule::instance->getTimestamp());

	// Radio configuration
	this->setHardwareConfiguration(this->connectionInitiationData.accessAddress, this->connectionInitiationData.crcInit);

	this->masterSCA = this->connectionInitiationData.sca;
	this->slaveSCA = 20;

	if (this->timeoutTimer == NULL) {
		this->timeoutTimer = this->timerModule->getTimer();
		this->timeoutTimer->setMode(REPEATED);
		this->timeoutTimer->setCallback((ControllerCallback)&BLEController::checkSynchronization, this);
		this->timeoutTimer->update(this->hopInterval * 1250UL);
		this->timeoutTimer->start();
	}
}

bool BLEController::sendFirstConnectionPacket() {
	this->initializeConnection();
	this->simulatedMasterSequenceNumbers.nesn = 0;
	this->simulatedMasterSequenceNumbers.sn = 0;

	this->temporaryPayload.payload[0] = (0x01 & 0xF3) | (this->simulatedMasterSequenceNumbers.nesn  << 2) | (this->simulatedMasterSequenceNumbers.sn << 3);
	this->temporaryPayload.payload[1] = 0x00;
	this->temporaryPayload.size = 2;
	this->radio->send(this->temporaryPayload.payload, this->temporaryPayload.size, BLEController::channelToFrequency(this->channel), this->channel);
	return false;
}

void BLEController::connectionInitiationAdvertisementProcessing(BLEPacket *pkt) {

	// Update the packet direction
	pkt->updateSource(DIRECTION_UNKNOWN);
	// Transmit the packet to host
	this->addPacket(pkt);

	if (pkt->extractAdvertisementType() == ADV_IND) {
		if (this->initTimer == NULL) {
			this->initTimer = this->timerModule->getTimer();
			this->initTimer->setMode(SINGLE_SHOT);
			this->initTimer->setCallback((ControllerCallback)&BLEController::sendFirstConnectionPacket, this);
			this->initTimer->update(150 + 43*8 + 1250 + this->connectionInitiationData.windowOffset * 1250);
			this->initTimer->start();
		}
	}

}

void BLEController::connectionInitiationConnectedProcessing(BLEPacket *pkt) {
	if (!this->sync) {
		this->sync = true;

        bsp_board_led_on(0);

        /* No empty PDUs. */
        this->setEmptyTransmitIndicator(false);

		this->controllerState = SIMULATING_MASTER;
		this->sendConnectedReport();
	}
}

void BLEController::connectionInitiationConnectedSlaveProcessing(BLEPacket *pkt) {

	if (!this->sync) {
		this->sync = true;

		this->setAnchorPoint(pkt->getTimestamp());
		pkt->setConnectionHandle(0);

		bsp_board_led_on(0);
		bsp_board_led_on(1);
		this->slavePayload.transmitted = true;
		this->slavePayload.responseReceived = false;

		this->setEmptyTransmitIndicator(false);

		this->controllerState = SIMULATING_SLAVE;
		this->enterSlaveMode();
		this->sendSlaveConnectedReport();
	}
}
void BLEController::advertisementScanningProcessing(BLEPacket *pkt) {
	if (this->activeScanning) {
		// Build connection request
		size_t scan_request_size;
		uint8_t *scan_request;

		uint8_t address[6];
		bool random;
		if (pkt->extractAdvertiserAddress(address, &random)) {
			BLEPacket::forgeScanRequest(
					&scan_request,
					&scan_request_size,
					this->own.bytes,
					this->ownRandom,
					address,
					random
			);
			//bsp_board_led_invert(0);
			this->radio->updateTXBuffer(scan_request, scan_request_size);
			free(scan_request);
		}
	}
	// Update the packet direction
	pkt->updateSource(DIRECTION_UNKNOWN);
	// Transmit the packet to host
	this->addPacket(pkt);
}
void BLEController::advertisementSniffingProcessing(BLEPacket *pkt) {
	// Release all timers
	this->releaseTimers();

		// Check the indicators and the packet type to decide if the packet must be transmitted to host
	if (!this->follow || this->advertisementsTransmitIndicator || pkt->extractAdvertisementType() == CONNECT_REQ) {
		// Update the packet direction
		pkt->updateSource(DIRECTION_UNKNOWN);
		// Transmit the packet to host
		this->addPacket(pkt);
	}

	// We receive a CONNECT_REQ and the follow mode is enabled...
	if (pkt->extractAdvertisementType() == CONNECT_REQ && this->follow) {
		// Start following the connection
		this->followConnection(
            (pkt->extractChSel()==1)?CSA2:CSA1,
			pkt->extractHopInterval(),
			pkt->extractHopIncrement(),
			pkt->extractChannelMap(),
			pkt->extractAccessAddress(),
			pkt->extractCrcInit(),
			pkt->extractSCA(),
			pkt->extractLatency(),
            pkt->extractWindowOffset(),
            pkt->extractWindowSize()
		);
	}

}

void BLEController::advertisingIntervalEstimationProcessing(BLEPacket *pkt) {
	// If we are on the first channel (37)
	if (this->getChannel() == 37) {
		// Store the current timestamp
		this->timestampsFirstChannel[this->collectedIntervals] = pkt->getTimestamp();
		// Jumps to the last channel (39)
		this->setChannel(39);
	}
	// If we are on the last channel (39)
	else if (this->getChannel() == 39){
		// Store the current timestamp
		this->timestampsThirdChannel[this->collectedIntervals] = pkt->getTimestamp();
		// Increment the counter to indicate the next free slot
		this->collectedIntervals++;
		// If we reach the advertising report size
		if (this->collectedIntervals == ADV_REPORT_SIZE) {
			// Calculate the advertising interval estimation and transmit an advertising report to host
			this->calculateAdvertisingIntervals();
		}
		// Jumps to first channel (37)
		this->setChannel(37);
	}
}

void BLEController::connectionManagementProcessing(BLEPacket *pkt) {
	// We receive a connection update request ....
	if (pkt->isLinkLayerConnectionUpdateRequest()) {
		// prepare a new update
		this->prepareConnectionUpdate(pkt->extractInstant(),pkt->extractHopInterval(),pkt->extractWindowSize(), pkt->extractWindowOffset(),pkt->extractLatency());
	}

	// We receive a channel map request ...
	else if (pkt->isLinkLayerChannelMapRequest()) {
		// prepare a new update
		this->prepareConnectionUpdate(pkt->extractInstant(),pkt->extractChannelMap());

		// If we are currently performing a Man-in-the-Middle attack, propagates the event to the slave
		if (this->controllerState == PERFORMING_MITM) {
			uint8_t *channel_map_request;
			size_t channel_map_request_size;
			BLEPacket::forgeChannelMapRequest(&channel_map_request, &channel_map_request_size,pkt->extractInstant(),pkt->extractChannelMap());
			this->setMasterPayload(channel_map_request,channel_map_request_size);
		}

	}
    // We receive a PHY update
    else if (pkt->isLinkLayerPhyUpdateInd()) {
        // prepare a PHY update, will be applied at given instant.
        this->preparePhyUpdate(pkt->extractInstant(), pkt->extractPhyC2P(), pkt->extractPhyP2C());
    }

	// We receive a terminate ind ...
	else if (pkt->isLinkLayerTerminateInd()) {
		// Close the connection
		this->connectionLost();
	}
}


void BLEController::disconnect() {
	if (this->controllerState == SIMULATING_MASTER) {
		uint8_t *terminate_ind;
		size_t terminate_ind_size;
		BLEPacket::forgeTerminateInd(&terminate_ind, &terminate_ind_size,0x13);
		this->setMasterPayload(terminate_ind,terminate_ind_size);
		this->connectionLost();
	}
}
void BLEController::masterPacketProcessing(BLEPacket *pkt) {
	// Update master's last timestamp and control flow counters
	this->lastMasterTimestamp = pkt->getTimestamp();
	this->updateMasterSequenceNumbers(pkt->extractSN(),pkt->extractNESN());

	// Indicate that it is a master to slave packet in the direction field
	pkt->updateSource(DIRECTION_MASTER_TO_SLAVE);
}


void BLEController::slavePacketProcessing(BLEPacket *pkt) {
	// Update slave's last timestamp and control flow counters
	this->lastSlaveTimestamp = pkt->getTimestamp();
	this->updateSlaveSequenceNumbers(pkt->extractSN(),pkt->extractNESN());

	// Indicate that it is a master to slave packet in the direction field
	pkt->updateSource(DIRECTION_SLAVE_TO_MASTER);
}

void BLEController::connectionSynchronizationProcessing(BLEPacket *pkt) {
	// If we are not synchronized, we assume it is a Master packet
	if (!this->sync) {
		// Indicate that the sniffer is now synchronized
		this->sync = true;

		// Process the packet as a master's packet
		this->masterPacketProcessing(pkt);

		// Update the anchor point
		this->setAnchorPoint(pkt->getTimestamp());
	}
	else {
		int32_t relativeTimestamp = (int32_t)(pkt->getTimestamp() - this->lastAnchorPoint);
		if (relativeTimestamp < 100) {
		// If we received the packet before anchorPoint us, it's a master packet

			// Process the packet as a master's packet
			this->masterPacketProcessing(pkt);

			// Update the anchor point
			this->setAnchorPoint(pkt->getTimestamp());
		}
		else if (relativeTimestamp >= 100 && relativeTimestamp < 600) {
		// If we received the packet between anchorPoint + 100us and anchorPoint + 600 us, it's a slave packet
			if (this->controllerState != SIMULATING_SLAVE) {
				this->slavePacketProcessing(pkt);
			}
			else {
				this->masterPacketProcessing(pkt);
			}
		}
	}
}

void BLEController::attackSynchronizationProcessing(BLEPacket *pkt) {
	int32_t relativeTimestamp = (int32_t)(pkt->getTimestamp() - this->lastAnchorPoint);
	if (this->controllerState == INJECTING_TO_MASTER) {
		/*if (pkt->isEncryptionRequest()) {
			this->attackStatus.injecting = true;
		}*/
		//this->attackStatus.payload[0] = (this->attackStatus.payload[0] & 0xF3) | (this->simulatedSlaveSequenceNumbers.nesn  << 2) | (this->simulatedSlaveSequenceNumbers.sn << 3); // MD = 1
		//this->radio->updateTXBuffer(this->attackStatus.payload,this->attackStatus.size);
	}
	if (relativeTimestamp > 2*1250) {
	// If we received the packet after anchorPoint + 2*1250 us, it's a slave packet in response to our fake master packet
		this->slavePacketProcessing(pkt);

		if (this->controllerState == SYNCHRONIZING_MASTER) {
			// If we are synchronizing as master, set the new anchor point
			this->lastMasterTimestamp = pkt->getTimestamp() - 350;
			this->setAnchorPoint(this->lastMasterTimestamp);

			// Switch state to simulating master
			this->controllerState = SIMULATING_MASTER;
		}
		else if (this->controllerState == SYNCHRONIZING_MITM) {
			// If we are synchronizing to perform a MiTM, switch state to performing MiTM
			this->controllerState = PERFORMING_MITM;
		}
	}
}

void BLEController::masterSimulationControlFlowProcessing(BLEPacket *pkt) {
	// If the SN of the received packet is equals to our nesn, the slave transmits a new data
	if (this->simulatedMasterSequenceNumbers.nesn == pkt->extractSN()) {
		// Increment the local nextExpectedSeqNum counter
		this->simulatedMasterSequenceNumbers.nesn = (this->simulatedMasterSequenceNumbers.nesn + 1) % 2;
		if (pkt->extractPayloadLength() > 0) {
			this->encRxCounter++;
		}
	}
	// If the NESN of the received packet is different than our sn, the slave acknowledged our packet
	if (this->simulatedMasterSequenceNumbers.sn != pkt->extractNESN()) {
		// Increment the local sequenceNumber counter
		this->simulatedMasterSequenceNumbers.sn = (this->simulatedMasterSequenceNumbers.sn + 1) % 2;

		// We can indicate that our packet has been transmitted
		if (!this->masterPayload.transmitted) {
			this->masterPayload.lastTransmitInstant = this->connectionEventCount;
			this->masterPayload.transmitted = true;
			this->encTxCounter++;
		}

		// Retransmit the packet if we didn't got a response (if needed !) after 10 connection events
		if (this->masterPayload.transmitted && BLEPacket::needResponse(this->masterPayload.payload, this->masterPayload.size) && !this->masterPayload.responseReceived) {
			if (pkt->extractPayloadLength() > 2) {
				this->masterPayload.responseReceived = true;
			}
			else if (this->connectionEventCount >= this->masterPayload.lastTransmitInstant + 10) {
				this->masterPayload.responseReceived = false;
				this->masterPayload.transmitted = false;
			}
		}
		if (this->masterPayload.transmitted && this->masterQueueHead != this->masterQueueTail) {
			this->loadNextMasterPayload();
		}
	}
	if (this->mdSequence && this->mdCount > 0) {
			nrf_delay_us(60);
			this->temporaryPayload.payload[0] = (0x01 & 0xF3) | (this->simulatedMasterSequenceNumbers.nesn  << 2) | (this->simulatedMasterSequenceNumbers.sn << 3) | (1 << 4);
			this->temporaryPayload.payload[1] = 0x00;
			this->temporaryPayload.size = 2;
			this->simulatedMasterSequenceNumbers.sn = (this->simulatedMasterSequenceNumbers.sn + 1) % 2;
			this->simulatedMasterSequenceNumbers.nesn = (this->simulatedMasterSequenceNumbers.nesn + 1) % 2;
			this->radio->send(this->temporaryPayload.payload, this->temporaryPayload.size, BLEController::channelToFrequency(this->channel), this->channel);
			this->mdCount--;
	}
}

void BLEController::roleSimulationProcessing(BLEPacket* pkt) {
	int32_t relativeTimestamp = (int32_t)(pkt->getTimestamp() - this->lastAnchorPoint);

	// If we received a master packet and we have to simulate the slave role
	if (
			pkt->getSource() == DIRECTION_MASTER_TO_SLAVE &&
			(this->controllerState == SIMULATING_SLAVE || this->controllerState == PERFORMING_MITM)
	) {
		// Call the slave callback
		this->slaveRoleCallback(pkt);

		// Update the connection handle
		pkt->setConnectionHandle(0);
	}
	// If we received the packet after anchorPoint + 2*1250 us in MiTM mode, it's a slave packet in response to our fake master packet
	else if (relativeTimestamp > 2*1250 && this->controllerState == PERFORMING_MITM) {
		// Exit the slave mode temporarily
		this->exitSlaveMode();

		this->slavePacketProcessing(pkt);

		// Perform the control flow operations
		this->masterSimulationControlFlowProcessing(pkt);
	}
	// If we are simulating a master
	else if (this->controllerState == SIMULATING_MASTER) {
		this->slavePacketProcessing(pkt);

		// Calculate our last transmitted packet size
		uint32_t lastTransmittedPacketDuration = 0;
		if (this->masterPayload.transmitted) {
			lastTransmittedPacketDuration = 80;
		}
		else {
			lastTransmittedPacketDuration = 8 * (this->masterPayload.size + BLE_1MBPS_PREAMBLE_SIZE + ACCESS_ADDRESS_SIZE + CRC_SIZE);
		}
		if (this->packetCount == 1) {
			// Update the anchor point according to our own packet size
			this->setAnchorPoint(pkt->getTimestamp() - BLE_IFS - lastTransmittedPacketDuration);
		}
		// Perform the control flow operations
		this->masterSimulationControlFlowProcessing(pkt);
	}
}


void BLEController::connectionPacketProcessing(BLEPacket *pkt) {
	// Increment the packet counter
	this->packetCount++;


	if (this->controllerState == CONNECTION_INITIATION) {
		this->connectionInitiationConnectedProcessing(pkt);
	}
	else if (this->controllerState == CONNECTION_INITIATION_SLAVE) {
		this->connectionInitiationConnectedSlaveProcessing(pkt);
	}
	else {
		// Process packets related to connection management (requiring a fast processing)
		this->connectionManagementProcessing(pkt);

		// Process packets to manage sniffer synchronization
		this->connectionSynchronizationProcessing(pkt);

		// If we are in an attack synchronization, run the corresponding processing
		if (this->controllerState == SYNCHRONIZING_MITM || this->controllerState == SYNCHRONIZING_MASTER || this->controllerState == INJECTING_TO_MASTER) {
			this->attackSynchronizationProcessing(pkt);
		}
		// If we are in a role simulation, run the corresponding processing
		else if (
			this->controllerState == SIMULATING_MASTER ||
			this->controllerState == SIMULATING_SLAVE ||
			this->controllerState == PERFORMING_MITM
		) {
			this->roleSimulationProcessing(pkt);
		}
	}

	// Decide if the packet must be transmitted to host
	if (pkt->extractPayloadLength() > 0 || this->emptyTransmitIndicator) {
		this->addPacket(pkt);
	}
}

void BLEController::advertisementPacketProcessing(BLEPacket *pkt) {
	// If the packet doesn't match the filter, drop it
	if (this->softwareFilterEnabled && !pkt->checkAdvertiserAddress(this->filter)) return;

	if (this->controllerState == CONNECTION_INITIATION) {
		this->connectionInitiationAdvertisementProcessing(pkt);
	}

	else if (this->controllerState == SCANNING) {
		this->advertisementScanningProcessing(pkt);
	}

	else if (this->controllerState == ADVERTISING) {

		if (pkt->extractAdvertisementType() == SCAN_REQ) {
			nrf_delay_us(60);
			uint8_t *scan_rsp;
			size_t scan_rsp_size;
			BLEPacket::forgeScanResponse(&scan_rsp, &scan_rsp_size, this->own.bytes, this->ownRandom, this->advertisingData.scanData, this->advertisingData.scanDataSize, (pkt->getPacketBuffer()[4] & 0x80) >> 7);
			this->radio->send(scan_rsp, scan_rsp_size, BLEController::channelToFrequency(this->channel), this->channel);
			nrf_delay_us(8*(scan_rsp_size+4+3));
			//bsp_board_led_invert(1);
			free(scan_rsp);
		}
		else if (pkt->extractAdvertisementType() == CONNECT_REQ) {
			if (this->follow) {
				this->releaseTimers();
				// Start following the connection
				this->followConnection(
                    (pkt->extractChSel()==1)?CSA2:CSA1,
					pkt->extractHopInterval(),
					pkt->extractHopIncrement(),
					pkt->extractChannelMap(),
					pkt->extractAccessAddress(),
					pkt->extractCrcInit(),
					pkt->extractSCA(),
					pkt->extractLatency(),
                    pkt->extractWindowOffset(),
                    pkt->extractWindowSize()
				);
				this->radio->setFastRampUpTime(true);
				this->radio->reload();
				pkt->extractAdvertiserAddress(this->connectionInitiationData.responder.bytes, &this->connectionInitiationData.responderRandom);
				this->controllerState = CONNECTION_INITIATION_SLAVE;

			}
		}
		pkt->updateSource(DIRECTION_UNKNOWN);
		this->addPacket(pkt);

	}

	else if (this->controllerState == SNIFFING_ADVERTISEMENTS) {
		this->advertisementSniffingProcessing(pkt);
	}

	else if (this->controllerState == COLLECTING_ADVINTERVAL) {
		this->advertisingIntervalEstimationProcessing(pkt);
	}
}

void BLEController::accessAddressProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
    uint8_t candidate_pdu[2];
    uint32_t accessAddress = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

    /* Dewhiten the two bytes following the candidate AA (BLE PDU header) */
    candidate_pdu[0] = buffer[4];
    candidate_pdu[1] = buffer[5];
    dewhiten_ble(candidate_pdu, 2, this->channel);

    /* If the dewhitened frame header may be an empty PDU header, add AA to our candidates. */
    if (is_access_address_valid(accessAddress) && (candidate_pdu[1] == 0) && ((candidate_pdu[0]&0x3)==1)) {
        if (this->isAccessAddressKnown(accessAddress)) {
            this->sendAccessAddressReport(accessAddress, timestamp, -1 * rssi);
        }
        else {
            this->addCandidateAccessAddress(accessAddress);
        }
    } else {
        /**
         * If not, try with same buffer after two bit shifts (right).
         * (algorithm from btlejack, inspired by MouseJack).
         **/
        for (int j=0; j<2; j++)
        {
            /* Shift right. */
            for (int i=0; i<9; i++)
                buffer[i] = buffer[i]>>1 | ((buffer[i+1]&0x01) << 7);

            /* Dewhiten candidate PDU. */
            candidate_pdu[0] = buffer[4];
            candidate_pdu[1] = buffer[5];
            dewhiten_ble(candidate_pdu, 2, this->channel);

            /* Check if PDU is the one expected. */
            accessAddress = buffer[0] | buffer[1]<<8 | buffer[2]<<16 | buffer[3]<<24;
            if (is_access_address_valid(accessAddress) && (candidate_pdu[1] == 0) && ((candidate_pdu[0]&0x3)==1)) {
                if (this->isAccessAddressKnown(accessAddress)) {
                    this->sendAccessAddressReport(accessAddress, timestamp, -1 * rssi);
                }
                else {
                    this->addCandidateAccessAddress(accessAddress);
                }
            }
        }
    }
}

void BLEController::crcInitRecoveryProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
		// If we got an empty packet, extract the CRC and reverse the CRCInit
		if ((buffer[0] & 0x3) == 1 && buffer[1] == 0x00) {
			uint32_t crc = buffer[2] | (buffer[3] << 8) | (buffer[4] << 16);
			uint32_t crcInit = reverse_crc_ble(crc, buffer, 2);


			if (this->crcInit != crcInit) {
				this->crcInit = crcInit;
				this->activeConnectionRecovery.validPacketOccurences = 0;
			}
			else {
				this->activeConnectionRecovery.validPacketOccurences++;
				if (this->activeConnectionRecovery.validPacketOccurences == 3) {
					this->sendExistingConnectionReport(this->accessAddress, this->crcInit, NULL, 0, 0);
					this->recoverChannelMap(this->accessAddress, this->crcInit);
					this->start();
				}
			}
		}
}
void BLEController::channelMapRecoveryProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
	if (crcValue.validity == VALID_CRC) {
		this->activeConnectionRecovery.validPacketOccurences++;
		this->discoveryTimer->update(4000000);
		this->hopToNextDataChannel();
	}
}

void BLEController::hopIntervalRecoveryProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
	if (crcValue.validity == VALID_CRC) {
		this->activeConnectionRecovery.validPacketOccurences++;
		if (this->activeConnectionRecovery.validPacketOccurences == 1) {
			this->activeConnectionRecovery.lastTimestamp = timestamp;
		}

		else if ((timestamp - this->activeConnectionRecovery.lastTimestamp) > 1250) {
			uint16_t hopInterval = (((timestamp - this->activeConnectionRecovery.lastTimestamp)/1250)/ 37);
            //uint16_t hopInterval = DIVIDE_ROUND((timestamp - this->activeConnectionRecovery.lastTimestamp)/1250, 37);
			if (this->hopInterval != hopInterval) {
				this->hopInterval = hopInterval;
				this->activeConnectionRecovery.numberOfMeasures = 0;
			}
			else {
				this->activeConnectionRecovery.numberOfMeasures++;
				if (this->activeConnectionRecovery.numberOfMeasures > 2) {
					this->hopInterval = this->hopInterval + 1; // hop interval seems too small by one
					this->sendExistingConnectionReport(this->accessAddress, this->crcInit, this->channelMap, this->hopInterval, 0);
					this->recoverHopIncrement(this->accessAddress, this->crcInit, this->channelMap, this->hopInterval);
				}
			}
			this->activeConnectionRecovery.validPacketOccurences = 0;
		}
	}
}
void BLEController::hopIncrementRecoveryProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
	if (crcValue.validity == VALID_CRC) {

		this->activeConnectionRecovery.validPacketOccurences++;
		if (this->channel == this->activeConnectionRecovery.firstChannel) {
			this->activeConnectionRecovery.lastTimestamp = timestamp;
			this->setChannel(this->activeConnectionRecovery.secondChannel);
		}
		else if (this->channel == this->activeConnectionRecovery.secondChannel) {
			//uint16_t interval = (((timestamp - this->activeConnectionRecovery.lastTimestamp)/1250) / this->hopInterval);
            uint16_t interval = DIVIDE_ROUND((timestamp - this->activeConnectionRecovery.lastTimestamp)/1250, this->hopInterval);
			uint8_t increment = this->activeConnectionRecovery.reverseLookUpTables[interval];
			if (increment > 0) {
				/* Save recovered increment. */
                this->updateHopIncrement(increment);
                //this->hopInterval++;

                /** 
                 * Follow connection (will tune on next channel and wait for
                 * a valid PDU.
                 **/

				this->followConnection(
                    CSA1,//this->csa,
					this->hopInterval,
					this->hopIncrement,
					this->channelMap,
					this->accessAddress,
					this->crcInit,
					20,
					6,
                    0,//this->hopInterval,
                    0xff
				);

                /* Send a report when everything has been set. */
				this->sendExistingConnectionReport(this->accessAddress, this->crcInit, this->channelMap, this->hopInterval, increment);

			}
			else {
				this->setChannel(this->activeConnectionRecovery.firstChannel);
			}
		}


	}
}

void BLEController::onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi) {
	int32_t relativeTimestamp = (this->accessAddress != 0x8e89bed6 ? (int32_t)(timestamp - this->lastAnchorPoint) : 0);
	if (this->controllerState == SNIFFING_ACCESS_ADDRESS) {
		this->accessAddressProcessing(timestamp, size , buffer, crcValue, rssi);
	}
	else if (this->controllerState == RECOVERING_CRC_INIT) {
		this->crcInitRecoveryProcessing(timestamp, size, buffer, crcValue, rssi);
	}
	else if (this->controllerState == RECOVERING_CHANNEL_MAP) {
		this->channelMapRecoveryProcessing(timestamp, size, buffer, crcValue, rssi);
	}
	else if (this->controllerState == RECOVERING_HOP_INTERVAL) {
		this->hopIntervalRecoveryProcessing(timestamp, size, buffer, crcValue, rssi);
	}
	else if (this->controllerState == RECOVERING_HOP_INCREMENT) {
		this->hopIncrementRecoveryProcessing(timestamp, size, buffer ,crcValue, rssi);
	}
	else if (crcValue.validity == VALID_CRC) {
		BLEPacket *pkt = new BLEPacket(this->accessAddress,buffer, size,timestamp, relativeTimestamp, 0, this->channel,rssi, crcValue);
		if (pkt->isAdvertisement()) {
			// If the packet is an advertisement, call onAdvertisementPacket method
			this->advertisementPacketProcessing(pkt);
		}
		else {
			// If the packet is a connection packet, call onConnectionPacket
			if (this->sync) {
				this->checkSequenceReceptionTriggers(buffer, size);
			}
			this->connectionPacketProcessing(pkt);
		}
		// Delete the packet object if it is not NULL
		if (pkt != NULL) delete pkt;
	}
}

void BLEController::onJam(uint32_t timestamp) {
	if (this->controllerState == REACTIVE_JAMMING) {
	}
	else if (this->controllerState == JAMMING_CONNECT_REQ) {
		Core::instance->sendDebug("JAMMED !");
		NRF_RADIO->TASKS_STOP = 1;

		nrf_delay_us(1000);
		NRF_RADIO->TASKS_START = 1;
	}
}

void BLEController::onEnergyDetection(uint32_t timestamp, uint8_t value) {}
