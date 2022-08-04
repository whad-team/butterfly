#ifndef PBCALLBACKS_H
#define PBCALLBACKS_H
#include "stddef.h"
#include "stdbool.h"
#include "whad/nanopb/pb.h"
#include "whad/nanopb/pb_encode.h"
#include "whad/protocol/whad.pb.h"
#include "whad/protocol/device.pb.h"
#include "capabilities.h"

bool whad_verbose_msg_encode_cb(pb_ostream_t *ostream, const pb_field_t *field, void * const *arg);
bool whad_disc_enum_capabilities_cb(pb_ostream_t *ostream, const pb_field_t *field, void * const *arg);
#endif
