#include "link.h"

namespace tsch {

    Link::Link(uint32_t src, uint32_t slot, uint32_t offset, uint32_t neighbor, 
           whad::dot15d4::LinkOptions opts, whad::dot15d4::LinkType t)
{
    this->m_src            = src;
    this->m_time_slot      = slot;
    this->m_channel_offset = offset;
    this->m_neighbor       = neighbor;
    this->m_options        = opts;
    this->m_type           = t;
}

uint32_t Link::getSrc() const 
{ 
    return this->m_src; 
}

uint32_t Link::getTimeSlot() const 
{ 
    return this->m_time_slot; 
}

uint32_t Link::getChannelOffset() const 
{ 
    return this->m_channel_offset; 
}

uint32_t Link::getNeighbor() const 
{ 
    return this->m_neighbor; 
}

whad::dot15d4::LinkOptions Link::getOptions() const 
{ 
    return this->m_options; 
}

whad::dot15d4::LinkType Link::getType() const 
{ 
    return this->m_type; 
}
}