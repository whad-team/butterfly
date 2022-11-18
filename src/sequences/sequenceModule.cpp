#include "sequenceModule.h"

SequenceModule::SequenceModule() {
    for (int i=0;i<MAX_SEQUENCES;i++) {
        this->sequences[i] = NULL;
    }
}

PacketSequence* SequenceModule::createSequence(size_t size, Trigger* trigger, SequenceDirection direction) {
    int selectedSpot = -1;
    for (int i=0; i<MAX_SEQUENCES;i++) {
        if (this->sequences[i] == NULL) {
            selectedSpot = i;
            break;
        }
    }
    if (selectedSpot != -1) {
        this->sequences[selectedSpot] = new PacketSequence(size, trigger, direction);
        this->sequences[selectedSpot]->setIdentifier(selectedSpot);
        return this->sequences[selectedSpot];
    }
    else {
        return NULL;
    }
}

void SequenceModule::deleteSequence(PacketSequence* sequence) {
    for (int i=0;i<MAX_SEQUENCES;i++) {
        if (sequence == this->sequences[i]) {
            delete this->sequences[i];
            this->sequences[i] = NULL;
            break;
        }
    }
}

void SequenceModule::reset() {
    for (int i=0;i<MAX_SEQUENCES;i++) {
        if (this->sequences[i] != NULL) {
          delete this->sequences[i];
          this->sequences[i] = NULL;
        }
    }
}
