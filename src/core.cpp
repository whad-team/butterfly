#include "core.h"
#include <whad.h>

// Global instance of Core
Core* Core::instance = NULL;
static Message msg;

void Core::processInputMessage(Message msg) {
  whad::NanoPbMsg whadMsg(&msg);

  switch (whadMsg.getType())
  {
    case whad::MessageType::GenericMsg:
      this->processGenericInputMessage(whadMsg);
      break;

    case whad::MessageType::DiscoveryMsg:
        this->processDiscoveryInputMessage(whad::discovery::DiscoveryMsg(whadMsg));
        break;

    case whad::MessageType::DomainMsg:
    {
        switch (whadMsg.getDomain())
        {
            case whad::MessageDomain::DomainBle:
                this->processBLEInputMessage(msg.msg.ble);
                break;

            case whad::MessageDomain::DomainZigbee:
                this->processZigbeeInputMessage(msg.msg.zigbee);
                break;

            case whad::MessageDomain::DomainEsb:
                this->processESBInputMessage(msg.msg.esb);
                break;

            case whad::MessageDomain::DomainUnifying:
                this->processUnifyingInputMessage(msg.msg.unifying);
                break;

            case whad::MessageDomain::DomainPhy:
                this->processPhyInputMessage(msg, msg.msg.phy);
                break;

            default:
                // send error ?
                break;
        }
    }
    break;

    default:
        break;
  }
}

void Core::processGenericInputMessage(whad::NanoPbMsg msg) {
}

void Core::processDiscoveryInputMessage(whad::discovery::DiscoveryMsg msg) {
    Message *response = NULL;

    switch (msg.getType())
    {
        /* Device reset message processing. */
        case whad::discovery::MessageType::DeviceResetMsg:
            {
                // Disable controller
                this->radio->disable();
                this->currentController = NULL;
                this->radio->setController(NULL);

                // Send Ready Response
                whad::discovery::ReadyResp readyRsp = whad::discovery::ReadyResp();
                response = readyRsp.getRaw();
            }
            break;

        /* Device info query message processing. */
        case whad::discovery::MessageType::DeviceInfoQueryMsg:
            {
                whad::discovery::DeviceInfoQuery query(msg);

                if (query.getVersion() <= WHAD_MIN_VERSION)
                {
                    /* Craft device ID from unique values. */
                    uint8_t deviceId[16];
                    memcpy(&deviceId[0], (const void *)NRF_FICR->DEVICEID, 8);
                    memcpy(&deviceId[8], (const void *)NRF_FICR->DEVICEADDR, 8);

                    whad::discovery::DeviceInfoResp deviceInfo(
                        whad::discovery::Butterfly,
                        deviceId,
                        WHAD_MIN_VERSION,
                        115200,
                        std::string(FIRMWARE_AUTHOR),
                        std::string(FIRMWARE_URL),
                        VERSION_MAJOR,
                        VERSION_MINOR,
                        VERSION_REVISION,
                        (whad_domain_desc_t *)CAPABILITIES
                    );
                    response = deviceInfo.getRaw();
                }
                else
                {
                    response = whad::generic::Error().getRaw();
                }
            }
            break;

        /* Domain info query message processing. */
        case whad::discovery::MessageType::DomainInfoQueryMsg:
            {
                whad::discovery::DomainInfoQuery query(msg);
                whad::discovery::Domains domain = query.getDomain();

                if (whad::discovery::isDomainSupported(CAPABILITIES, domain))
                {
                    whad::discovery::DomainInfoResp domainInfoRsp(
                        (whad::discovery::Domains)domain,
                        (whad_domain_desc_t *)CAPABILITIES
                    );
                    
                    response = domainInfoRsp.getRaw();
                }
                else {
                    response = whad::generic::UnsupportedDomain().getRaw();
                }
            }
            break;

        default:
            response = whad::generic::Error().getRaw();
            break;
    }

    /* Send response. */
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
      response = whad::generic::Success().getRaw();
    }
    else {
      response = whad::generic::ParameterError().getRaw();
    }
  }
  else if (msg.which_msg == zigbee_Message_ed_tag) {
    int channel = msg.msg.ed.channel;
    if (channel >= 11 && channel <= 26) {
      this->dot15d4Controller->setChannel(channel);
      this->dot15d4Controller->enterEDScanMode();
      response = whad::generic::Success().getRaw();
    }
    else {
      response = whad::generic::ParameterError().getRaw();
    }
  }
  else if (msg.which_msg == zigbee_Message_start_tag) {
    this->currentController->start();
    response = whad::generic::Success().getRaw();
  }

  else if (msg.which_msg == zigbee_Message_end_device_tag) {
    int channel = msg.msg.end_device.channel;
    if (channel >= 11 && channel <= 26) {
      this->dot15d4Controller->setChannel(channel);
      this->dot15d4Controller->enterReceptionMode();
      this->dot15d4Controller->setAutoAcknowledgement(true);
      response = whad::generic::Success().getRaw();
    }
    else {
      response = whad::generic::ParameterError().getRaw();
    }
  }


  else if (msg.which_msg == zigbee_Message_coordinator_tag) {
    int channel = msg.msg.coordinator.channel;
    if (channel >= 11 && channel <= 26) {
      this->dot15d4Controller->setChannel(channel);
      this->dot15d4Controller->enterReceptionMode();
      this->dot15d4Controller->setAutoAcknowledgement(true);
      response = whad::generic::Success().getRaw();
    }
    else {
      response = whad::generic::ParameterError().getRaw();
    }
  }

  else if (msg.which_msg == zigbee_Message_router_tag) {
    int channel = msg.msg.router.channel;
    if (channel >= 11 && channel <= 26) {
      this->dot15d4Controller->setChannel(channel);
      this->dot15d4Controller->enterReceptionMode();
      this->dot15d4Controller->setAutoAcknowledgement(true);
      response = whad::generic::Success().getRaw();
    }
    else {
      response = whad::generic::ParameterError().getRaw();
    }
  }
  else if (msg.which_msg == zigbee_Message_set_node_addr_tag) {
    if (msg.msg.set_node_addr.address_type == zigbee_AddressType_SHORT) {
      uint16_t shortAddress = msg.msg.set_node_addr.address & 0xFFFF;
      this->dot15d4Controller->setShortAddress(shortAddress);
      response = whad::generic::Success().getRaw();

    }
    else {
      uint64_t extendedAddress = msg.msg.set_node_addr.address;
      this->dot15d4Controller->setExtendedAddress(extendedAddress);
      response = whad::generic::Success().getRaw();
    }
  }

  else if (msg.which_msg == zigbee_Message_stop_tag) {
    this->currentController->stop();
    response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == zigbee_Message_send_tag) {
    int channel = msg.msg.send.channel;
    
    if (channel >= 11 && channel <= 26) {
      /* Set channel. */
      this->dot15d4Controller->setChannel(channel);

      /* Build packet. */
      size_t size = msg.msg.send.pdu.size;
      uint8_t *packet = (uint8_t*)malloc(1 + size);
      packet[0] = size;
      memcpy(packet+1,msg.msg.send.pdu.bytes, size);
      this->dot15d4Controller->send(packet, size+1, false);
      free(packet);

      /* Success. */
      response = whad::generic::Success().getRaw();
    }
    else {
      response = whad::generic::ParameterError().getRaw();
    }
  }
  else if (msg.which_msg == zigbee_Message_send_raw_tag) {
    int channel = msg.msg.send_raw.channel;

    if (channel >= 11 && channel <= 26) {
      /* Set channel. */
      this->dot15d4Controller->setChannel(channel);

      /* Build packet. */
      size_t size = msg.msg.send_raw.pdu.size;
      uint8_t *packet = (uint8_t*)malloc(3 + size);
      packet[0] = size+2;
      memcpy(packet+1,msg.msg.send_raw.pdu.bytes, size);
      memcpy(packet+1+size, &msg.msg.send_raw.fcs, 2);
      this->dot15d4Controller->send(packet, size+3, true);
      free(packet);        

      /* Success. */
      response = whad::generic::Success().getRaw();
    }
    else {
      response = whad::generic::ParameterError().getRaw();
    }
  }

  /* Send response message. */
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
    response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == ble_Message_reactive_jam_tag) {

    int channel = msg.msg.reactive_jam.channel;
    uint8_t *pattern = msg.msg.reactive_jam.pattern.bytes;
    size_t pattern_size = msg.msg.reactive_jam.pattern.size;
    int position = msg.msg.reactive_jam.position;

    this->bleController->setChannel(channel);
    this->bleController->setReactiveJammerConfiguration(pattern, pattern_size, position );
    response = whad::generic::Success().getRaw();
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
    response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == ble_Message_sniff_aa_tag) {
    this->bleController->setChannel(0);
    this->bleController->setMonitoredChannels(msg.msg.sniff_aa.monitored_channels);
    this->bleController->sniffAccessAddresses();
    response = whad::generic::Success().getRaw();
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

    response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == ble_Message_start_tag) {
    this->bleController->start();
    response = whad::generic::Success().getRaw();
  }

  else if (msg.which_msg == ble_Message_stop_tag) {
    this->bleController->stop();
    response = whad::generic::Success().getRaw();
  }

  else if (msg.which_msg == ble_Message_scan_mode_tag) {
    this->bleController->startScanning(msg.msg.scan_mode.active_scan);
    response = whad::generic::Success().getRaw();
  }

  else if (msg.which_msg == ble_Message_send_raw_pdu_tag) {


    //uint32_t access_address = msg.msg.send_pdu.access_address;
    //uint32_t crc = msg.msg.send_pdu.crc;
    uint32_t max_retry = 1 << 24;

    if (msg.msg.send_raw_pdu.direction == ble_BleDirection_INJECTION_TO_SLAVE) {
      if (this->bleController->getState() == SNIFFING_CONNECTION) {

        this->bleController->setAttackPayload(msg.msg.send_raw_pdu.pdu.bytes, msg.msg.send_raw_pdu.pdu.size);
  			this->bleController->startAttack(BLE_ATTACK_FRAME_INJECTION_TO_SLAVE);
        response = whad::generic::Success().getRaw();
      }
      else {
        response = whad::generic::WrongMode().getRaw();
      }
    }
    else if (msg.msg.send_raw_pdu.direction == ble_BleDirection_INJECTION_TO_MASTER) {
      if (this->bleController->getState() == SNIFFING_CONNECTION) {
        this->bleController->setAttackPayload(msg.msg.send_raw_pdu.pdu.bytes, msg.msg.send_raw_pdu.pdu.size);
  			this->bleController->startAttack(BLE_ATTACK_FRAME_INJECTION_TO_MASTER);
        response = whad::generic::Success().getRaw();
      }
      else {
        response = whad::generic::WrongMode().getRaw();
      }
    }
    else if (msg.msg.send_raw_pdu.direction == ble_BleDirection_MASTER_TO_SLAVE) {
      if (this->bleController->getState() == SIMULATING_MASTER || this->bleController->getState() == PERFORMING_MITM) {
        this->bleController->setMasterPayload(msg.msg.send_raw_pdu.pdu.bytes, msg.msg.send_raw_pdu.pdu.size);
        while (!max_retry && !this->bleController->isMasterPayloadTransmitted()) {--max_retry;}
        if (max_retry != 0) {
            response = whad::generic::Success().getRaw();
        } else {
            response = whad::generic::Error().getRaw();

        }
      }
      else {
        response = whad::generic::WrongMode().getRaw();
      }
    }
    else if (msg.msg.send_raw_pdu.direction == ble_BleDirection_SLAVE_TO_MASTER) {
      if (this->bleController->getState() == SIMULATING_SLAVE || this->bleController->getState() == PERFORMING_MITM) {

        this->bleController->setSlavePayload(msg.msg.send_raw_pdu.pdu.bytes, msg.msg.send_raw_pdu.pdu.size);
        while (!this->bleController->isSlavePayloadTransmitted()) {}
        response = whad::generic::Success().getRaw();
      }
      else {
        response = whad::generic::WrongMode().getRaw();
      }
    }
  }
  else if (msg.which_msg == ble_Message_hijack_master_tag) {
        this->bleController->startAttack(BLE_ATTACK_MASTER_HIJACKING);
        response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == ble_Message_hijack_slave_tag) {
        this->bleController->startAttack(BLE_ATTACK_SLAVE_HIJACKING);
        response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == ble_Message_hijack_both_tag) {
        this->bleController->startAttack(BLE_ATTACK_MITM);
        response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == ble_Message_central_mode_tag) {

      //if (this->bleController->getState() == CONNECTION_INITIATION || this->bleController->getState() == SIMULATING_MASTER || this->bleController->getState() == PERFORMING_MITM) {
      response = whad::generic::Success().getRaw();
      /*}
      else {
        response = Whad::buildResultMessage(generic_ResultCode_WRONG_MODE);
      }*/
  }

  else if (msg.which_msg == ble_Message_periph_mode_tag) {
      if (this->bleController->getState() == SIMULATING_SLAVE || this->bleController->getState() == PERFORMING_MITM) {
        response = whad::generic::Success().getRaw();
      }
      else {
        //this->bleController->setEmptyTransmitIndicator(true);
        this->bleController->advertise(msg.msg.periph_mode.scan_data.bytes, msg.msg.periph_mode.scan_data.size, msg.msg.periph_mode.scanrsp_data.bytes, msg.msg.periph_mode.scanrsp_data.size, true, 100);
        this->bleController->start();
        response = whad::generic::Success().getRaw();
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

    uint32_t accessAddress = 0x23a3d487;
    uint32_t crcInit = 0x049095;
    uint16_t hopInterval = 56;
    uint8_t hopIncrement = 8;
    uint8_t channelMap[5] = {
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0x1F
    };
    if (msg.msg.connect.has_access_address) {
      accessAddress = msg.msg.connect.access_address;
    }
    if (msg.msg.connect.has_channel_map) {
      memcpy(channelMap, msg.msg.connect.channel_map, 5);
    }

    if (msg.msg.connect.has_crc_init) {
      crcInit = msg.msg.connect.crc_init;
    }

    if (msg.msg.connect.has_hop_interval) {
      hopInterval = msg.msg.connect.hop_interval;
    }

    if (msg.msg.connect.has_hop_increment) {
      hopIncrement = msg.msg.connect.hop_increment;
    }
    //this->bleController->setEmptyTransmitIndicator(true);
    this->bleController->connect(
        address,
        msg.msg.connect.addr_type == ble_BleAddrType_RANDOM,
        accessAddress,
        crcInit,
        3,
        9,
        hopInterval,
        0,
        42,
        1,
        hopIncrement,
        channelMap
    );

    response = whad::generic::Success().getRaw();
  }

  else if (msg.which_msg == ble_Message_disconnect_tag) {
    this->bleController->disconnect();
    response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == ble_Message_prepare_tag) {
    SequenceDirection direction = BLE_TO_SLAVE;
    Trigger* trigger = NULL;

    if (
      msg.msg.prepare.direction == ble_BleDirection_INJECTION_TO_SLAVE ||
      msg.msg.prepare.direction == ble_BleDirection_MASTER_TO_SLAVE
    ) {
      direction = BLE_TO_SLAVE;
    }
    else if (
      msg.msg.prepare.direction == ble_BleDirection_INJECTION_TO_MASTER ||
      msg.msg.prepare.direction == ble_BleDirection_SLAVE_TO_MASTER
    ) {
      direction = BLE_TO_MASTER;
    }
    else {
      response = whad::generic::WrongMode().getRaw();
    }
    if (response == NULL) {
      if (msg.msg.prepare.trigger.which_trigger == ble_PrepareSequenceCmd_Trigger_reception_tag) {
        trigger = new ReceptionTrigger(
            msg.msg.prepare.trigger.trigger.reception.pattern.bytes,
            msg.msg.prepare.trigger.trigger.reception.mask.bytes,
            msg.msg.prepare.trigger.trigger.reception.pattern.size,
            msg.msg.prepare.trigger.trigger.reception.offset
         );
      }
      else if (msg.msg.prepare.trigger.which_trigger == ble_PrepareSequenceCmd_Trigger_connection_event_tag) {
        trigger = new ConnectionEventTrigger(
          msg.msg.prepare.trigger.trigger.connection_event.connection_event
        );
      }
      else if (msg.msg.prepare.trigger.which_trigger == ble_PrepareSequenceCmd_Trigger_manual_tag) {
        trigger = new ManualTrigger();
      }
      else {
        response = whad::generic::WrongMode().getRaw();
      }
    }
    if (trigger != NULL) {
      int numberOfPackets = msg.msg.prepare.sequence_count;
      uint8_t identifier = msg.msg.prepare.id;
      PacketSequence *sequence = this->sequenceModule->createSequence(numberOfPackets, trigger, direction, identifier);
      for (int i=0;i<numberOfPackets;i++) {
        sequence->preparePacket(msg.msg.prepare.sequence[i].packet.bytes,msg.msg.prepare.sequence[i].packet.size, true);
      }

      response = whad::generic::Success().getRaw();
    }

  }
  else if (msg.which_msg == ble_Message_trigger_tag) {
      uint8_t id = msg.msg.trigger.id;
      if (this->bleController->checkManualTriggers(id)) {
        response = whad::generic::Success().getRaw();
      }
      else {
        response = whad::generic::Error().getRaw();
      }
  }
  else if (msg.which_msg == ble_Message_set_bd_addr_tag) {
    uint8_t address[6] = {
      msg.msg.set_bd_addr.bd_address[5],
      msg.msg.set_bd_addr.bd_address[4],
      msg.msg.set_bd_addr.bd_address[3],
      msg.msg.set_bd_addr.bd_address[2],
      msg.msg.set_bd_addr.bd_address[1],
      msg.msg.set_bd_addr.bd_address[0]
    };
    this->bleController->setOwnAddress(address, false);
    response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == ble_Message_delete_seq_tag) {
    uint8_t id = msg.msg.delete_seq.id;
    if (this->bleController->deleteSequence(id)) {
      response = whad::generic::Success().getRaw();
    }
    else {
      response = whad::generic::ParameterError().getRaw();
    }
  }
  else if (msg.which_msg == ble_Message_encryption_tag) {
    if (msg.msg.encryption.enabled) {
      this->bleController->configureEncryption(
        msg.msg.encryption.ll_key,
        msg.msg.encryption.ll_iv,
        0
      );
      if (this->bleController->startEncryption()) {
        response = whad::generic::Success().getRaw();
      }
      else {
        response = whad::generic::Error().getRaw();
      }
    }
    else {
      if (this->bleController->stopEncryption()) {
        response = whad::generic::Success().getRaw();
      }
      else {
        response = whad::generic::Error().getRaw();
      }
    }
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

      response = whad::generic::Success().getRaw();
    }
    else {
      response = whad::generic::ParameterError().getRaw();
    }
  }
  else if (msg.which_msg == esb_Message_start_tag) {
    this->esbController->start();
    response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == esb_Message_stop_tag) {
    this->esbController->stop();
    response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == esb_Message_send_raw_tag) {
    int channel = msg.msg.send_raw.channel;
    if (channel >= 0 && channel <= 100) {
      this->esbController->setChannel(channel);
    }
    if (this->esbController->send(msg.msg.send_raw.pdu.bytes, msg.msg.send_raw.pdu.size, msg.msg.send_raw.retransmission_count)) {
      response = whad::generic::Success().getRaw();
    }
    else {
      response = whad::generic::Error().getRaw();
    }
  }
  else if (msg.which_msg == esb_Message_set_node_addr_tag) {
    this->esbController->setFilter(
              msg.msg.set_node_addr.address.bytes[0],
              msg.msg.set_node_addr.address.bytes[1],
              msg.msg.set_node_addr.address.bytes[2],
              msg.msg.set_node_addr.address.bytes[3],
              msg.msg.set_node_addr.address.bytes[4]
    );
    response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == esb_Message_prx_tag) {
    this->esbController->setChannel(msg.msg.prx.channel);
    this->esbController->disableAcknowledgementsSniffing();
    this->esbController->enableAcknowledgementsTransmission();
    response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == esb_Message_ptx_tag) {
    this->esbController->setChannel(msg.msg.ptx.channel);
    this->esbController->enableAcknowledgementsSniffing();
    this->esbController->disableAcknowledgementsTransmission();
    response = whad::generic::Success().getRaw();
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

      response = whad::generic::Success().getRaw();
    }
    else {
      response = whad::generic::ParameterError().getRaw();
    }
  }
  else if (msg.which_msg == unifying_Message_start_tag) {
    this->esbController->start();
    response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == unifying_Message_stop_tag) {
    this->esbController->stop();
    response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == unifying_Message_send_raw_tag) {
    int channel = msg.msg.send_raw.channel;
    if (channel >= 0 && channel <= 100) {
      this->esbController->setChannel(channel);
    }
    if (this->esbController->send(msg.msg.send_raw.pdu.bytes, msg.msg.send_raw.pdu.size, msg.msg.send_raw.retransmission_count)) {
      response = whad::generic::Success().getRaw();
    }
    else {
      response = whad::generic::Error().getRaw();
    }
  }
  else if (msg.which_msg == unifying_Message_set_node_addr_tag) {
    this->esbController->setFilter(
              msg.msg.set_node_addr.address.bytes[0],
              msg.msg.set_node_addr.address.bytes[1],
              msg.msg.set_node_addr.address.bytes[2],
              msg.msg.set_node_addr.address.bytes[3],
              msg.msg.set_node_addr.address.bytes[4]
    );

    response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == unifying_Message_dongle_tag) {
    this->esbController->setChannel(msg.msg.dongle.channel);
    this->esbController->disableAcknowledgementsSniffing();
    this->esbController->enableAcknowledgementsTransmission();

    response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == unifying_Message_mouse_tag) {
    if (msg.msg.mouse.channel == 0xFF || (msg.msg.mouse.channel >= 0 && msg.msg.mouse.channel <= 100)) {
      this->esbController->setChannel(msg.msg.mouse.channel);
      this->esbController->enableAcknowledgementsSniffing();
      this->esbController->disableAcknowledgementsTransmission();

      response = whad::generic::Success().getRaw();
    }
    else {

      response = whad::generic::ParameterError().getRaw();
    }
  }
  else if (msg.which_msg == unifying_Message_keyboard_tag) {
    this->esbController->setChannel(msg.msg.keyboard.channel);
    this->esbController->enableAcknowledgementsSniffing();
    this->esbController->disableAcknowledgementsTransmission();

    response = whad::generic::Success().getRaw();
  }
  else if (msg.which_msg == unifying_Message_sniff_pairing_tag) {

    this->esbController->setFilter(0xBB, 0x0A, 0xDC, 0xA5, 0x75);
    this->esbController->setChannel(5);

    response = whad::generic::Success().getRaw();
  }
  this->pushMessageToQueue(response);
}

void Core::processPhyInputMessage(Message msg_raw, phy_Message msg) {
  Message *response = NULL;
  whad::phy::PhyMsg message(&msg_raw);

  if (this->currentController != this->genericController) {
    this->selectController(GENERIC_PROTOCOL);
  }

  switch (message.getType())
  {
    case whad::phy::SetGfskModMsg:
        {
            if (msg.msg.mod_gfsk.deviation == 170000) {
            this->genericController->setPhy(GENERIC_PHY_1MBPS_ESB);
            response = whad::generic::Success().getRaw();
            }
            else if (msg.msg.mod_gfsk.deviation == 250000) {
            response = whad::generic::Success().getRaw();
            this->genericController->setPhy(GENERIC_PHY_1MBPS_BLE);
            }
            else if (msg.msg.mod_gfsk.deviation == 320000) {
            this->genericController->setPhy(GENERIC_PHY_2MBPS_ESB);
            response = whad::generic::Success().getRaw();
            }
            else if (msg.msg.mod_gfsk.deviation == 500000) {
            this->genericController->setPhy(GENERIC_PHY_2MBPS_BLE);
            response = whad::generic::Success().getRaw();
            }
            else {
            response = whad::generic::ParameterError().getRaw();
            }
        }
        break;

    case whad::phy::SendMsg:
        {
            this->genericController->send(msg.msg.send.packet.bytes, msg.msg.send.packet.size);
            response = whad::generic::Success().getRaw();
        }
        break;

    case whad::phy::GetSupportedFreqsMsg:
        {
            //response = Whad::buildPhySupportedFrequencyRangeMessage();
            response = whad::phy::SupportedFreqsResp(SUPPORTED_FREQUENCY_RANGES).getRaw();
        }
        break;

    case whad::phy::SetFreqMsg:
        {
            uint64_t frequency = msg.msg.set_freq.frequency;
            if (frequency >= 2400000000L && frequency <= 2500000000L) {
                int frequency_offset = (frequency / 1000000) - 2400;
                this->genericController->setChannel(frequency_offset);
                response = whad::generic::Success().getRaw();
            }
            else {
            response = whad::generic::ParameterError().getRaw();
            }            
        }
        break;

    case whad::phy::SetDatarateMsg:
        {
            if (msg.msg.datarate.rate == 1000000) {
                if (
                    this->genericController->getPhy() == GENERIC_PHY_1MBPS_ESB ||
                    this->genericController->getPhy() == GENERIC_PHY_2MBPS_ESB
                ) {
                    this->genericController->setPhy(GENERIC_PHY_1MBPS_ESB);
                    response = whad::generic::Success().getRaw();
                }
                else if (
                    this->genericController->getPhy() == GENERIC_PHY_1MBPS_BLE ||
                    this->genericController->getPhy() == GENERIC_PHY_2MBPS_BLE
                ) {
                    this->genericController->setPhy(GENERIC_PHY_1MBPS_BLE);
                    response = whad::generic::Success().getRaw();
                }
                else {
                    response = whad::generic::Error().getRaw();
                }
            }
            else if (msg.msg.datarate.rate == 2000000) {
                if (
                    this->genericController->getPhy() == GENERIC_PHY_1MBPS_ESB ||
                    this->genericController->getPhy() == GENERIC_PHY_2MBPS_ESB
                ) {
                    this->genericController->setPhy(GENERIC_PHY_2MBPS_ESB);
                    response = whad::generic::Success().getRaw();
                }
                else if (
                    this->genericController->getPhy() == GENERIC_PHY_1MBPS_BLE ||
                    this->genericController->getPhy() == GENERIC_PHY_2MBPS_BLE
                ) {
                    this->genericController->setPhy(GENERIC_PHY_2MBPS_BLE);
                    response = whad::generic::Success().getRaw();
                }
                else {
                    response = whad::generic::Error().getRaw();
                }
            }
            else {
                response = whad::generic::ParameterError().getRaw();
            }
        }
        break;

    case whad::phy::SetEndiannessMsg:
        {
            if (msg.msg.endianness.endianness == phy_Endianness_BIG) {
                this->genericController->setEndianness(GENERIC_ENDIANNESS_BIG);
            }
            else {
                this->genericController->setEndianness(GENERIC_ENDIANNESS_LITTLE);
            }
            response = whad::generic::Success().getRaw();
        }
        break;

    case whad::phy::SetTxPowerMsg:
        {
            if (msg.msg.tx_power.tx_power == phy_TXPower_LOW) {
                this->genericController->setTxPower(LOW);
            }
            else if (msg.msg.tx_power.tx_power == phy_TXPower_MEDIUM) {
                this->genericController->setTxPower(MEDIUM);
            }
            else {
                this->genericController->setTxPower(HIGH);
            }
            response = whad::generic::Success().getRaw();
        }
        break;

    case whad::phy::SetPacketSizeMsg:
        {
            if (msg.msg.packet_size.packet_size <= 252) {
                this->genericController->setPacketSize(msg.msg.packet_size.packet_size);
                response = whad::generic::Success().getRaw();
            }
            else {
                response = whad::generic::ParameterError().getRaw();
            }
        }
        break;

    case whad::phy::SetSyncWordMsg:
        {
            this->genericController->setPreamble(msg.msg.sync_word.sync_word.bytes, msg.msg.sync_word.sync_word.size);
            response = whad::generic::Success().getRaw();
        }
        break;

    case whad::phy::SetSniffModeMsg:
        {
            if (msg.msg.sniff.iq_stream) {
                response = whad::generic::Error().getRaw();
            }
            else {
                response = whad::generic::Success().getRaw();
            }
        }
        break;

    case whad::phy::StartMsg:
        {
            this->genericController->start();
            response = whad::generic::Success().getRaw();
        }
        break;

    case whad::phy::StopMsg:
        {
            this->genericController->stop();
            response = whad::generic::Success().getRaw();
        }
        break;

    default:
        response = whad::generic::Error().getRaw();
        break;
  }

  this->pushMessageToQueue(response);
}

#ifdef PA_ENABLED
void Core::configurePowerAmplifier(bool enabled) {
	NRF_P1->PIN_CNF[11] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
	                                    | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
	                                    | (GPIO_PIN_CNF_PULL_Pulldown << GPIO_PIN_CNF_PULL_Pos)
	                                    | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos)
	                                    | (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
	NRF_P1->PIN_CNF[12] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
	                                    | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
	                                    | (GPIO_PIN_CNF_PULL_Pulldown << GPIO_PIN_CNF_PULL_Pos)
	                                    | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos)
	                                    | (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
	if (enabled) {
		NRF_P1->OUTSET =   (1ul << 11) | (1ul << 12);
	}
	else {
		NRF_P1->OUTCLR =   (1ul << 11) | (1ul << 12);
	}
}

#endif

void core_send_bytes(uint8_t *p_bytes, int size)
{
    if (Core::instance != NULL)
    {
        //Core::instance->getLedModule()->off(LED2);
        Core::instance->getSerialModule()->send(p_bytes, size);
    }
    else
    {
        whad_transport_data_sent();
    }
}

Core::Core() {
	instance = this;
	this->ledModule = new LedModule();
	this->timerModule = new TimerModule();
	this->sequenceModule = new SequenceModule();
	this->serialModule = new SerialComm();
	this->radio = new Radio();

    /* Initialize WHAD library. */
    memset(&this->transportConfig, 0, sizeof(whad_transport_cfg_t));
    this->transportConfig.max_txbuf_size = 64;
    this->transportConfig.pfn_data_send_buffer = core_send_bytes;
    whad_init(&this->transportConfig);

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

SequenceModule* Core::getSequenceModule() {
	return (this->sequenceModule);
}

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
}

bool Core::selectController(Protocol controller) {
  //this->getLedModule()->on(LED2);
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
    //this->getLedModule()->off(LED2);
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

void Core::sendVerbose(const char* data) {
  std::string message(data);
  whad::generic::Verbose verbMsg(message);
  this->pushMessageToQueue(verbMsg.getRaw());
}

void Core::loop() {
    Message *message = this->popMessageFromQueue();

	while (true) {

		this->serialModule->process();
        //this->getLedModule()->on(LED1);

        /* Check if we receveived a WHAD message. */
        if (whad_get_message(&msg) == WHAD_SUCCESS)
        {
            //this->getLedModule()->off(LED1);
            //this->getLedModule()->on(LED2);
            this->processInputMessage(msg);
        }
        if (message != NULL) {
          if (whad_send_message(message) == WHAD_ERROR)
          {
              //this->getLedModule()->on(LED1);
          }
          free(message);
          message = this->popMessageFromQueue();
        }
        else {
          message = this->popMessageFromQueue();
        }
    }
}
