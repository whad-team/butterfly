#include "trigger.h"

Trigger::Trigger(TriggerType type) {
    this->type = type;
    this->triggered = false;
}

bool Trigger::isTriggered() {
    return this->triggered;
}

void Trigger::trigger() {
    this->triggered = true;
}

TriggerType Trigger::getType() {
    return this->type;
}

ReceptionTrigger::ReceptionTrigger(uint8_t *pattern, uint8_t *mask, size_t patternSize, uint8_t offset) : Trigger(RECEPTION_TRIGGER) {
    this->pattern = (uint8_t*)malloc(patternSize);
    this->mask = (uint8_t*)malloc(patternSize);
    memcpy(this->pattern, pattern, patternSize);
    memcpy(this->mask, mask, patternSize);
    this->patternSize = patternSize;
    this->offset = offset;
}

ReceptionTrigger::~ReceptionTrigger() {
    free(this->pattern);
    free(this->mask);
}

bool ReceptionTrigger::evaluate(uint8_t *payload, size_t payloadSize) {
    if (payloadSize < this->offset + this->patternSize) {
        return false;
    }

    for (size_t i=0;i<this->patternSize; i++) {
        if ((payload[this->offset + i] & this->mask[i]) != (this->pattern[i] & this->mask[i])) {
            return false;
        }
    }
    return true;
}

ConnectionEventTrigger::ConnectionEventTrigger(uint16_t connectionEvent) : Trigger(CONNECTION_EVENT_TRIGGER) {
    this->connectionEvent = connectionEvent;
}

bool ConnectionEventTrigger::evaluate(uint16_t connectionEvent) {
    return connectionEvent == this->connectionEvent;
}

ManualTrigger::ManualTrigger() : Trigger(MANUAL_TRIGGER) {}
