#include "sequence.h"

PacketSequence::PacketSequence(size_t sequenceSize, Trigger *trigger, SequenceDirection direction) {
    this->sequenceSize = sequenceSize;
    this->sequence = (PendingPacket*)malloc(sizeof(PendingPacket) * sequenceSize);
    this->sequenceIndex = 0;
    this->trigger = trigger;
    this->ready = false;
    this->direction = direction;
}

bool PacketSequence::isReady() {
  return this->ready;
}

bool PacketSequence::preparePacket(uint8_t *packet, size_t packetSize, bool updateHeader) {
    this->sequence[this->sequenceIndex].packet = (uint8_t*)malloc(packetSize);
    this->sequence[this->sequenceIndex].packetSize = packetSize;
    this->sequence[this->sequenceIndex].updateHeader = updateHeader;
    this->sequence[this->sequenceIndex].processed = false;
    this->sequenceIndex++;
    if (this->sequenceIndex == this->sequenceSize) {
        this->sequenceIndex = 0;
        this->ready = true;
        return true;
    }
    return false;
}
SequenceDirection PacketSequence::getDirection() {
  return this->direction;
}

Trigger *PacketSequence::getTrigger() {
    return this->trigger;
}
bool PacketSequence::isTriggered() {
    return this->trigger->isTriggered();
}
bool PacketSequence::isTerminated() {
    for (size_t i=0;i<this->sequenceSize; i++) {
        if (!this->sequence[i].processed) return false;
    }
    return true;
}

void PacketSequence::processPacket(uint8_t **packet, size_t *packetSize, bool* updateHeader) {
    if (this->sequenceIndex >= this->sequenceSize) {
        (*packet) = NULL;
        (*packetSize) = 0;
        (*updateHeader) = false;
    }

    (*packetSize) = this->sequence[this->sequenceIndex].packetSize;
    (*packet) = (uint8_t*)malloc(*packetSize);
    memcpy(*packet, this->sequence[this->sequenceIndex].packet, this->sequence[this->sequenceIndex].packetSize);
    (*updateHeader) = this->sequence[this->sequenceIndex].updateHeader;
    this->sequence[this->sequenceIndex].processed = true;
    this->sequenceIndex++;
}

PacketSequence::~PacketSequence() {
    for (size_t i=0;i<this->sequenceSize; i++) {
        free(this->sequence[i].packet);
    }
    free(this->sequence);
    delete this->trigger;
}
