#ifndef RADIO_DESC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "radio_defs.h"

#define MAX_PAYLOAD_SIZE 257

typedef enum {
    DESC_FREE,      /* Descriptor is free. */
    DESC_PENDING,   /* Descriptor ready to be processed by controller (pre-hook). */
    DESC_READY,     /* Descriptor ready to be processed by radio. */
    DESC_DONE       /* Descriptor successfully sent/received, can be post-processed and freed. */
} radio_desc_state_t;

typedef struct _radio_desc_t {
    /* Current descriptor state. */
    radio_desc_state_t state;

    /* Payload buffer. */
    uint8_t payload[MAX_PAYLOAD_SIZE];
    size_t size;

    /* Metadata. */
    CrcValue crc;
    uint8_t rssi;
} radio_desc_t;

#endif /* RADIO_DESC_H */
