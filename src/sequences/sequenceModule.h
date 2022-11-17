#ifndef SEQUENCE_MODULE_H
#define SEQUENCE_MODULE_H
#include "stddef.h"
#include "sequence.h"
#define MAX_SEQUENCES 10

class SequenceModule {


    public:
        PacketSequence* sequences[MAX_SEQUENCES];

        SequenceModule();
        PacketSequence* createSequence(size_t size, Trigger* trigger, SequenceDirection direction);
        void deleteSequence(PacketSequence* sequence);
        void reset();
};

#endif
