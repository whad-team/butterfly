#include "whad/whad.h"


bool Whad::isDomainSupported(discovery_Domain domain) {
  bool found = false;
  int index = 0;
  while (CAPABILITIES[index].domain != 0 && CAPABILITIES[index].cap != 0) {
    if (CAPABILITIES[index].domain == domain) {
      found = true;
      break;
    }
    index++;
  }
  return found;
}

uint64_t Whad::getSupportedCommandByDomain(discovery_Domain domain) {
  uint64_t supportedCommands = 0x00000000;
  int index = 0;
  while (CAPABILITIES[index].domain != 0) {
    if (CAPABILITIES[index].domain == domain) {
      supportedCommands = CAPABILITIES[index].supported_commands;
      break;
    }
    index++;
  }
  return supportedCommands;
}

Message Whad::decodeMessage(uint8_t *buffer, size_t size) {
  Message msg = {};
  pb_istream_t stream = pb_istream_from_buffer(buffer, size);
  pb_decode(&stream, Message_fields, &msg);
  return msg;
}

size_t Whad::encodeMessage(Message* msg, uint8_t* buffer, size_t maxSize) {
  pb_ostream_t stream = pb_ostream_from_buffer(buffer, maxSize);
  pb_encode(&stream, Message_fields, msg);
  return stream.bytes_written;
}

Message* Whad::buildMessage() {
  Message *msg = (Message*)malloc(sizeof(Message));
  return msg;
}

Message* Whad::buildVerboseMessage(const char* data) {
  char* ndata = (char*)malloc(strlen(data)+1);
  strcpy(ndata, data);
  Message* msg = Whad::buildMessage();
  /* Specify payload type. */
  msg->which_msg = Message_generic_tag;

  /* Fills verbose message data. */
  msg->msg.generic.which_msg = generic_Message_verbose_tag;
  msg->msg.generic.msg.verbose.data.arg = ndata;
  msg->msg.generic.msg.verbose.data.funcs.encode = whad_verbose_msg_encode_cb;
  return msg;
}

Message* Whad::buildResultMessage(generic_ResultCode code) {
  Message *msg = Whad::buildMessage();
  msg->which_msg = Message_generic_tag;
  msg->msg.generic.which_msg = generic_Message_cmd_result_tag;
  msg->msg.generic.msg.cmd_result.result = code;
  return msg;
}

Message* Whad::buildDiscoveryReadyResponseMessage() {
  Message *msg = Whad::buildMessage();
  msg->which_msg = Message_discovery_tag;
  msg->msg.discovery.which_msg = discovery_Message_ready_resp_tag;
  return msg;
}

Message* Whad::buildDiscoveryDeviceInfoMessage() {
  Message* msg = Whad::buildMessage();
  msg->which_msg = Message_discovery_tag;
  msg->msg.discovery.which_msg = discovery_Message_info_resp_tag;
  msg->msg.discovery.msg.info_resp.devid[0] = NRF_FICR->DEVICEID[0] & 0x000000FF;
  msg->msg.discovery.msg.info_resp.devid[1] = (NRF_FICR->DEVICEID[0] & 0x0000FF00) >> 8;
  msg->msg.discovery.msg.info_resp.devid[2] = (NRF_FICR->DEVICEID[0] & 0x00FF0000) >> 16;
  msg->msg.discovery.msg.info_resp.devid[3] = (NRF_FICR->DEVICEID[0] & 0xFF000000) >> 24;
  msg->msg.discovery.msg.info_resp.devid[4] = NRF_FICR->DEVICEID[1] & 0x000000FF;
  msg->msg.discovery.msg.info_resp.devid[5] = (NRF_FICR->DEVICEID[1] & 0x0000FF00) >> 8;
  msg->msg.discovery.msg.info_resp.devid[6] = (NRF_FICR->DEVICEID[1] & 0x00FF0000) >> 16;
  msg->msg.discovery.msg.info_resp.devid[7] = (NRF_FICR->DEVICEID[1] & 0xFF000000) >> 24;
  msg->msg.discovery.msg.info_resp.devid[8] = NRF_FICR->DEVICEADDR[0] & 0x000000FF;
  msg->msg.discovery.msg.info_resp.devid[9] = (NRF_FICR->DEVICEADDR[0] & 0x0000FF00) >> 8;
  msg->msg.discovery.msg.info_resp.devid[10] = (NRF_FICR->DEVICEADDR[0] & 0x00FF0000) >> 16;
  msg->msg.discovery.msg.info_resp.devid[11] = (NRF_FICR->DEVICEADDR[0] & 0xFF000000) >> 24;
  msg->msg.discovery.msg.info_resp.devid[12] = NRF_FICR->DEVICEADDR[1] & 0x000000FF;
  msg->msg.discovery.msg.info_resp.devid[13] = (NRF_FICR->DEVICEADDR[1] & 0x0000FF00) >> 8;
  msg->msg.discovery.msg.info_resp.devid[14] = (NRF_FICR->DEVICEADDR[1] & 0x00FF0000) >> 16;
  msg->msg.discovery.msg.info_resp.devid[15] = (NRF_FICR->DEVICEADDR[1] & 0xFF000000) >> 24;

  msg->msg.discovery.msg.info_resp.fw_author.size = strlen(FIRMWARE_AUTHOR);
  memcpy(msg->msg.discovery.msg.info_resp.fw_author.bytes, FIRMWARE_AUTHOR, strlen(FIRMWARE_AUTHOR));

  msg->msg.discovery.msg.info_resp.fw_url.size = strlen(FIRMWARE_URL);
  memcpy(msg->msg.discovery.msg.info_resp.fw_url.bytes, FIRMWARE_URL, strlen(FIRMWARE_URL));

  msg->msg.discovery.msg.info_resp.max_speed = 115200;
  msg->msg.discovery.msg.info_resp.proto_min_ver = WHAD_MIN_VERSION;
  msg->msg.discovery.msg.info_resp.fw_version_major = VERSION_MAJOR;
  msg->msg.discovery.msg.info_resp.fw_version_minor = VERSION_MINOR;
  msg->msg.discovery.msg.info_resp.fw_version_rev = VERSION_REVISION;
  msg->msg.discovery.msg.info_resp.type = discovery_DeviceType_Butterfly;
  msg->msg.discovery.msg.info_resp.capabilities.arg = (DeviceCapability*)CAPABILITIES;
  msg->msg.discovery.msg.info_resp.capabilities.funcs.encode = whad_disc_enum_capabilities_cb;
  return msg;
}



Message* Whad::buildDiscoveryDomainInfoMessage(discovery_Domain domain) {
  Message *msg = Whad::buildMessage();
  msg->which_msg = Message_discovery_tag;
  msg->msg.discovery.which_msg = discovery_Message_domain_resp_tag;
  msg->msg.discovery.msg.domain_resp.domain = domain;
  msg->msg.discovery.msg.domain_resp.supported_commands = Whad::getSupportedCommandByDomain(domain);
  return msg;
}

Message* Whad::buildMessageFromPacket(Packet* packet) {
  Message *msg = NULL;
  if (packet->getPacketType() == BLE_PACKET_TYPE) {
    msg = Whad::buildBLERawPduMessage((BLEPacket*)packet);
  }
  else if (packet->getPacketType() == DOT15D4_PACKET_TYPE) {
    msg = Whad::buildDot15d4RawPduMessage((Dot15d4Packet*)packet);
  }
  else if (packet->getPacketType() == ESB_PACKET_TYPE) {
    msg = Whad::buildESBRawPduMessage((ESBPacket*)packet);
  }
  else if (packet->getPacketType() == GENERIC_PACKET_TYPE) {
    msg = Whad::buildPhyPacketMessage((GenericPacket*)packet);
  }
  return msg;
}

Message* Whad::buildPhyPacketMessage(GenericPacket* packet) {
  Message *msg = Whad::buildMessage();
  msg->which_msg = Message_phy_tag;
  msg->msg.phy.which_msg = phy_Message_packet_tag;
  msg->msg.phy.msg.packet.has_rssi = true;
  msg->msg.phy.msg.packet.rssi = packet->getRssi();
  msg->msg.phy.msg.packet.frequency = 2400 + packet->getChannel();
  msg->msg.phy.msg.packet.has_timestamp = true;
  msg->msg.phy.msg.packet.timestamp = packet->getTimestamp();
  msg->msg.phy.msg.packet.packet.size = packet->getPacketSize();
  memcpy(msg->msg.phy.msg.packet.packet.bytes, packet->getPacketBuffer(), packet->getPacketSize());
  return msg;
}

Message* Whad::buildESBRawPduMessage(ESBPacket* packet) {
  Message* msg = Whad::buildMessage();
  if (packet->isUnifying()) {
    msg->which_msg = Message_unifying_tag;
    msg->msg.unifying.which_msg = unifying_Message_raw_pdu_tag;
    msg->msg.unifying.msg.raw_pdu.has_rssi = true;
    msg->msg.unifying.msg.raw_pdu.rssi = packet->getRssi();
    msg->msg.unifying.msg.raw_pdu.channel = packet->getChannel();
    msg->msg.unifying.msg.raw_pdu.has_timestamp = true;
    msg->msg.unifying.msg.raw_pdu.timestamp = packet->getTimestamp();
    msg->msg.unifying.msg.raw_pdu.has_crc_validity = true;
    msg->msg.unifying.msg.raw_pdu.crc_validity = packet->isCrcValid();
    msg->msg.unifying.msg.raw_pdu.has_address = true;
    msg->msg.unifying.msg.raw_pdu.address.size = 5;
    memcpy(msg->msg.unifying.msg.raw_pdu.address.bytes, packet->getAddress(), 5);

    msg->msg.unifying.msg.raw_pdu.pdu.size = packet->getPacketSize();
    memcpy(msg->msg.unifying.msg.raw_pdu.pdu.bytes, packet->getPacketBuffer(), packet->getPacketSize());

  }
  else {
    msg->which_msg = Message_esb_tag;
    msg->msg.esb.which_msg = esb_Message_raw_pdu_tag;
    msg->msg.esb.msg.raw_pdu.has_rssi = true;
    msg->msg.esb.msg.raw_pdu.rssi = packet->getRssi();
    msg->msg.esb.msg.raw_pdu.channel = packet->getChannel();
    msg->msg.esb.msg.raw_pdu.has_timestamp = true;
    msg->msg.esb.msg.raw_pdu.timestamp = packet->getTimestamp();
    msg->msg.esb.msg.raw_pdu.has_crc_validity = true;
    msg->msg.esb.msg.raw_pdu.crc_validity = packet->isCrcValid();
    msg->msg.esb.msg.raw_pdu.has_address = true;
    msg->msg.esb.msg.raw_pdu.address.size = 5;
    memcpy(msg->msg.esb.msg.raw_pdu.address.bytes, packet->getAddress(), 5);

    msg->msg.esb.msg.raw_pdu.pdu.size = packet->getPacketSize();
    memcpy(msg->msg.esb.msg.raw_pdu.pdu.bytes, packet->getPacketBuffer(), packet->getPacketSize());
  }
  return msg;

}


Message* Whad::buildDot15d4RawPduMessage(Dot15d4Packet* packet) {
  Message* msg = Whad::buildMessage();
  msg->which_msg = Message_zigbee_tag;
  msg->msg.zigbee.which_msg = zigbee_Message_raw_pdu_tag;
  msg->msg.zigbee.msg.raw_pdu.has_rssi = true;
  msg->msg.zigbee.msg.raw_pdu.rssi = packet->getRssi();
  msg->msg.zigbee.msg.raw_pdu.channel = packet->getChannel();
  msg->msg.zigbee.msg.raw_pdu.has_timestamp = true;
  msg->msg.zigbee.msg.raw_pdu.timestamp = packet->getTimestamp();
  msg->msg.zigbee.msg.raw_pdu.has_fcs_validity = true;
  msg->msg.zigbee.msg.raw_pdu.fcs_validity = packet->isCrcValid();
  msg->msg.zigbee.msg.raw_pdu.has_lqi = true;
  msg->msg.zigbee.msg.raw_pdu.lqi = packet->getLQI();
  msg->msg.zigbee.msg.raw_pdu.pdu.size = packet->getPacketSize()-3;
  memcpy(msg->msg.zigbee.msg.raw_pdu.pdu.bytes, packet->getPacketBuffer()+1, packet->getPacketSize()-3);
  msg->msg.zigbee.msg.raw_pdu.fcs = packet->getFCS();
  //msg->msg.zigbee.msg.raw_pdu.processed = false;
  return msg;

}

Message* Whad::buildDot15d4EnergyDetectionSampleMessage(uint32_t sample, uint32_t timestamp) {
  Message* msg = Whad::buildMessage();
  msg->which_msg = Message_zigbee_tag;
  msg->msg.zigbee.which_msg = zigbee_Message_ed_sample_tag;
  msg->msg.zigbee.msg.ed_sample.timestamp = timestamp;
  msg->msg.zigbee.msg.ed_sample.sample = sample;
  return msg;

}
Message* Whad::buildBLERawPduMessage(BLEPacket* packet) {
  Message* msg = Whad::buildMessage();
  msg->which_msg = Message_ble_tag;
  msg->msg.ble.which_msg = ble_Message_raw_pdu_tag;
  msg->msg.ble.msg.raw_pdu.direction = (ble_BleDirection)packet->getSource();
  msg->msg.ble.msg.raw_pdu.has_rssi = true;
  msg->msg.ble.msg.raw_pdu.rssi = packet->getRssi();
  msg->msg.ble.msg.raw_pdu.channel = packet->getChannel();
  msg->msg.ble.msg.raw_pdu.has_timestamp = true;
  msg->msg.ble.msg.raw_pdu.timestamp = packet->getTimestamp();
  if (packet->getAccessAddress() != 0x8e89bed6) {
    msg->msg.ble.msg.raw_pdu.has_relative_timestamp = true;
    msg->msg.ble.msg.raw_pdu.relative_timestamp = packet->getRelativeTimestamp();
  }
  else {
    msg->msg.ble.msg.raw_pdu.has_relative_timestamp = false;
  }
  msg->msg.ble.msg.raw_pdu.has_crc_validity = true;
  msg->msg.ble.msg.raw_pdu.crc_validity = packet->isCrcValid();
  msg->msg.ble.msg.raw_pdu.access_address = packet->getAccessAddress();
  msg->msg.ble.msg.raw_pdu.pdu.size = packet->extractPayloadLength() + 2;
  memcpy(msg->msg.ble.msg.raw_pdu.pdu.bytes, packet->getPacketBuffer()+4, packet->extractPayloadLength() + 2);
  msg->msg.ble.msg.raw_pdu.crc = packet->getCrc();
  msg->msg.ble.msg.raw_pdu.conn_handle = packet->getConnectionHandle();
  msg->msg.ble.msg.raw_pdu.processed = false;
  return msg;
}

Message* Whad::buildBLESynchronizedMessage(uint32_t accessAddress, uint32_t crcInit, uint32_t hopInterval, uint32_t hopIncrement, uint8_t *channelMap) {
  Message* msg = Whad::buildMessage();
  msg->which_msg = Message_ble_tag;
  msg->msg.ble.which_msg = ble_Message_synchronized_tag;
  msg->msg.ble.msg.synchronized.access_address = accessAddress;
  msg->msg.ble.msg.synchronized.crc_init = crcInit;
  msg->msg.ble.msg.synchronized.hop_interval = hopInterval;
  msg->msg.ble.msg.synchronized.hop_increment = hopIncrement;
  if (channelMap != NULL) {
    memcpy(msg->msg.ble.msg.synchronized.channel_map, channelMap, 5);
  }
  else {
    msg->msg.ble.msg.synchronized.channel_map[0] = 0;
    msg->msg.ble.msg.synchronized.channel_map[1] = 0;
    msg->msg.ble.msg.synchronized.channel_map[2] = 0;
    msg->msg.ble.msg.synchronized.channel_map[3] = 0;
    msg->msg.ble.msg.synchronized.channel_map[4] = 0;
  }
  return msg;
}
Message* Whad::buildBLEDesynchronizedMessage(uint32_t accessAddress) {
  Message* msg = Whad::buildMessage();
  msg->which_msg = Message_ble_tag;
  msg->msg.ble.which_msg = ble_Message_desynchronized_tag;
  msg->msg.ble.msg.desynchronized.access_address = accessAddress;
  return msg;
}

Message* Whad::buildBLEInjectedMessage(bool success, uint32_t injectionAttempts, uint32_t accessAddress) {
  Message* msg = Whad::buildMessage();
  msg->which_msg = Message_ble_tag;
  msg->msg.ble.which_msg = ble_Message_injected_tag;
  msg->msg.ble.msg.injected.success = success;
  msg->msg.ble.msg.injected.access_address = accessAddress;
  msg->msg.ble.msg.injected.injection_attempts = injectionAttempts;
  return msg;
}

Message* Whad::buildBLEHijackedMessage(bool success, uint32_t accessAddress) {
  Message* msg = Whad::buildMessage();
  msg->which_msg = Message_ble_tag;
  msg->msg.ble.which_msg = ble_Message_hijacked_tag;
  msg->msg.ble.msg.hijacked.success = success;
  msg->msg.ble.msg.hijacked.access_address = accessAddress;
  return msg;
}


Message* Whad::buildBLEAccessAddressDiscoveredMessage(uint32_t accessAddress, uint32_t timestamp, int32_t rssi) {
  Message* msg = Whad::buildMessage();
  msg->which_msg = Message_ble_tag;
  msg->msg.ble.which_msg = ble_Message_aa_disc_tag;

  msg->msg.ble.msg.aa_disc.access_address = accessAddress;
  msg->msg.ble.msg.aa_disc.has_timestamp = true;
  msg->msg.ble.msg.aa_disc.timestamp = timestamp;
  msg->msg.ble.msg.aa_disc.has_rssi = true;
  msg->msg.ble.msg.aa_disc.rssi = rssi;

  return msg;
}
