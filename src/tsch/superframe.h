#ifndef TSCH_SUPERFRAME_H
#define TSCH_SUPERFRAME_H

#include <array>
#include "link.h"

namespace tsch {

class Superframe {
public:
    static constexpr size_t MAX_LINKS = 16;

    Superframe() = default;
    
    Superframe(uint32_t id, uint32_t numberOfSlots, uint32_t flags);

    bool addLink(const Link& link);
    bool deleteLink(uint32_t timeSlot, uint32_t channelOffset);

    uint32_t getId() const;
    
    uint32_t getNumberOfSlots() const;
    void setNumberOfSlots(uint32_t slots);
    
    uint32_t getFlags() const;
    void setFlags(uint32_t flags);
    
    size_t getLinkCount() const;
    const Link& getLinkAt(size_t index) const;

private:
    uint32_t m_id = 0;
    uint32_t m_number_of_slots = 0;
    uint32_t m_flags = 0;
    
    std::array<Link, MAX_LINKS> m_links;
    size_t m_link_count = 0;
};

}

#endif