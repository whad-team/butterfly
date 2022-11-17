#ifndef TRIGGER_H
#define TRIGGER_H
#include "stdint.h"
#include "string.h"
#include "stdlib.h"

typedef enum TriggerType {
    RECEPTION_TRIGGER, 
    MANUAL_TRIGGER, 
    CONNECTION_EVENT_TRIGGER
} TriggerType;

class Trigger {
    protected:
        bool triggered;
        TriggerType type;
        
    public:
        Trigger(TriggerType triggerType);
        bool isTriggered();
        void trigger();
        TriggerType getType();
};

class ReceptionTrigger : public Trigger {
    protected:
        uint8_t *pattern;
        uint8_t *mask;
        size_t patternSize;
        uint8_t offset;
        
    public:
        ReceptionTrigger(uint8_t *pattern, uint8_t *mask, size_t patternSize, uint8_t offset);
        bool evaluate(uint8_t *payload, size_t payloadSize);
        ~ReceptionTrigger();
};


class ManualTrigger : public Trigger {
        ManualTrigger();
};

class ConnectionEventTrigger : public Trigger {
    protected:
        uint16_t connectionEvent;
        
    public:
        ConnectionEventTrigger(uint16_t connectionEvent);
        bool evaluate(uint16_t connectionEvent);
};

#endif
