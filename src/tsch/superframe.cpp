#include "superframe.h"

namespace tsch {

Superframe::Superframe(uint32_t id, uint32_t numberOfSlots, uint32_t flags)
{
    this->m_id              = id;
    this->m_number_of_slots = numberOfSlots;
    this->m_flags           = flags;
    this->m_link_count      = 0;
}

bool Superframe::addLink(const Link& link)
{
    if (this->m_link_count >= MAX_LINKS)
    {
        return false;
    }
    
    this->m_links[this->m_link_count] = link;
    this->m_link_count++;
    return true;
}

bool Superframe::deleteLink(uint32_t timeSlot, uint32_t channelOffset)
{
    for (size_t i = 0; i < this->m_link_count; ++i)
    {
        if (this->m_links[i].getTimeSlot() == timeSlot && this->m_links[i].getChannelOffset() == channelOffset)
        {
            this->m_links[i] = this->m_links[this->m_link_count - 1];
            this->m_link_count--;
            return true;
        }
    }
    return false;
}

uint32_t Superframe::getId() const
{
    return this->m_id;
}

uint32_t Superframe::getNumberOfSlots() const
{
    return this->m_number_of_slots;
}

void Superframe::setNumberOfSlots(uint32_t slots)
{
    this->m_number_of_slots = slots;
}

uint32_t Superframe::getFlags() const
{
    return this->m_flags;
}

void Superframe::setFlags(uint32_t flags)
{
    this->m_flags = flags;
}

size_t Superframe::getLinkCount() const
{
    return this->m_link_count;
}

const Link& Superframe::getLinkAt(size_t index) const
{
    return this->m_links[index];
}

}