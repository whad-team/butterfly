#ifndef SEQUENCE_H
#define SEQUENCE_H
#include "trigger.h"

typedef struct PendingPacket {
    uint8_t *packet;
    size_t packetSize;
    bool updateHeader;
    bool processed;
} PendingPacket;

typedef enum SequenceDirection {
  BLE_TO_SLAVE,
  BLE_TO_MASTER
} SequenceDirection;
class PacketSequence {
    protected:
        PendingPacket* sequence;
        size_t sequenceSize;
        uint8_t sequenceIndex;
        Trigger *trigger;
        bool ready;
        SequenceDirection direction;

    public:
        PacketSequence(size_t sequenceSize, Trigger *trigger, SequenceDirection direction);
        SequenceDirection getDirection();
        bool isReady();
        bool preparePacket(uint8_t *packet, size_t packetSize, bool updateHeader);
        void processPacket(uint8_t **packet, size_t *packetSize, bool* updateHeader);
        bool isTriggered();
        bool isTerminated();
        Trigger *getTrigger();
        ~PacketSequence();

};

#endif
