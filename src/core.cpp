#include "core.h"

// Global instance of Core
Core* Core::instance = NULL;


void Core::handleInputData(uint8_t *buffer, size_t size) {
	// Commands are received from Host
  Message msg = Whad::decodeMessage(buffer, size);
	this->processInputMessage(msg);
}

void Core::processInputMessage(Message msg) {
  switch (msg.which_msg) {
    case Message_generic_tag:
      this->processGenericInputMessage(msg.msg.generic);
      break;

    case Message_discovery_tag:
      this->processDiscoveryInputMessage(msg.msg.discovery);
      break;

    case Message_ble_tag:
      this->processBLEInputMessage(msg.msg.ble);
      break;

    case Message_zigbee_tag:
      this->processZigbeeInputMessage(msg.msg.zigbee);
      break;

    case Message_esb_tag:
      this->processESBInputMessage(msg.msg.esb);
      break;

    case Message_unifying_tag:
      this->processUnifyingInputMessage(msg.msg.unifying);
      break;

    case Message_phy_tag:
      this->processPhyInputMessage(msg.msg.phy);
      break;
    default:
      // send error ?
      break;
  };
}

void Core::processGenericInputMessage(generic_Message msg) {
}

void Core::processDiscoveryInputMessage(discovery_Message msg) {
  Message *response = NULL;

  if (msg.which_msg == discovery_Message_reset_query_tag) {
      // Disable controller
      this->radio->disable();
      this->currentController = NULL;
      this->radio->setController(NULL);
      // Send Ready Response
      response = Whad::buildDiscoveryReadyResponseMessage();
      this->pushMessageToQueue(response);
  }
  else if (msg.which_msg == discovery_Message_info_query_tag) {
        if (msg.msg.info_query.proto_ver <= WHAD_MIN_VERSION) {
          response = Whad::buildDiscoveryDeviceInfoMessage();
        }
        else {
          response = Whad::buildResultMessage(generic_ResultCode_ERROR); // unsupported protcol version
        }
        this->pushMessageToQueue(response);
   }
   else if (msg.which_msg == discovery_Message_domain_query_tag) {
      discovery_Domain domain = (discovery_Domain)msg.msg.domain_query.domain;
      if (Whad::isDomainSupported(domain)) {
        response = Whad::buildDiscoveryDomainInfoMessage(domain);
      }
      else {
        response = Whad::buildResultMessage(generic_ResultCode_UNSUPPORTED_DOMAIN);
      }
    }
    this->pushMessageToQueue(response);
}

void Core::processZigbeeInputMessage(zigbee_Message msg) {
  Message *response = NULL;
  if (this->currentController != this->dot15d4Controller) {
      this->selectController(DOT15D4_PROTOCOL);
  }

  if (msg.which_msg == zigbee_Message_sniff_tag) {
    int channel = msg.msg.sniff.channel;
    if (channel >= 11 && channel <= 26) {
      this->dot15d4Controller->setChannel(channel);
      this->dot15d4Controller->enterReceptionMode();
      this->dot15d4Controller->setAutoAcknowledgement(false);
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
    else {
      response = Whad::buildResultMessage(generic_ResultCode_PARAMETER_ERROR);
    }
  }
  else if (msg.which_msg == zigbee_Message_ed_tag) {
    int channel = msg.msg.ed.channel;
    if (channel >= 11 && channel <= 26) {
      this->dot15d4Controller->setChannel(channel);
      this->dot15d4Controller->enterEDScanMode();
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
    else {
      response = Whad::buildResultMessage(generic_ResultCode_PARAMETER_ERROR);
    }
  }
  else if (msg.which_msg == zigbee_Message_start_tag) {
    this->currentController->start();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }

  else if (msg.which_msg == zigbee_Message_end_device_tag) {
    int channel = msg.msg.end_device.channel;
    if (channel >= 11 && channel <= 26) {
      this->dot15d4Controller->setChannel(channel);
      this->dot15d4Controller->enterReceptionMode();
      this->dot15d4Controller->setAutoAcknowledgement(true);
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
    else {
      response = Whad::buildResultMessage(generic_ResultCode_PARAMETER_ERROR);
    }
  }


  else if (msg.which_msg == zigbee_Message_coordinator_tag) {
    int channel = msg.msg.coordinator.channel;
    if (channel >= 11 && channel <= 26) {
      this->dot15d4Controller->setChannel(channel);
      this->dot15d4Controller->enterReceptionMode();
      this->dot15d4Controller->setAutoAcknowledgement(true);
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
    else {
      response = Whad::buildResultMessage(generic_ResultCode_PARAMETER_ERROR);
    }
  }

  else if (msg.which_msg == zigbee_Message_router_tag) {
    int channel = msg.msg.router.channel;
    if (channel >= 11 && channel <= 26) {
      this->dot15d4Controller->setChannel(channel);
      this->dot15d4Controller->enterReceptionMode();
      this->dot15d4Controller->setAutoAcknowledgement(true);
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
    else {
      response = Whad::buildResultMessage(generic_ResultCode_PARAMETER_ERROR);
    }
  }
  else if (msg.which_msg == zigbee_Message_set_node_addr_tag) {
    if (msg.msg.set_node_addr.address_type == zigbee_AddressType_SHORT) {
      uint16_t shortAddress = msg.msg.set_node_addr.address & 0xFFFF;
      this->dot15d4Controller->setShortAddress(shortAddress);
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);

    }
    else {
      uint64_t extendedAddress = msg.msg.set_node_addr.address;
      this->dot15d4Controller->setExtendedAddress(extendedAddress);
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);

    }
  }

  else if (msg.which_msg == zigbee_Message_stop_tag) {
    this->currentController->stop();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == zigbee_Message_send_tag) {
    int channel = msg.msg.send.channel;
    generic_ResultCode code = generic_ResultCode_SUCCESS;
    if (channel >= 11 && channel <= 26) {
      this->dot15d4Controller->setChannel(channel);
    }
    else {
      code = generic_ResultCode_PARAMETER_ERROR;
    }
    if (code == generic_ResultCode_SUCCESS) {
      size_t size = msg.msg.send.pdu.size;
      uint8_t *packet = (uint8_t*)malloc(1 + size);
      packet[0] = size;
      memcpy(packet+1,msg.msg.send.pdu.bytes, size);
      this->dot15d4Controller->send(packet, size+1, false);
      free(packet);
      code = generic_ResultCode_SUCCESS;
    }
    response = Whad::buildResultMessage(code);
  }
  else if (msg.which_msg == zigbee_Message_send_raw_tag) {
    int channel = msg.msg.send_raw.channel;
    generic_ResultCode code = generic_ResultCode_SUCCESS;
    if (channel >= 11 && channel <= 26) {
      this->dot15d4Controller->setChannel(channel);
    }
    else {
      code = generic_ResultCode_PARAMETER_ERROR;
    }
    if (code == generic_ResultCode_SUCCESS) {
      size_t size = msg.msg.send_raw.pdu.size;
      uint8_t *packet = (uint8_t*)malloc(3 + size);
      packet[0] = size+2;
      memcpy(packet+1,msg.msg.send_raw.pdu.bytes, size);
      memcpy(packet+1+size, &msg.msg.send_raw.fcs, 2);
      this->dot15d4Controller->send(packet, size+3, true);
      free(packet);
      code = generic_ResultCode_SUCCESS;
    }
    response = Whad::buildResultMessage(code);
  }
  this->pushMessageToQueue(response);
}

void Core::processBLEInputMessage(ble_Message msg) {
  Message *response = NULL;

  if (this->currentController != this->bleController) {
    this->selectController(BLE_PROTOCOL);
  }

  if (msg.which_msg == ble_Message_sniff_adv_tag) {

    // bool enableExtended = msg.msg.sniff_adv.use_extended_adv;

    int channel = msg.msg.sniff_adv.channel;
    this->bleController->setChannel(channel);
    this->bleController->setAdvertisementsTransmitIndicator(true);
    this->bleController->setFilter(
                                    true,
                                    msg.msg.sniff_adv.bd_address[5],
                                    msg.msg.sniff_adv.bd_address[4],
                                    msg.msg.sniff_adv.bd_address[3],
                                    msg.msg.sniff_adv.bd_address[2],
                                    msg.msg.sniff_adv.bd_address[1],
                                    msg.msg.sniff_adv.bd_address[0]
    );
    this->bleController->setFollowMode(false);
    this->bleController->sniff();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == ble_Message_reactive_jam_tag) {

    int channel = msg.msg.reactive_jam.channel;
    uint8_t *pattern = msg.msg.reactive_jam.pattern.bytes;
    size_t pattern_size = msg.msg.reactive_jam.pattern.size;
    int position = msg.msg.reactive_jam.position;

    this->bleController->setChannel(channel);
    this->bleController->setReactiveJammerConfiguration(pattern, pattern_size, position );
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == ble_Message_sniff_connreq_tag) {
    int channel = msg.msg.sniff_connreq.channel;
    bool show_advertisements = msg.msg.sniff_connreq.show_advertisements;
    bool show_empty_packets = msg.msg.sniff_connreq.show_empty_packets;
    this->bleController->setChannel(channel);
    this->bleController->setAdvertisementsTransmitIndicator(show_advertisements);
    this->bleController->setEmptyTransmitIndicator(show_empty_packets);
    this->bleController->setFilter(
                                    false,
                                    msg.msg.sniff_connreq.bd_address[5],
                                    msg.msg.sniff_connreq.bd_address[4],
                                    msg.msg.sniff_connreq.bd_address[3],
                                    msg.msg.sniff_connreq.bd_address[2],
                                    msg.msg.sniff_connreq.bd_address[1],
                                    msg.msg.sniff_connreq.bd_address[0]
    );
    this->bleController->setFollowMode(true);
    this->bleController->sniff();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == ble_Message_sniff_aa_tag) {
    this->bleController->setChannel(0);
    this->bleController->setMonitoredChannels(msg.msg.sniff_aa.monitored_channels);
    this->bleController->sniffAccessAddresses();

    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == ble_Message_sniff_conn_tag) {
    this->bleController->setChannel(0);
    this->bleController->setMonitoredChannels(msg.msg.sniff_conn.monitored_channels);

    if (msg.msg.sniff_conn.crc_init == 0) {
      this->bleController->recoverCrcInit(msg.msg.sniff_conn.access_address);
    }
    else if (msg.msg.sniff_conn.channel_map[0] == 0 && msg.msg.sniff_conn.channel_map[1] == 0 && msg.msg.sniff_conn.channel_map[2] == 0 && msg.msg.sniff_conn.channel_map[3] == 0 && msg.msg.sniff_conn.channel_map[4] == 0) {
      this->bleController->recoverChannelMap(msg.msg.sniff_conn.access_address, msg.msg.sniff_conn.crc_init);
    }
    else if (msg.msg.sniff_conn.hop_interval == 0) {
      this->bleController->recoverHopInterval(msg.msg.sniff_conn.access_address, msg.msg.sniff_conn.crc_init, msg.msg.sniff_conn.channel_map);
    }
    else if (msg.msg.sniff_conn.hop_increment == 0) {
      this->bleController->recoverHopIncrement(msg.msg.sniff_conn.access_address, msg.msg.sniff_conn.crc_init, msg.msg.sniff_conn.channel_map, msg.msg.sniff_conn.hop_interval);
    }
    else {
      this->bleController->attachToExistingConnection(msg.msg.sniff_conn.access_address, msg.msg.sniff_conn.crc_init, msg.msg.sniff_conn.channel_map, msg.msg.sniff_conn.hop_interval, msg.msg.sniff_conn.hop_increment);
    }

    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == ble_Message_start_tag) {
    this->bleController->start();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }

  else if (msg.which_msg == ble_Message_stop_tag) {
    this->bleController->stop();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }

  else if (msg.which_msg == ble_Message_send_raw_pdu_tag) {


    //uint32_t access_address = msg.msg.send_pdu.access_address;
    //uint32_t crc = msg.msg.send_pdu.crc;

    if (msg.msg.send_raw_pdu.direction == ble_BleDirection_INJECTION_TO_SLAVE) {
      this->bleController->setAttackPayload(msg.msg.send_raw_pdu.pdu.bytes, msg.msg.send_raw_pdu.pdu.size);
			this->bleController->startAttack(BLE_ATTACK_FRAME_INJECTION_TO_SLAVE);
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
    else if (msg.msg.send_raw_pdu.direction == ble_BleDirection_INJECTION_TO_MASTER) {
      this->bleController->setAttackPayload(msg.msg.send_raw_pdu.pdu.bytes, msg.msg.send_raw_pdu.pdu.size);
			this->bleController->startAttack(BLE_ATTACK_FRAME_INJECTION_TO_MASTER);
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
    else if (msg.msg.send_raw_pdu.direction == ble_BleDirection_MASTER_TO_SLAVE) {
      this->bleController->setMasterPayload(msg.msg.send_raw_pdu.pdu.bytes, msg.msg.send_raw_pdu.pdu.size);
      while (!this->bleController->isMasterPayloadTransmitted()) {}
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
    else if (msg.msg.send_raw_pdu.direction == ble_BleDirection_SLAVE_TO_MASTER) {
      this->bleController->setSlavePayload(msg.msg.send_raw_pdu.pdu.bytes, msg.msg.send_raw_pdu.pdu.size);
      while (!this->bleController->isSlavePayloadTransmitted()) {}
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
  }
  else if (msg.which_msg == ble_Message_hijack_master_tag) {
        this->bleController->startAttack(BLE_ATTACK_MASTER_HIJACKING);
        response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == ble_Message_hijack_slave_tag) {
        this->bleController->startAttack(BLE_ATTACK_SLAVE_HIJACKING);
        response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == ble_Message_hijack_both_tag) {
        this->bleController->startAttack(BLE_ATTACK_MITM);
        response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == ble_Message_central_mode_tag) {

      //if (this->bleController->getState() == CONNECTION_INITIATION || this->bleController->getState() == SIMULATING_MASTER || this->bleController->getState() == PERFORMING_MITM) {
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
      /*}
      else {
        response = Whad::buildResultMessage(generic_ResultCode_WRONG_MODE);
      }*/
  }

  else if (msg.which_msg == ble_Message_periph_mode_tag) {
      if (this->bleController->getState() == SIMULATING_SLAVE || this->bleController->getState() == PERFORMING_MITM) {
        response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
      }
      else {
        response = Whad::buildResultMessage(generic_ResultCode_WRONG_MODE);
      }
  }

  else if (msg.which_msg == ble_Message_connect_tag) {
    this->bleController->setChannel(37);
    uint8_t address[6];
    address[0] = msg.msg.connect.bd_address[5];
    address[1] = msg.msg.connect.bd_address[4];
    address[2] = msg.msg.connect.bd_address[3];
    address[3] = msg.msg.connect.bd_address[2];
    address[4] = msg.msg.connect.bd_address[1];
    address[5] = msg.msg.connect.bd_address[0];

    this->bleController->connect(
        address,
        msg.msg.connect.addr_type == ble_BleAddrType_RANDOM
    );

    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }

  else if (msg.which_msg == ble_Message_disconnect_tag) {
    uint8_t *terminate_ind;
    size_t terminate_ind_size;
    BLEPacket::forgeTerminateInd(&terminate_ind, &terminate_ind_size,0x13);
    this->bleController->setMasterPayload(terminate_ind,terminate_ind_size);
    while (!this->bleController->isMasterPayloadTransmitted()) {}
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  /*
  else if (msg.which_msg == ble_Message_jam_adv_chan_tag) {
    int channel = msg.msg.jam_adv_chan.channel;
    if (channel == 37 || channel == 38 || channel == 39) {
      this->bleController->setChannel(channel);
      this->bleController->setJammerConfiguration();
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
    else {
      response = Whad::buildResultMessage(generic_ResultCode_PARAMETER_ERROR);
    }
  }*/
  this->pushMessageToQueue(response);
}
void Core::processESBInputMessage(esb_Message msg) {
  Message *response = NULL;

  if (this->currentController != this->esbController) {
    this->selectController(ESB_PROTOCOL);
    this->esbController->disableUnifying();
  }

  if (msg.which_msg == esb_Message_sniff_tag) {
    if (msg.msg.sniff.channel == 0xFF || (msg.msg.sniff.channel >= 0 && msg.msg.sniff.channel <= 100)) {
      this->esbController->setFilter(
                msg.msg.sniff.address.bytes[0],
                msg.msg.sniff.address.bytes[1],
                msg.msg.sniff.address.bytes[2],
                msg.msg.sniff.address.bytes[3],
                msg.msg.sniff.address.bytes[4]
      );
      this->esbController->setChannel(msg.msg.sniff.channel);
      if (msg.msg.sniff.show_acknowledgements) {
        this->esbController->enableAcknowledgementsSniffing();
      }
      else {
        this->esbController->disableAcknowledgementsSniffing();
      }

      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
    else {
      response = Whad::buildResultMessage(generic_ResultCode_PARAMETER_ERROR);
    }
  }
  else if (msg.which_msg == esb_Message_start_tag) {
    this->esbController->start();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == esb_Message_stop_tag) {
    this->esbController->stop();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == esb_Message_send_raw_tag) {
    int channel = msg.msg.send_raw.channel;
    if (channel >= 0 && channel <= 100) {
      this->esbController->setChannel(channel);
    }
    this->esbController->send(msg.msg.send_raw.pdu.bytes, msg.msg.send_raw.pdu.size);
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == esb_Message_set_node_addr_tag) {
    this->esbController->setFilter(
              msg.msg.set_node_addr.address.bytes[0],
              msg.msg.set_node_addr.address.bytes[1],
              msg.msg.set_node_addr.address.bytes[2],
              msg.msg.set_node_addr.address.bytes[3],
              msg.msg.set_node_addr.address.bytes[4]
    );
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == esb_Message_prx_tag) {
    this->esbController->setChannel(msg.msg.prx.channel);
    this->esbController->enableAcknowledgementsTransmission();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == esb_Message_ptx_tag) {
    this->esbController->setChannel(msg.msg.ptx.channel);
    this->esbController->disableAcknowledgementsTransmission();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  this->pushMessageToQueue(response);
}

void Core::processUnifyingInputMessage(unifying_Message msg) {
  Message *response = NULL;

  if (this->currentController != this->esbController) {
    this->selectController(ESB_PROTOCOL);
    this->esbController->enableUnifying();
  }

  if (msg.which_msg == unifying_Message_sniff_tag) {
    if (msg.msg.sniff.channel == 0xFF || (msg.msg.sniff.channel >= 0 && msg.msg.sniff.channel <= 100)) {
      this->esbController->setFilter(
                msg.msg.sniff.address.bytes[0],
                msg.msg.sniff.address.bytes[1],
                msg.msg.sniff.address.bytes[2],
                msg.msg.sniff.address.bytes[3],
                msg.msg.sniff.address.bytes[4]
      );
      this->esbController->setChannel(msg.msg.sniff.channel);
      if (msg.msg.sniff.show_acknowledgements) {
        this->esbController->enableAcknowledgementsSniffing();
      }
      else {
        this->esbController->disableAcknowledgementsSniffing();
      }

      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
    else {
      response = Whad::buildResultMessage(generic_ResultCode_PARAMETER_ERROR);
    }
  }
  else if (msg.which_msg == unifying_Message_start_tag) {
    this->esbController->start();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == unifying_Message_stop_tag) {
    this->esbController->stop();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == unifying_Message_send_raw_tag) {
    int channel = msg.msg.send_raw.channel;
    if (channel >= 0 && channel <= 100) {
      this->esbController->setChannel(channel);
    }
    this->esbController->send(msg.msg.send_raw.pdu.bytes, msg.msg.send_raw.pdu.size);
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == unifying_Message_set_node_addr_tag) {
    this->esbController->setFilter(
              msg.msg.set_node_addr.address.bytes[0],
              msg.msg.set_node_addr.address.bytes[1],
              msg.msg.set_node_addr.address.bytes[2],
              msg.msg.set_node_addr.address.bytes[3],
              msg.msg.set_node_addr.address.bytes[4]
    );
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == unifying_Message_dongle_tag) {
    this->esbController->setChannel(msg.msg.dongle.channel);
    this->esbController->enableAcknowledgementsTransmission();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == unifying_Message_mouse_tag) {
    this->esbController->setChannel(msg.msg.mouse.channel);
    this->esbController->disableAcknowledgementsTransmission();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == unifying_Message_keyboard_tag) {
    this->esbController->setChannel(msg.msg.keyboard.channel);
    this->esbController->disableAcknowledgementsTransmission();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == unifying_Message_sniff_pairing_tag) {

    this->esbController->setFilter(0xBB, 0x0A, 0xDC, 0xA5, 0x75);
    this->esbController->setChannel(5);

    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  this->pushMessageToQueue(response);
}

void Core::processPhyInputMessage(phy_Message msg) {
  Message *response = NULL;

  if (this->currentController != this->genericController) {
    this->selectController(GENERIC_PROTOCOL);
  }

  if (msg.which_msg == phy_Message_mod_gfsk_tag) {
    if (msg.msg.mod_gfsk.deviation == 170000) {
      this->genericController->setPhy(GENERIC_PHY_1MBPS_ESB);
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
    else if (msg.msg.mod_gfsk.deviation == 250000) {
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
      this->genericController->setPhy(GENERIC_PHY_1MBPS_BLE);
    }
    else if (msg.msg.mod_gfsk.deviation == 320000) {
      this->genericController->setPhy(GENERIC_PHY_2MBPS_ESB);
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
    else if (msg.msg.mod_gfsk.deviation == 500000) {
      this->genericController->setPhy(GENERIC_PHY_2MBPS_BLE);
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
    else {
      response = Whad::buildResultMessage(generic_ResultCode_PARAMETER_ERROR);
    }
  }

  else if (msg.which_msg == phy_Message_freq_twodotfourghz_tag) {
    int frequency_offset = msg.msg.freq_twodotfourghz.frequency_offset;
    if (frequency_offset >= 0 && frequency_offset <= 100) {
      this->genericController->setChannel(frequency_offset);
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
    else {
      response = Whad::buildResultMessage(generic_ResultCode_PARAMETER_ERROR);
    }
  }

  else if (msg.which_msg == phy_Message_datarate_tag) {
    if (msg.msg.datarate.rate == 1000000) {
      if (
          this->genericController->getPhy() == GENERIC_PHY_1MBPS_ESB ||
          this->genericController->getPhy() == GENERIC_PHY_2MBPS_ESB
      ) {
         this->genericController->setPhy(GENERIC_PHY_1MBPS_ESB);
         response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
      }
      else if (
        this->genericController->getPhy() == GENERIC_PHY_1MBPS_BLE ||
        this->genericController->getPhy() == GENERIC_PHY_2MBPS_BLE
      ) {
        this->genericController->setPhy(GENERIC_PHY_1MBPS_BLE);
        response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
      }
      else {
        response = Whad::buildResultMessage(generic_ResultCode_ERROR);
      }
    }
    else if (msg.msg.datarate.rate == 2000000) {
      if (
          this->genericController->getPhy() == GENERIC_PHY_1MBPS_ESB ||
          this->genericController->getPhy() == GENERIC_PHY_2MBPS_ESB
      ) {
         this->genericController->setPhy(GENERIC_PHY_2MBPS_ESB);
         response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
      }
      else if (
        this->genericController->getPhy() == GENERIC_PHY_1MBPS_BLE ||
        this->genericController->getPhy() == GENERIC_PHY_2MBPS_BLE
      ) {
        this->genericController->setPhy(GENERIC_PHY_2MBPS_BLE);
        response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
      }
      else {
        response = Whad::buildResultMessage(generic_ResultCode_ERROR);
      }
    }
    else {
      response = Whad::buildResultMessage(generic_ResultCode_PARAMETER_ERROR);
    }
  }
  else if (msg.which_msg == phy_Message_endianness_tag) {
    if (msg.msg.endianness.endianness == phy_Endianness_BIG) {
      this->genericController->setEndianness(GENERIC_ENDIANNESS_BIG);
    }
    else {
      this->genericController->setEndianness(GENERIC_ENDIANNESS_LITTLE);
    }
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == phy_Message_tx_power_tag) {
    if (msg.msg.tx_power.tx_power == phy_TXPower_LOW) {
      this->genericController->setTxPower(LOW);
    }
    else if (msg.msg.tx_power.tx_power == phy_TXPower_MEDIUM) {
      this->genericController->setTxPower(MEDIUM);
    }
    else {
      this->genericController->setTxPower(HIGH);
    }
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }

  else if (msg.which_msg == phy_Message_packet_size_tag) {
    this->genericController->setPacketSize(msg.msg.packet_size.size);
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == phy_Message_sync_word_tag) {
    this->genericController->setPreamble(msg.msg.sync_word.sync_word.bytes, msg.msg.sync_word.sync_word.size);
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == phy_Message_sniff_tag) {
    if (msg.msg.sniff.iq_stream) {
      response = Whad::buildResultMessage(generic_ResultCode_ERROR);
    }
    else {
      response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
    }
  }
  else if (msg.which_msg == phy_Message_start_tag) {
    this->genericController->start();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }

  else if (msg.which_msg == phy_Message_stop_tag) {
    this->genericController->stop();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  this->pushMessageToQueue(response);
}

#ifdef PA_ENABLED
void Core::configurePowerAmplifier(bool enabled) {
	NRF_P1->PIN_CNF[11] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
	                                    | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
	                                    | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
	                                    | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos)
	                                    | (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
	NRF_P1->PIN_CNF[12] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
	                                    | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
	                                    | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
	                                    | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos)
	                                    | (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
	if (enabled) {
		NRF_P1->OUTSET =   (1ul << 12) | (1ul << 13);
	}
	else {
		NRF_P1->OUTCLR =   (1ul << 12) | (1ul << 13);
	}
}

#endif
Core::Core() {
	instance = this;
	this->ledModule = new LedModule();
	this->timerModule = new TimerModule();
	//this->linkModule = new LinkModule();
	this->serialModule = new SerialComm((CoreCallback)&Core::handleInputData,this);
	this->radio = new Radio();

	#ifdef PA_ENABLED
	this->configurePowerAmplifier(true);
	#endif

  /*
  //this->linkModule->configureLink(LINK_SLAVE);


  this->linkModule->configureLink(LINK_MASTER);

  while (true) {
    this->linkModule->sendSignalToSlave(1);
    nrf_delay_ms(1000);
    this->linkModule->sendSignalToSlave(2);
    nrf_delay_ms(1000);

  }
  */
}

LedModule* Core::getLedModule() {
	return (this->ledModule);
}

SerialComm *Core::getSerialModule() {
	return (this->serialModule);
}
/*
LinkModule* Core::getLinkModule() {
	return (this->linkModule);
}*/

TimerModule* Core::getTimerModule() {
	return (this->timerModule);
}

Radio* Core::getRadioModule() {
	return (this->radio);
}

void Core::setControllerChannel(int channel) {
	if (this->currentController == this->bleController) {
    this->bleController->setChannel(channel);
  }
}
void Core::init() {

	this->messageQueue.size = 0;
	this->messageQueue.firstElement = NULL;
	this->messageQueue.lastElement = NULL;


	this->bleController = new BLEController(this->getRadioModule());
	this->dot15d4Controller = new Dot15d4Controller(this->getRadioModule());
	this->esbController = new ESBController(this->getRadioModule());
	this->antController = new ANTController(this->getRadioModule());
	this->mosartController = new MosartController(this->getRadioModule());
	this->genericController = new GenericController(this->getRadioModule());


	this->currentController = NULL;
	this->radio->setController(this->currentController);
  Message* response = Whad::buildDiscoveryReadyResponseMessage();
  this->pushMessageToQueue(response);

}

bool Core::selectController(Protocol controller) {
  this->getLedModule()->on(LED2);
	if (controller == BLE_PROTOCOL) {
    this->getLedModule()->setColor(BLUE);
		this->radio->disable();
		this->currentController = this->bleController;
		this->radio->setController(this->currentController);
		return true;
	}
	else if (controller == DOT15D4_PROTOCOL) {
    this->getLedModule()->setColor(GREEN);
		this->radio->disable();
		this->currentController = this->dot15d4Controller;
		this->radio->setController(this->currentController);
		return true;
	}
	else if (controller == ESB_PROTOCOL) {
    this->getLedModule()->setColor(PURPLE);
		this->radio->disable();
		this->currentController = this->esbController;
		this->radio->setController(this->currentController);
		return true;
	}
	else if (controller == ANT_PROTOCOL) {
    this->getLedModule()->setColor(RED);
		this->radio->disable();
		this->currentController = this->antController;
		this->radio->setController(this->currentController);
		return true;
	}
	else if (controller == MOSART_PROTOCOL) {
    this->getLedModule()->setColor(YELLOW);
		this->radio->disable();
		this->currentController = this->mosartController;
		this->radio->setController(this->currentController);
		return true;
	}
	else if (controller == GENERIC_PROTOCOL) {
    this->getLedModule()->setColor(CYAN);
		this->radio->disable();
		this->currentController = this->genericController;
		this->radio->setController(this->currentController);
		return true;
	}
	else {
    this->getLedModule()->off(LED2);
		this->radio->disable();
		this->currentController = NULL;
		this->radio->setController(NULL);
		return false;
	}
}

void Core::sendDebug(const char *message) {
	//this->pushMessageToQueue(new DebugNotification(message));
}

void Core::sendDebug(uint8_t *buffer, uint8_t size) {
	//this->pushMessageToQueue(new DebugNotification(buffer,size));
}


void Core::pushMessageToQueue(Message *msg) {
	MessageQueueElement *element = (MessageQueueElement*)malloc(sizeof(MessageQueueElement));
	element->message = msg;
	element->nextElement = NULL;
	if (this->messageQueue.size == 0) {
		this->messageQueue.firstElement = element;
		this->messageQueue.lastElement = element;
	}
	else {
		// We insert the message at the end of the queue
		this->messageQueue.lastElement->nextElement = element;
		this->messageQueue.lastElement = element;
	}
	this->messageQueue.size = this->messageQueue.size + 1;
}

Message* Core::popMessageFromQueue() {
	if (this->messageQueue.size == 0) return NULL;
	else {
		MessageQueueElement* element = this->messageQueue.firstElement;
		Message* msg = element->message;
		this->messageQueue.firstElement = element->nextElement;
		this->messageQueue.size = this->messageQueue.size - 1;
		free(element);
		return msg;
	}
}
/*
void Core::sendMessage(Message *msg) {
  uint8_t buffer[1024];
  size_t size = Whad::encodeMessage(msg, buffer, 1024);
	this->serialModule->send(buffer,size);
	free(msg);
}*/

bool Core::sendMessage(Message *msg) {
  uint8_t buffer[1024];
  size_t size = Whad::encodeMessage(msg, buffer, 1024);
	bool success = this->serialModule->send(buffer,size);
  if (success) free(msg);
  return success;
}

void Core::sendVerbose(const char* data) {
  Message *msg = Whad::buildVerboseMessage(data);
  this->pushMessageToQueue(msg);
}

void Core::loop() {
  Message *msg = this->popMessageFromQueue();
	while (true) {
		this->serialModule->process();

    if (msg != NULL) {
    	if (this->sendMessage(msg)) {
        msg = this->popMessageFromQueue();
      }
    }
    else {
      msg = this->popMessageFromQueue();
    }
    // Even if we miss an event enabling USB, USB event would wake us up.
    __WFE();
    // Clear SEV flag if CPU was woken up by event
    __SEV();

	}
}
/*
void Core::loop() {
	while (true) {
		this->serialModule->process();
    Message *msg = this->popMessageFromQueue();
    while (msg != NULL) {
    	this->sendMessage(msg);
    	msg = this->popMessageFromQueue();
    }
		// Even if we miss an event enabling USB, USB event would wake us up.
		__WFE();
		// Clear SEV flag if CPU was woken up by event
		__SEV();

	}
}
*/
