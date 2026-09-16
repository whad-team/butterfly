#include "network.h"

namespace tsch {

    Network::Network()
{
    this->m_asn = 0;
    this->m_correction_asn = 0;
    this->m_channel_map = 0;
    this->m_start_of_slot_timestamp = 0;
    this->m_channel_bitmap = std::bitset<16>(0);
    this->m_channel_count = 0;
    this->m_superframe_count = 0;
    this->m_pan_id = 0;
    this->m_default_channel = 12;
    this->m_last_sync_asn = 0;
    this->m_last_sync_timestamp = 0;
    
}

void Network::reset()
{
    this->m_asn = 0;
    this->m_correction_asn = 0;
    this->m_channel_map = 0;
    this->m_start_of_slot_timestamp = 0;
    this->m_channel_bitmap = std::bitset<16>(0);
    this->m_channel_count = 0;
    this->m_superframe_count = 0;
    this->m_pan_id = 0;
    this->m_default_channel = 12;
    this->m_last_sync_asn = 0;
    this->m_last_sync_timestamp = 0;

}

void Network::setLastSync(uint64_t asn, uint32_t timestamp) {
    this->m_last_sync_asn = asn;
    this->m_last_sync_timestamp = timestamp;
}

uint32_t Network::getLastSyncTimestamp() {
    return this->m_last_sync_timestamp;
}

uint64_t Network::getLastSyncASN() {
    return this->m_last_sync_asn;
}

void Network::setChannelMap(uint16_t map) {
    this->m_channel_map = map;
    this->m_channel_bitmap = std::bitset<16>(m_channel_map);
    this->countActiveChannels();
}

uint16_t Network::getChannelMap() const {
    return this->m_channel_map;
}

uint32_t Network::getActiveChannel(uint32_t index) {
    if (index >= 0 && index < this->m_channel_count)
    {
        for (uint32_t i = 0; i < 16; i++)
        {
            if (this->m_channel_bitmap[i])
            {
                if (index == 0)
                    return i;
                else
                    index -= 1;
            }
        }
    }
    
    for (uint32_t i = 0; i < 16; i++) {
        if (this->m_channel_bitmap[i]) return i;
    }
    
    return 0;
}

void Network::countActiveChannels() {
    uint32_t nb = 0;
    for (uint32_t i = 0; i < 16; i++)
    {
        if (this->m_channel_bitmap[i])
        {
            nb++;
        }
    }
    this->m_channel_count = nb;
}

uint32_t Network::getNumberOfActiveChannels() {
    return this->m_channel_count;
}


void Network::setDefaultChannel(uint32_t channel)
{
    this->m_default_channel = channel;
}

uint32_t Network::getDefaultChannel() 
{
    return this->m_default_channel;
}


void Network::setStartOfSlotTimestamp(uint32_t timestamp)
{
    this->m_start_of_slot_timestamp = timestamp;
}

uint32_t Network::getStartOfSlotTimestamp() const
{
    return this->m_start_of_slot_timestamp;
}

void Network::setAsn(uint64_t asn)
{
    this->m_asn = asn;
}

uint64_t Network::getAsn() const
{
    return this->m_asn;
}

void Network::setPanId(uint16_t pan_id)
{
    this->m_pan_id = pan_id;
}

uint16_t Network::getPanId() {
    return this->m_pan_id;
}

void Network::setCorrectionAsn(uint64_t asn)
{
    this->m_correction_asn = asn;
}

uint64_t Network::getCorrectionAsn() const
{
    return this->m_correction_asn;
}


bool Network::updateSuperframe(uint32_t id, uint32_t slots, uint32_t flags)
{
    Superframe* sf = this->findSuperframe(id);
    if (sf != nullptr)
    {
        sf->setNumberOfSlots(slots);
        sf->setFlags(flags);
        return true;
    }

    if (this->m_superframe_count >= MAX_SUPERFRAMES)
    {
        return false;
    }

    this->m_superframes[this->m_superframe_count] = Superframe(id, slots, flags);
    this->m_superframe_count++;
    return true;
}

bool Network::deleteSuperframe(uint32_t id)
{
    for (size_t i = 0; i < this->m_superframe_count; ++i)
    {
        if (this->m_superframes[i].getId() == id)
        {
            // Swap-to-delete : écrase par la dernière pour rester contigu en mémoire
            this->m_superframes[i] = this->m_superframes[this->m_superframe_count - 1];
            this->m_superframe_count--;
            return true;
        }
    }
    return false;
}

Superframe* Network::findSuperframe(uint32_t id)
{
    for (size_t i = 0; i < this->m_superframe_count; ++i)
    {
        if (this->m_superframes[i].getId() == id)
        {
            return &(this->m_superframes[i]);
        }
    }
    return nullptr;
}

size_t Network::getSuperframeCount() const
{
    return this->m_superframe_count;
}

uint32_t Network::getChannelOffset() const {
    uint64_t currentAsn = this->getAsn();

    for (size_t i = 0; i < this->m_superframe_count; i++) {
        const tsch::Superframe& sf = this->m_superframes[i];

        if (sf.getNumberOfSlots() == 0) continue;

        uint32_t targetSlot = (uint32_t)(currentAsn % sf.getNumberOfSlots());

        for (size_t j = 0; j < sf.getLinkCount(); j++) {
            const tsch::Link& link = sf.getLinkAt(j);

            if (link.getTimeSlot() == targetSlot) {
                return link.getChannelOffset();
            }
        }
    }

    return CHANNEL_OFFSET_NOT_DEFINED;
}

}