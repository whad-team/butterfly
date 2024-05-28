#include "core.h"
#include "capabilities.h"
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
                this->processBLEInputMessage(whad::ble::BleMsg(whadMsg));
                break;

            case whad::MessageDomain::DomainDot15d4:
                this->processDot15d4InputMessage(whad::dot15d4::Dot15d4Msg(whadMsg));
                break;

            case whad::MessageDomain::DomainEsb:
                this->processESBInputMessage(whad::esb::EsbMsg(whadMsg));
                break;

            case whad::MessageDomain::DomainUnifying:
                this->processUnifyingInputMessage(whad::unifying::UnifyingMsg(whadMsg));
                break;

            case whad::MessageDomain::DomainPhy:
                this->processPhyInputMessage(whad::phy::PhyMsg(whadMsg));
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
    whad::NanoPbMsg *response = NULL;

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
                response = new whad::discovery::ReadyResp();
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

                    response = new whad::discovery::DeviceInfoResp(
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
                }
                else
                {
                    response = new whad::generic::Error();
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
                    response = new whad::discovery::DomainInfoResp(
                        (whad::discovery::Domains)domain,
                        (whad_domain_desc_t *)CAPABILITIES
                    );
                }
                else {
                    response = new whad::generic::UnsupportedDomain();
                }
            }
            break;

        default:
            response = new whad::generic::Error();
            break;
    }

    /* Push our response message into the TX queue. */
    this->pushMessageToQueue(response);

    /* Free our message wrapper. */
    delete response;
}

void Core::processDot15d4InputMessage(whad::dot15d4::Dot15d4Msg dot15d4Msg) {
    whad::NanoPbMsg *response = NULL;

    if (this->currentController != this->dot15d4Controller) {
        this->selectController(DOT15D4_PROTOCOL);
    }

    switch (dot15d4Msg.getType())
    {
        case whad::dot15d4::SniffModeMsg:
        {
            whad::dot15d4::SniffMode query(dot15d4Msg);

            int channel = query.getChannel();
            if (channel >= 11 && channel <= 26) {
                this->dot15d4Controller->setChannel(channel);
                this->dot15d4Controller->enterReceptionMode();
                this->dot15d4Controller->setAutoAcknowledgement(false);
                response = new whad::generic::Success();
            }
            else {
                response = new whad::generic::ParameterError();
            }            
        }
        break;

        case whad::dot15d4::EnergyDetectionMsg:
        {
            whad::dot15d4::EnergyDetect query(dot15d4Msg);

            int channel = query.getChannel();
            if (channel >= 11 && channel <= 26) {
                this->dot15d4Controller->setChannel(channel);
                this->dot15d4Controller->enterEDScanMode();
                response = new whad::generic::Success();
            }
            else {
                response = new whad::generic::ParameterError();
            }           
        }
        break;

        case whad::dot15d4::StartMsg:
        {
            this->currentController->start();
            response = new whad::generic::Success();
        }
        break;

        case whad::dot15d4::EndDeviceModeMsg:
        {
            whad::dot15d4::EndDeviceMode query(dot15d4Msg);

            int channel = query.getChannel();
            if (channel >= 11 && channel <= 26) {
                this->dot15d4Controller->setChannel(channel);
                this->dot15d4Controller->enterReceptionMode();
                this->dot15d4Controller->setAutoAcknowledgement(true);
                response = new whad::generic::Success();
            }
            else {
                response = new whad::generic::ParameterError();
            }
        }
        break;


        case whad::dot15d4::CoordModeMsg:
        {
            whad::dot15d4::CoordMode query(dot15d4Msg);

            int channel = query.getChannel();
            if (channel >= 11 && channel <= 26) {
                this->dot15d4Controller->setChannel(channel);
                this->dot15d4Controller->enterReceptionMode();
                this->dot15d4Controller->setAutoAcknowledgement(true);
                response = new whad::generic::Success();
            }
            else {
                response = new whad::generic::ParameterError();
            }
        }
        break;


        case whad::dot15d4::RouterModeMsg:
        {
            whad::dot15d4::RouterMode query(dot15d4Msg);

            int channel = query.getChannel();
            if (channel >= 11 && channel <= 26) {
                this->dot15d4Controller->setChannel(channel);
                this->dot15d4Controller->enterReceptionMode();
                this->dot15d4Controller->setAutoAcknowledgement(true);
                response = new whad::generic::Success();
            }
            else {
                response = new whad::generic::ParameterError();
            }            
        }
        break;

        case whad::dot15d4::SetNodeAddressMsg:
        {
            whad::dot15d4::SetNodeAddress query(dot15d4Msg);

            if (query.getAddressType() == whad::dot15d4::AddressShort) {
                uint16_t shortAddress = query.getAddress() & 0xFFFF;
                this->dot15d4Controller->setShortAddress(shortAddress);
                response = new whad::generic::Success();

            }
            else {
                uint64_t extendedAddress = query.getAddress();
                this->dot15d4Controller->setExtendedAddress(extendedAddress);
                response = new whad::generic::Success();
            }
        }
        break;


        case whad::dot15d4::StopMsg:
        {
            this->currentController->stop();
            response = new whad::generic::Success();
        }
        break;


        case whad::dot15d4::SendMsg:
        {
            whad::dot15d4::SendPdu query(dot15d4Msg);

            int channel = query.getChannel();
            
            if (channel >= 11 && channel <= 26) {
                /* Set channel. */
                this->dot15d4Controller->setChannel(channel);

                /* Build packet. */
                size_t size = query.getPdu().getSize();
                uint8_t *packet = (uint8_t*)malloc(1 + size);
                packet[0] = size;
                memcpy(packet+1,query.getPdu().getBytes(), size);
                this->dot15d4Controller->send(packet, size+1, false);
                free(packet);

                /* Success. */
                response = new whad::generic::Success();
            }
            else {
                response = new whad::generic::ParameterError();
            }
        }
        break;


        case whad::dot15d4::SendRawMsg:
        {
            whad::dot15d4::SendRawPdu query(dot15d4Msg);

            int channel = query.getChannel();
            uint16_t fcs = (uint16_t)(query.getFcs() & 0xFFFF);

            if (channel >= 11 && channel <= 26) {
                /* Set channel. */
                this->dot15d4Controller->setChannel(channel);

                /* Build packet. */
                size_t size = query.getPdu().getSize();
                uint8_t *packet = (uint8_t*)malloc(3 + size);
                packet[0] = size+2;
                memcpy(packet+1, query.getPdu().getBytes(), size);
                memcpy(packet+1+size, &fcs, 2);
                this->dot15d4Controller->send(packet, size+3, true);
                free(packet);        

                /* Success. */
                response = new whad::generic::Success();
            }
            else {
                response = new whad::generic::ParameterError();
            }            
        }
        break;

        default:
            response = new whad::generic::Error();
            break;
    }

    /* Push our response message into the TX queue. */
    this->pushMessageToQueue(response);

    /* Free our message wrapper. */
    delete response;
}

void Core::processBLEInputMessage(whad::ble::BleMsg bleMsg) {
    whad::NanoPbMsg *response = NULL;

    if (this->currentController != this->bleController) {
        this->selectController(BLE_PROTOCOL);
    }

    switch (bleMsg.getType())
    {
        case whad::ble::SniffAdvMsg:
        {
            whad::ble::SniffAdv query(bleMsg);
            uint8_t *bd_addr = query.getAddress().getAddressBuf();

            this->bleController->setChannel(query.getChannel());
            this->bleController->setAdvertisementsTransmitIndicator(true);
            this->bleController->setFilter(
                                            true,
                                            bd_addr[5],
                                            bd_addr[4],
                                            bd_addr[3],
                                            bd_addr[2],
                                            bd_addr[1],
                                            bd_addr[0]
            );
            this->bleController->setFollowMode(false);
            this->bleController->sniff();
            response = new whad::generic::Success();
        }
        break;

        case whad::ble::ReactiveJamMsg:
        {
            whad::ble::ReactiveJam query(bleMsg);
            
            this->bleController->setChannel(query.getChannel());
            this->bleController->setReactiveJammerConfiguration(query.getPattern(), query.getPatternLength(), query.getPosition());
            response = new whad::generic::Success();
        }
        break;

        case whad::ble::SniffConnReqMsg:
        {
            whad::ble::SniffConnReq query(bleMsg);
            uint8_t *bd_address = query.getTargetAddress().getAddressBuf();

            this->bleController->setChannel(query.getChannel());
            this->bleController->setAdvertisementsTransmitIndicator(query.mustReportAdv());
            this->bleController->setEmptyTransmitIndicator(query.mustReportEmpty());
            this->bleController->setFilter(
                false,
                bd_address[5],
                bd_address[4],
                bd_address[3],
                bd_address[2],
                bd_address[1],
                bd_address[0]
            );
            this->bleController->setFollowMode(true);
            this->bleController->sniff();
            response = new whad::generic::Success();
        }
        break;

        case whad::ble::SniffAAMsg:
        {
            whad::ble::SniffAccessAddress query(bleMsg);
            
            this->bleController->setChannel(0);
            this->bleController->setMonitoredChannels(query.getChannelMap().getChannelMapBuf());
            this->bleController->sniffAccessAddresses();
            response = new whad::generic::Success();
        }
        break;

        case whad::ble::SniffActConnMsg:
        {
            whad::ble::SniffActiveConn query(bleMsg);
            
            this->bleController->setChannel(0);
            this->bleController->setMonitoredChannels(query.getChannels().getChannelMapBuf());

            if (query.getCrcInit() == 0)
            {
                this->bleController->recoverCrcInit(query.getAccessAddress());
            }
            else if (query.getChannelMap().isNull()) {
                this->bleController->recoverChannelMap(query.getAccessAddress(), query.getCrcInit());
            }
            else if (query.getHopInterval() == 0) {
                this->bleController->recoverHopInterval(query.getAccessAddress(), query.getCrcInit(), query.getChannelMap().getChannelMapBuf());
            }
            else if (query.getHopIncrement() == 0) {
                this->bleController->recoverHopIncrement(query.getAccessAddress(), query.getCrcInit(), query.getChannelMap().getChannelMapBuf(), query.getHopInterval());
            }
            else {
                this->bleController->attachToExistingConnection(query.getAccessAddress(), query.getCrcInit(), query.getChannelMap().getChannelMapBuf(), query.getHopInterval(), query.getHopIncrement());
            }

            response = new whad::generic::Success();
        }
        break;

        case whad::ble::StartMsg:
        {
            this->bleController->start();
            response = new whad::generic::Success();
        }
        break;

        case whad::ble::StopMsg:
        {
            this->bleController->stop();
            response = new whad::generic::Success();
        }
        break;

        case whad::ble::ScanModeMsg:
        {
            whad::ble::ScanMode query(bleMsg);
            
            this->bleController->startScanning(query.isActiveModeEnabled());
            response = new whad::generic::Success();
        }
        break;

        case whad::ble::SendRawPduMsg:
        {
            whad::ble::SendRawPdu query(bleMsg);
            uint32_t max_retry = 1 << 24;

            switch (query.getDirection())
            {
                case whad::ble::DirectionInjectionToSlave:
                {
                     if (this->bleController->getState() == SNIFFING_CONNECTION) {
                        this->bleController->setAttackPayload(query.getPdu().getBytes(), query.getPdu().getSize());
                        this->bleController->startAttack(BLE_ATTACK_FRAME_INJECTION_TO_SLAVE);
                        response = new whad::generic::Success();
                    }
                    else {
                        response = new whad::generic::WrongMode();
                    }
                }
                break;

                case whad::ble::DirectionInjectionToMaster:
                {
                    if (this->bleController->getState() == SNIFFING_CONNECTION) {
                        this->bleController->setAttackPayload(query.getPdu().getBytes(), query.getPdu().getSize());
                            this->bleController->startAttack(BLE_ATTACK_FRAME_INJECTION_TO_MASTER);
                        response = new whad::generic::Success();
                    }
                    else {
                        response = new whad::generic::WrongMode();
                    }
                }
                break;

                case whad::ble::DirectionMasterToSlave:
                {
                    if (this->bleController->getState() == SIMULATING_MASTER || this->bleController->getState() == PERFORMING_MITM) {
                        this->bleController->setMasterPayload(query.getPdu().getBytes(), query.getPdu().getSize());
                        while (!max_retry && !this->bleController->isMasterPayloadTransmitted()) {--max_retry;}
                        if (max_retry != 0) {
                            response = new whad::generic::Success();
                        } else {
                            response = new whad::generic::Error();

                        }
                    }
                    else {
                        response = new whad::generic::WrongMode();
                    }
                }
                break;

                case whad::ble::DirectionSlaveToMaster:
                {
                    if (this->bleController->getState() == SIMULATING_SLAVE || this->bleController->getState() == PERFORMING_MITM) {

                        this->bleController->setSlavePayload(query.getPdu().getBytes(), query.getPdu().getSize());
                        while (!this->bleController->isSlavePayloadTransmitted()) {}
                        response = new whad::generic::Success();
                    }
                    else {
                        response = new whad::generic::WrongMode();
                    }
                }
                break;

                case whad::ble::DirectionUnknown:
                default:
                {
                    response = new whad::generic::ParameterError();
                }
                break;
            }
        }
        break;

        case whad::ble::HijackMasterMsg:
        {
            this->bleController->startAttack(BLE_ATTACK_MASTER_HIJACKING);
            response = new whad::generic::Success();            
        }
        break;

        case whad::ble::HijackSlaveMsg:
        {
            this->bleController->startAttack(BLE_ATTACK_SLAVE_HIJACKING);
            response = new whad::generic::Success();
        }
        break;

        case whad::ble::HijackBothMsg:
        {
            this->bleController->startAttack(BLE_ATTACK_MITM);
            response = new whad::generic::Success();
        }
        break;

        case whad::ble::CentralModeMsg:
        {
            //if (this->bleController->getState() == CONNECTION_INITIATION || this->bleController->getState() == SIMULATING_MASTER || this->bleController->getState() == PERFORMING_MITM) {
            response = new whad::generic::Success();
            /*}
            else {
                response = Whad::buildResultMessage(generic_ResultCode_WRONG_MODE);
            }*/
        }
        break;

        case whad::ble::PeriphModeMsg:
        {
            whad::ble::PeripheralMode query(bleMsg);

            if (this->bleController->getState() == SIMULATING_SLAVE || this->bleController->getState() == PERFORMING_MITM) {
                response = new whad::generic::Success();
            }
            else {
                this->bleController->advertise(
                    query.getAdvData(), query.getAdvDataLength(),
                    query.getScanRsp(), query.getScanRspLength(),
                    true,
                    100);
                this->bleController->start();
                response = new whad::generic::Success();
            }
        }
        break;

        case whad::ble::ConnectToMsg:
        {
            whad::ble::ConnectTo query(bleMsg);
            uint8_t *bd_address = query.getTargetAddr().getAddressBuf();

            this->bleController->setChannel(37);
            uint8_t address[6];
            address[0] = bd_address[5];
            address[1] = bd_address[4];
            address[2] = bd_address[3];
            address[3] = bd_address[2];
            address[4] = bd_address[1];
            address[5] = bd_address[0];

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

            if (query.getAccessAddr() != 0)
            {
                accessAddress = query.getAccessAddr();
            }

            if (!query.getChannelMap().isNull())
            {
                memcpy(channelMap, query.getChannelMap().getChannelMapBuf(), 5);
            }

            if (query.getCrcInit() != 0)
            {
                crcInit = query.getCrcInit();
            }

            if (query.getHopInterval() != 0)
            {
                hopInterval = query.getHopInterval();
            }

            if (query.getHopIncrement() != 0)
            {
                hopIncrement = query.getHopIncrement();
            }

            //this->bleController->setEmptyTransmitIndicator(true);

            this->bleController->connect(
                address,
                query.getTargetAddr().getType() == whad::ble::AddressRandom,
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

            response = new whad::generic::Success();
        }
        break;

        case whad::ble::DisconnectMsg:
        {
            this->bleController->disconnect();
            response = new whad::generic::Success();
        }
        break;

        case whad::ble::PrepareSeqMsg:
        {
            /* TODO: Implement pattern-based sequence. */
            
            SequenceDirection direction = BLE_TO_SLAVE;
            Trigger* trigger = NULL;
            int numberOfPackets = 0;
            PacketSequence *sequence = NULL;

            switch (whad::ble::PrepareSequence::getType(bleMsg))
            {
                case whad::ble::SequenceManual:
                {
                    whad::ble::PrepareSequenceManual query(bleMsg);

                    /* Change direction if targeted to master. */
                    if ((query.getDirection() == whad::ble::DirectionInjectionToMaster) ||
                        (query.getDirection() == whad::ble::DirectionSlaveToMaster))
                    {
                        direction = BLE_TO_MASTER;
                    }

                    /* Set trigger. */
                    trigger = new ManualTrigger();

                    /* Process sequence packets. */
                    numberOfPackets = query.getPackets().size();
                    sequence = this->sequenceModule->createSequence(numberOfPackets, trigger, direction, query.getId());
                    for (whad::ble::PDU& packet : query.getPackets())
                    {
                        sequence->preparePacket(packet.getBytes(), packet.getSize() , true);
                    }

                    /* Success. */
                    response = new whad::generic::Success();
                }
                break;

                case whad::ble::SequenceConnEvt:
                {
                    whad::ble::PrepareSequenceConnEvt query(bleMsg);

                    /* Change direction if targeted to master. */
                    if ((query.getDirection() == whad::ble::DirectionInjectionToMaster) ||
                        (query.getDirection() == whad::ble::DirectionSlaveToMaster))
                    {
                        direction = BLE_TO_MASTER;
                    }

                    /* Set trigger. */
                    trigger = new ConnectionEventTrigger(query.getConnEvt());

                    /* Process sequence packets. */
                    numberOfPackets = query.getPackets().size();
                    sequence = this->sequenceModule->createSequence(numberOfPackets, trigger, direction, query.getId());
                    for (whad::ble::PDU& packet : query.getPackets())
                    {
                        sequence->preparePacket(packet.getBytes(), packet.getSize() , true);
                    }

                    /* Success. */
                    response = new whad::generic::Success();
                }
                break;

                case whad::ble::SequencePattern:
                {
                    response = new whad::generic::WrongMode();
                }
                break;
            }
        }
        break;

        case whad::ble::PrepareSeqTriggerMsg:
        {
            whad::ble::ManualTrigger query(bleMsg);

            uint8_t id = query.getId();
            if (this->bleController->checkManualTriggers(id)) {
                response = new whad::generic::Success();
            }
            else {
                response = new whad::generic::Error();
            }
        }
        break;

        case whad::ble::SetBdAddressMsg:
        {
            whad::ble::SetBdAddress query(bleMsg);
            uint8_t *bd_address = query.getAddress()->getAddressBuf();

            uint8_t address[6] = {
                bd_address[5],
                bd_address[4],
                bd_address[3],
                bd_address[2],
                bd_address[1],
                bd_address[0]
            };
            this->bleController->setOwnAddress(address, false);
            response = new whad::generic::Success();
        }
        break;

        case whad::ble::PrepareSeqDeleteMsg:
        {
            whad::ble::DeleteSequence query(bleMsg);

            uint8_t id = query.getId();
            if (this->bleController->deleteSequence(id)) {
            response = new whad::generic::Success();
            }
            else {
            response = new whad::generic::ParameterError();
            }            
        }
        break;

        case whad::ble::SetEncryptionMsg:
        {
            whad::ble::SetEncryption query(bleMsg);

            if (query.isEnabled()) {
                this->bleController->configureEncryption(
                    query.getLLKey(),
                    query.getLLIv(),
                    0
                );
                if (this->bleController->startEncryption()) {
                    response = new whad::generic::Success();
                }
                else {
                    response = new whad::generic::Error();
                }
            }
            else {
                if (this->bleController->stopEncryption()) {
                    response = new whad::generic::Success();
                }
                else {
                    response = new whad::generic::Error();
                }
            }
        }
        break;

        default:
        {
            response = new whad::generic::Error();
        }
        break;
    }

    /* Push our response message into the TX queue. */
    this->pushMessageToQueue(response);

    /* Free our message wrapper. */
    delete response;
}


void Core::processESBInputMessage(whad::esb::EsbMsg esbMsg) {
    whad::NanoPbMsg *response = NULL;
    uint8_t address[5];
    uint8_t addressLen = 0;

    if (this->currentController != this->esbController) {
        this->selectController(ESB_PROTOCOL);
        this->esbController->disableUnifying();
    }

    /* Dispatch ESB message. */
    switch (esbMsg.getType())
    {
        /* Sniffing mode. */
        case whad::esb::SniffMsg:
        {
            /* Wrap our ESB message into a SniffMode message. */
            whad::esb::SniffMode query(esbMsg);

            /* If channel is valid (0xFF is a magic value to sniff on all channels). */
            int channel = query.getChannel();
            if (channel == 0xFF || (channel >= 0 && channel <= 100))
            {
                /* Retrieve the address length in bytes. */
                addressLen = query.getAddress().getLength();

                /* Make sure this length is valid (0 < length <= 5). */
                if ((addressLen > 0) && (addressLen <= 5))
                {
                    /* Copy address temporarily. */
                    memcpy(address, query.getAddress().getAddressBuf(), addressLen);

                    /* Set this address as a filter for our ESB controller. */
                    this->esbController->setFilter(
                        address[0],
                        address[1],
                        address[2],
                        address[3],
                        address[4]
                    );

                    /* Set channel information. */
                    this->esbController->setChannel(query.getChannel());

                    /* If acks must be reported, ask our controller to do so. */
                    if (query.mustShowAcks()) {
                        this->esbController->enableAcknowledgementsSniffing();
                    }
                    else {
                        this->esbController->disableAcknowledgementsSniffing();
                    }

                    /* Success ! */
                    response = new whad::generic::Success();
                }
                else
                {
                    /* Parameter error (wrong address size). */
                    response = new whad::generic::ParameterError();
                }
            }
            else {
                /* Parameter error (Invalid channel value). */
                response = new whad::generic::ParameterError();
            }
        }
        break;

        /* Start message. */
        case whad::esb::StartMsg:
        {
            /* Start our controller in current mode. */
            this->esbController->start();

            /* Success. */
            response = new whad::generic::Success();
        }
        break;

        /* Stop message. */
        case whad::esb::StopMsg:
        {
            /* Stop our controller (go to idle mode). */
            this->esbController->stop();

            /* Success. */
            response = new whad::generic::Success();            
        }
        break;

        /* Send raw packet message. */
        case whad::esb::SendRawMsg:
        {
            /* Wrap our esbMsg into a SendPacketRaw message. */
            whad::esb::SendPacketRaw query(esbMsg);

            int channel = query.getChannel();
            if (channel >= 0 && channel <= 100) {
                this->esbController->setChannel(channel);
            }
            if (this->esbController->send(query.getPacket().getBytes(), query.getPacket().getSize(), query.getRetrCount())) {
                /* Success. */
                response = new whad::generic::Success();
            }
            else {
                /* Error while sending packet. */
                response = new whad::generic::Error();
            }            
        }
        break;

        /* Set node address message. */
        case whad::esb::SetNodeAddrMsg:
        {
            /* Wrap our ESB message into a SetNodeAddress message. */
            whad::esb::SetNodeAddress query(esbMsg);

            /* Retrieve the address length in bytes. */
            addressLen = query.getAddress().getLength();

            /* Make sure this length is valid (0 < length <= 5). */
            if ((addressLen > 0) && (addressLen <= 5))
            {
                /* Copy address temporarily. */
                memcpy(address, query.getAddress().getAddressBuf(), addressLen);

                /* Set ESB address. */
                this->esbController->setFilter(
                    address[0],
                    address[1],
                    address[2],
                    address[3],
                    address[4]
                );

                /* Success. */
                response = new whad::generic::Success();            
            }
            else
            {
                /* Parameter error (invalid address size). */
                response = new whad::generic::ParameterError();
            }
        }
        break;

        /* Receiver mode message. */
        case whad::esb::PrxMsg:
        {
            /* Wrap our ESB message in a PrxMode message. */
            whad::esb::PrxMode query(esbMsg);

            /* Configure our controller in PRX (receiver) mode. */
            this->esbController->setChannel(query.getChannel());
            this->esbController->disableAcknowledgementsSniffing();
            this->esbController->enableAcknowledgementsTransmission();

            /* Success. */
            response = new whad::generic::Success();
        }
        break;

        /* Transmitter mode message. */
        case whad::esb::PtxMsg:
        {
            /* Wrap our ESB message into a PtxMode message. */
            whad::esb::PtxMode query(esbMsg);

            /* Configure our ESB controller in PTX (transmitter) mode. */
            this->esbController->setChannel(query.getChannel());
            this->esbController->enableAcknowledgementsSniffing();
            this->esbController->disableAcknowledgementsTransmission();

            /* Success. */
            response = new whad::generic::Success();
        }
        break;

        case whad::esb::UnknownMsg:
        default:
        {
            /* Error (unknown message). */
            response = new whad::generic::Error();
        }
        break;

    }

    /* Push our response message into the TX queue. */
    this->pushMessageToQueue(response);

    /* Free our message wrapper. */
    delete response;
}

void Core::processUnifyingInputMessage(whad::unifying::UnifyingMsg uniMsg) {
    whad::NanoPbMsg *response = NULL;
    uint8_t address[5];
    uint8_t addressLen = 0;

    if (this->currentController != this->esbController) {
    this->selectController(ESB_PROTOCOL);
    this->esbController->enableUnifying();
    }

    /* Dispatch messages. */
    switch (uniMsg.getType())
    {
        /* Sniffing mode message. */
        case whad::unifying::SniffModeMsg:
        {
            /* Wrap our Unifying message into a SniffMode message. */
            whad::unifying::SniffMode query(uniMsg);

            /* Check channel validity. */
            int channel = query.getChannel();
            if (channel == 0xFF || (channel >= 0 && channel <= 100))
            {

                addressLen = query.getAddress().getLength();
                if ((addressLen > 0) && (addressLen <= 5))
                {
                    /* Copy address. */
                    memcpy(address, query.getAddress().getBytes(), addressLen);

                    /* Set address. */
                    this->esbController->setFilter(
                        address[0],
                        address[1],
                        address[2],
                        address[3],
                        address[4]
                    );

                    /* Set channel for our ESB controller. */
                    this->esbController->setChannel(channel);

                    /* Enable acks if required. */
                    if (query.mustShowAcks()) {
                        this->esbController->enableAcknowledgementsSniffing();
                    }
                    else {
                        this->esbController->disableAcknowledgementsSniffing();
                    }

                    /* Success. */
                    response = new whad::generic::Success();
                }
                else
                {
                    /* Parameter error (invalid address size). */
                    response = new whad::generic::ParameterError();
                }
            }
            else
            {
                /* Parameter error (invalid channel). */
                response = new whad::generic::ParameterError();
            }            
        }
        break;

        /* Start message. */
        case whad::unifying::StartMsg:
        {
            /* Start controller in current mode. */
            this->esbController->start();

            /* Success. */
            response = new whad::generic::Success();            
        }
        break;

        /* Stop message. */
        case whad::unifying::StopMsg:
        {
            /* Stop controller. */
            this->esbController->stop();

            /* Success. */
            response = new whad::generic::Success();
        }
        break;

        /* Send raw packet message. */
        case whad::unifying::SendRawMsg:
        {
            /* Wrap our Unifying message into a SendRawPdu message. */
            whad::unifying::SendRawPdu query(uniMsg);

            /* Check channel. */
            int channel = query.getChannel();
            if (channel >= 0 && channel <= 100) {

                /* Set controller channel. */
                this->esbController->setChannel(channel);

                /* Send packet. */
                if (this->esbController->send(query.getPdu().getBytes(), query.getPdu().getSize(), query.getRetrCounter()))
                {
                    /* Success. */
                    response = new whad::generic::Success();
                }
                else
                {
                    /* Error while sending packet. */
                    response = new whad::generic::Error();
                }
            }
            else
            {
                /* Parameter error (invalid channel). */
                response = new whad::generic::ParameterError();
            }
        }
        break;

        /* Set node address message. */
        case whad::unifying::SetNodeAddressMsg:
        {
            /* Wrap our Unifying message into a SetNodeAddress message. */
            whad::unifying::SetNodeAddress query(uniMsg);

            /* Check address. */
            addressLen = query.getAddress().getLength();
            if ((addressLen > 0) && (addressLen <= 5))
            {
                /* Copy address. */
                memcpy(address, query.getAddress().getBytes(), addressLen);

                /* Set controller address. */
                this->esbController->setFilter(
                    address[0],
                    address[1],
                    address[2],
                    address[3],
                    address[4]
                );

                /* Success. */
                response = new whad::generic::Success();
            }
            else
            {
                /* Parameter error (invalid address size). */
                response = new whad::generic::ParameterError();
            }
        }
        break;

        /* Dongle mode message. */
        case whad::unifying::DongleModeMsg:
        {
            /* Wrap our Unifying message into a DongleMode message. */
            whad::unifying::DongleMode query(uniMsg);

            /* Check channel value. */
            int channel = query.getChannel();
            if (((channel >= 0) && (channel <= 100)) || (channel == 0xFF))
            {
                /* Configure our ESB controller accordingly. */
                this->esbController->setChannel(channel);
                this->esbController->disableAcknowledgementsSniffing();
                this->esbController->enableAcknowledgementsTransmission();

                /* Success. */
                response = new whad::generic::Success();
            }
            else
            {
                /* Parametter error (invalid channel). */
                response = new whad::generic::ParameterError();
            }
        }
        break;

        /* Mouse mode message. */
        case whad::unifying::MouseModeMsg:
        {
            /* Wrap our Unifying message into a MouseMode message. */
            whad::unifying::MouseMode query(uniMsg);

            /* Check channel value. */
            int channel = query.getChannel();
            if (((channel >= 0) && (channel <= 100)) || (channel == 0xFF))
            {
                /* Configure our ESB controller accordingly. */
                this->esbController->setChannel(channel);
                this->esbController->disableAcknowledgementsSniffing();
                this->esbController->enableAcknowledgementsTransmission();

                /* Success. */
                response = new whad::generic::Success();
            }
            else
            {
                /* Parametter error (invalid channel). */
                response = new whad::generic::ParameterError();
            }
        }
        break;

        /* Keyboard mode message. */
        case whad::unifying::KeyboardModeMsg:
        {
            /* Wrap our Unifying message into a KeyboardMode message. */
            whad::unifying::KeyboardMode query(uniMsg);

            /* Check channel value. */
            int channel = query.getChannel();
            if (((channel >= 0) && (channel <= 100)) || (channel == 0xFF))
            {
                /* Configure our ESB controller accordingly. */
                this->esbController->setChannel(channel);
                this->esbController->disableAcknowledgementsSniffing();
                this->esbController->enableAcknowledgementsTransmission();

                /* Success. */
                response = new whad::generic::Success();
            }
            else
            {
                /* Parametter error (invalid channel). */
                response = new whad::generic::ParameterError();
            }
        }
        break;

        /* Pairing sniffing mode. */
        case whad::unifying::SniffPairingMsg:
        {
            /*
             * Put our ESB controller in sniffing mode in order to sniff
             * Logitech Unifying pairing requests on channel 5.
             */
            this->esbController->setFilter(0xBB, 0x0A, 0xDC, 0xA5, 0x75);
            this->esbController->setChannel(5);

            /* Success. */
            response = new whad::generic::Success();            
        }
        break;

        /* Unkown message. */
        case whad::unifying::UnknownMsg:
        default:
        {
            /* Unkown message error. */
            response = new whad::generic::Error();
        }
        break;
    }

    /* Push our response message into the TX queue. */
    this->pushMessageToQueue(response);

    /* Free our message wrapper. */
    delete response;
}

void Core::processPhyInputMessage(whad::phy::PhyMsg msg) {
    whad::NanoPbMsg *response = NULL;

    if (this->currentController != this->genericController) {
    this->selectController(GENERIC_PROTOCOL);
    }

    switch (msg.getType())
    {
        case whad::phy::SetGfskModMsg:
        {
            whad::phy::SetGfskMod query(msg);

            switch (query.getDeviation())
            {
                case 170000:
                    {
                        this->genericController->setPhy(GENERIC_PHY_1MBPS_ESB);
                        response = new whad::generic::Success();
                    }
                    break;

                case 250000:
                    {
                        response = new whad::generic::Success();
                        this->genericController->setPhy(GENERIC_PHY_1MBPS_BLE);
                    }
                    break;

                case 320000:
                    {
                        this->genericController->setPhy(GENERIC_PHY_2MBPS_ESB);
                        response = new whad::generic::Success();
                    }
                    break;

                case 500000:
                    {
                        this->genericController->setPhy(GENERIC_PHY_2MBPS_BLE);
                        response = new whad::generic::Success();
                    }
                    break;

                default:
                    /* Error, deviation is not supported. */
                    response = new whad::generic::ParameterError();
                    break;

            }
        }
        break;

        case whad::phy::SendMsg:
        {
            whad::phy::SendPacket query(msg);
            whad::phy::Packet packet = query.getPacket();
            this->genericController->send(packet.getBytes(), packet.getSize());
            response = new whad::generic::Success();
        }
        break;

        case whad::phy::GetSupportedFreqsMsg:
        {
            response = new whad::phy::SupportedFreqsResp(SUPPORTED_FREQUENCY_RANGES);
        }
        break;

        case whad::phy::SetFreqMsg:
        {
            whad::phy::SetFreq query(msg);

            uint64_t frequency = query.getFrequency();
            if (frequency >= 2400000000L && frequency <= 2500000000L) {
                int frequency_offset = (frequency / 1000000) - 2400;
                this->genericController->setChannel(frequency_offset);
                response = new whad::generic::Success();
            }
            else {
                response = new whad::generic::ParameterError();
            }            
        }
        break;

        case whad::phy::SetDatarateMsg:
        {
            whad::phy::SetDatarate query(msg);

            switch (query.getDatarate())
            {
                case 1000000:
                    {
                        if (
                            this->genericController->getPhy() == GENERIC_PHY_1MBPS_ESB ||
                            this->genericController->getPhy() == GENERIC_PHY_2MBPS_ESB
                        ) {
                            this->genericController->setPhy(GENERIC_PHY_1MBPS_ESB);
                            response = new whad::generic::Success();
                        }
                        else if (
                            this->genericController->getPhy() == GENERIC_PHY_1MBPS_BLE ||
                            this->genericController->getPhy() == GENERIC_PHY_2MBPS_BLE
                        ) {
                            this->genericController->setPhy(GENERIC_PHY_1MBPS_BLE);
                            response = new whad::generic::Success();
                        }
                        else {
                            response = new whad::generic::Error();
                        }
                    }
                    break;

                case 2000000:
                    {
                        if (
                            this->genericController->getPhy() == GENERIC_PHY_1MBPS_ESB ||
                            this->genericController->getPhy() == GENERIC_PHY_2MBPS_ESB
                        ) {
                            this->genericController->setPhy(GENERIC_PHY_2MBPS_ESB);
                            response = new whad::generic::Success();
                        }
                        else if (
                            this->genericController->getPhy() == GENERIC_PHY_1MBPS_BLE ||
                            this->genericController->getPhy() == GENERIC_PHY_2MBPS_BLE
                        ) {
                            this->genericController->setPhy(GENERIC_PHY_2MBPS_BLE);
                            response = new whad::generic::Success();
                        }
                        else {
                            response = new whad::generic::Error();
                        }
                    }
                    break;

                default:
                    response = new whad::generic::ParameterError();
                    break;
            }
        }
        break;

        case whad::phy::SetEndiannessMsg:
        {
            whad::phy::SetEndianness query(msg);
            if (query.getEndianness() == whad::phy::PhyBigEndian) {
                this->genericController->setEndianness(GENERIC_ENDIANNESS_BIG);
            }
            else {
                this->genericController->setEndianness(GENERIC_ENDIANNESS_LITTLE);
            }
            response = new whad::generic::Success();
        }
        break;

        case whad::phy::SetTxPowerMsg:
        {
            whad::phy::SetTxPower query(msg);

            if (query.getPower() == whad::phy::PhyTxPowerLow) {
                this->genericController->setTxPower(LOW);
            }
            else if (query.getPower() == whad::phy::PhyTxPowerMedium) {
                this->genericController->setTxPower(MEDIUM);
            }
            else {
                this->genericController->setTxPower(HIGH);
            }
            response = new whad::generic::Success();
        }
        break;

        case whad::phy::SetPacketSizeMsg:
        {
            whad::phy::SetPacketSize query(msg);

            if (query.getSize() <= 252) {
                this->genericController->setPacketSize(query.getSize());
                response = new whad::generic::Success();
            }
            else {
                response = new whad::generic::ParameterError();
            }
        }
        break;

        case whad::phy::SetSyncWordMsg:
        {
            whad::phy::SetSyncWord query(msg);
            this->genericController->setPreamble(query.get().get(), query.get().getSize());
            response = new whad::generic::Success();
        }
        break;

        case whad::phy::SetSniffModeMsg:
        {
            whad::phy::SniffMode query(msg);

            if (query.isIqModeEnabled()) {
                response = new whad::generic::Error();
            }
            else {
                response = new whad::generic::Success();
            }
        }
        break;

        case whad::phy::StartMsg:
        {
            this->genericController->start();
            response = new whad::generic::Success();
        }
        break;

        case whad::phy::StopMsg:
        {
            this->genericController->stop();
            response = new whad::generic::Success();
        }
        break;

        default:
        {
            response = new whad::generic::Error();
        }
        break;
    }

    /* Push our response message into the TX queue. */
    this->pushMessageToQueue(response);

    /* Free our message wrapper. */
    delete response;
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

void Core::pushMessageToQueue(whad::NanoPbMsg *msg) {
	MessageQueueElement *element = (MessageQueueElement*)malloc(sizeof(MessageQueueElement));
	element->message = msg->getRaw();
	element->nextElement = NULL;
	if (this->messageQueue.size == 0) {
		this->messageQueue.firstElement = element;
		this->messageQueue.lastElement = element;
	}
	else {
		/* We insert the message at the end of the queue. */
		this->messageQueue.lastElement->nextElement = element;
		this->messageQueue.lastElement = element;
	}
	this->messageQueue.size = this->messageQueue.size + 1;
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
		/* We insert the message at the end of the queue. */
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
