#ifndef TSCH_LINK_H
#define TSCH_LINK_H

#include "whad.h"

namespace tsch {

class Link {
    public:
        Link() = default;

        Link(uint32_t src, uint32_t slot, uint32_t offset, uint32_t neighbor, whad::dot15d4::LinkOptions opts, whad::dot15d4::LinkType t);
        
        uint32_t getSrc() const;
        uint32_t getTimeSlot() const;
        uint32_t getChannelOffset() const;
        uint32_t getNeighbor() const;
        whad::dot15d4::LinkOptions getOptions() const;
        whad::dot15d4::LinkType getType() const;

    private:
        uint32_t m_src = 0;
        uint32_t m_time_slot = 0;
        uint32_t m_channel_offset = 0;
        uint32_t m_neighbor = 0;
        whad::dot15d4::LinkOptions m_options = whad::dot15d4::Unknown;
        whad::dot15d4::LinkType m_type = whad::dot15d4::Normal;
    };

}

#endif