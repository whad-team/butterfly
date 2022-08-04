#ifndef WHAD_H
#define WHAD_H

#include <stdlib.h>
#include "nrf.h"
#include "whad/nanopb/pb_encode.h"
#include "whad/nanopb/pb_decode.h"
#include "whad/callbacks/callbacks.h"
#include "version.h"
#include "capabilities.h"
#include "packet.h"

class Whad {

  public:
    static bool isDomainSupported(discovery_Domain domain);
    static uint64_t getSupportedCommandByDomain(discovery_Domain domain);

    static Message decodeMessage(uint8_t *buffer, size_t size);
    static size_t encodeMessage(Message* msg, uint8_t* buffer, size_t maxSize);

    static Message* buildMessage();

    // Generic messages builders
    static Message* buildResultMessage(generic_ResultCode code);
    static Message* buildVerboseMessage(const char* data);
    static Message* buildDiscoveryDeviceInfoMessage();
    static Message* buildDiscoveryReadyResponseMessage();
    static Message* buildDiscoveryDomainInfoMessage(discovery_Domain domain);

    static Message* buildMessageFromPacket(Packet* packet);
    static Message* buildDot15d4RawPduMessage(Dot15d4Packet* packet);
    static Message* buildBLERawPduMessage(BLEPacket* packet);
    static Message* buildBLESynchronizedMessage(uint32_t accessAddress, uint32_t crcInit, uint32_t hopInterval, uint32_t hopIncrement, uint8_t *channelMap);
    static Message* buildBLEDesynchronizedMessage(uint32_t accessAddress);
    static Message* buildBLEInjectedMessage(bool success, uint32_t injectionAttempts, uint32_t accessAddress);
    static Message* buildBLEHijackedMessage(bool success, uint32_t accessAddress);
};

#endif
