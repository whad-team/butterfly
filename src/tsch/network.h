#ifndef TSCH_NETWORK_H
#define TSCH_NETWORK_H

#include <array>
#include <bitset>
#include "superframe.h"

#define CHANNEL_OFFSET_NOT_DEFINED -1

namespace tsch {

class Network {
public:
    static constexpr size_t MAX_SUPERFRAMES = 12;

    Network();

    void reset();

    void setLastSync(uint64_t asn, uint32_t timestamp);
    uint32_t getLastSyncTimestamp();
    uint64_t getLastSyncASN();
    
    void setChannelMap(uint16_t map);
    uint16_t getChannelMap() const;
    uint32_t getActiveChannel(uint32_t index);
    void countActiveChannels();
    uint32_t getNumberOfActiveChannels();

    void setPanId(uint16_t panId);
    uint16_t getPanId();

    uint32_t getChannelOffset() const;

    void setStartOfSlotTimestamp(uint32_t timestamp);
    uint32_t getStartOfSlotTimestamp() const;

    void setAsn(uint64_t asn);
    uint64_t getAsn() const;
    void setCorrectionAsn(uint64_t asn);
    uint64_t getCorrectionAsn() const;

    bool updateSuperframe(uint32_t id, uint32_t slots, uint32_t flags);
    bool deleteSuperframe(uint32_t id);
    
    Superframe* findSuperframe(uint32_t id);
    size_t getSuperframeCount() const;

    void setDefaultChannel(uint32_t channel);
    uint32_t getDefaultChannel();

private:
    uint32_t m_default_channel = 0;


    uint64_t m_last_sync_asn = 0;
    uint32_t m_last_sync_timestamp = 0;


    uint32_t m_start_of_slot_timestamp = 0;

    uint64_t m_asn = 0;
    uint64_t m_correction_asn = 0;

    uint16_t m_pan_id = 0;

    uint16_t m_channel_map = 0;
    uint32_t m_channel_count = 0;
    std::bitset<16> m_channel_bitmap = std::bitset<16>(0);

    std::array<Superframe, MAX_SUPERFRAMES> m_superframes;
    size_t m_superframe_count = 0;
};

}

#endif