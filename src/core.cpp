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
    default:
      // send error ?
      break;
  };
}

void Core::processGenericInputMessage(generic_Message msg) {
  this->sendVerbose("generic");
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

  else if (msg.which_msg == zigbee_Message_stop_tag) {
    this->currentController->stop();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
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
                                    msg.msg.sniff_adv.bd_address[5],
                                    msg.msg.sniff_adv.bd_address[4],
                                    msg.msg.sniff_adv.bd_address[3],
                                    msg.msg.sniff_adv.bd_address[2],
                                    msg.msg.sniff_adv.bd_address[1],
                                    msg.msg.sniff_adv.bd_address[0]
    );
    this->bleController->setFollowMode(false);

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
                                    msg.msg.sniff_connreq.bd_address[5],
                                    msg.msg.sniff_connreq.bd_address[4],
                                    msg.msg.sniff_connreq.bd_address[3],
                                    msg.msg.sniff_connreq.bd_address[2],
                                    msg.msg.sniff_connreq.bd_address[1],
                                    msg.msg.sniff_connreq.bd_address[0]
    );
    this->bleController->setFollowMode(true);

    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }
  else if (msg.which_msg == ble_Message_start_tag) {
    this->currentController->start();
    response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
  }

  else if (msg.which_msg == ble_Message_stop_tag) {
    this->currentController->stop();
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
      if (this->bleController->getState() == SIMULATING_MASTER || this->bleController->getState() == PERFORMING_MITM) {
        response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
      }
      else {
        response = Whad::buildResultMessage(generic_ResultCode_WRONG_MODE);
      }
  }

  else if (msg.which_msg == ble_Message_periph_mode_tag) {
      if (this->bleController->getState() == SIMULATING_SLAVE || this->bleController->getState() == PERFORMING_MITM) {
        response = Whad::buildResultMessage(generic_ResultCode_SUCCESS);
      }
      else {
        response = Whad::buildResultMessage(generic_ResultCode_WRONG_MODE);
      }
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
	this->linkModule = new LinkModule();
	this->serialModule = new SerialComm((CoreCallback)&Core::handleInputData,this);
	this->radio = new Radio();

	#ifdef PA_ENABLED
	this->configurePowerAmplifier(true);
	#endif

}

LedModule* Core::getLedModule() {
	return (this->ledModule);
}

SerialComm *Core::getSerialModule() {
	return (this->serialModule);
}

LinkModule* Core::getLinkModule() {
	return (this->linkModule);
}

TimerModule* Core::getTimerModule() {
	return (this->timerModule);
}

Radio* Core::getRadioModule() {
	return (this->radio);
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
	if (controller == BLE_PROTOCOL) {
		this->radio->disable();
		this->currentController = this->bleController;
		this->radio->setController(this->currentController);
		return true;
	}
	else if (controller == DOT15D4_PROTOCOL) {
		this->radio->disable();
		this->currentController = this->dot15d4Controller;
		this->radio->setController(this->currentController);
		return true;
	}
	else if (controller == ESB_PROTOCOL) {
		this->radio->disable();
		this->currentController = this->esbController;
		this->radio->setController(this->currentController);
		return true;
	}
	else if (controller == ANT_PROTOCOL) {
		this->radio->disable();
		this->currentController = this->antController;
		this->radio->setController(this->currentController);
		return true;
	}
	else if (controller == MOSART_PROTOCOL) {
		this->radio->disable();
		this->currentController = this->mosartController;
		this->radio->setController(this->currentController);
		return true;
	}
	else if (controller == GENERIC_PROTOCOL) {
		this->radio->disable();
		this->currentController = this->genericController;
		this->radio->setController(this->currentController);
		return true;
	}
	else {
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

void Core::sendMessage(Message *msg) {
  uint8_t buffer[1024];
  size_t size = Whad::encodeMessage(msg, buffer, 1024);
	this->serialModule->send(buffer,size);
	free(msg);
}

void Core::sendVerbose(const char* data) {
  Message *msg = Whad::buildVerboseMessage(data);
  this->pushMessageToQueue(msg);
}

/*
void Core::handleCommand(Command *cmd) {

	if (cmd->getCommandType() == GET_VERSION) {
		this->pushMessageToQueue(new GetVersionResponse(VERSION_MAJOR,VERSION_MINOR));
	}
	else if (cmd->getCommandType() == SELECT_CONTROLLER) {
		ProtocolParameters parameters(cmd->getParameters(),cmd->getParametersSize());
		bool success = this->selectController(parameters.getProtocol());
		this->pushMessageToQueue(new SelectControllerResponse(success));
	}
	else if (cmd->getCommandType() == ENABLE_CONTROLLER) {
		this->pushMessageToQueue(new EnableControllerResponse(this->currentController != NULL));
		if (this->currentController != NULL) {
			this->currentController->start();
		}
	}

	else if (cmd->getCommandType() == DISABLE_CONTROLLER) {
		bool success = false;
		if (this->currentController != NULL) {
			this->currentController->stop();
			success = true;
		}
		this->pushMessageToQueue(new DisableControllerResponse(success));
	}
	else if (cmd->getCommandType() == GET_CHANNEL) {
		uint8_t channel = 0xFF;
		if (this->currentController == this->bleController) {
			channel = this->bleController->getChannel();
		}
		else if (this->currentController == this->dot15d4Controller) {
			channel = this->dot15d4Controller->getChannel();
		}
		else if (this->currentController == this->esbController) {
			channel = this->esbController->getChannel();
		}
		else if (this->currentController == this->antController) {
			channel = this->antController->getChannel();
		}
		else if (this->currentController == this->mosartController) {
			channel = this->mosartController->getChannel();
		}
		else if (this->currentController == this->genericController) {
			channel = this->genericController->getChannel();
		}
		this->pushMessageToQueue(new GetChannelResponse(channel));
	}
	else if (cmd->getCommandType() == SET_CHANNEL) {
		ChannelParameters parameters(cmd->getParameters(),cmd->getParametersSize());
		bool status = false;
		if (this->currentController == this->bleController) {
			if (parameters.getChannel() >= 0 && parameters.getChannel() <= 39) {
				this->bleController->setChannel(parameters.getChannel());
				status = true;
			}
		}
		else if (this->currentController == this->dot15d4Controller) {
			if (parameters.getChannel() >= 11 && parameters.getChannel() <= 26) {
				this->dot15d4Controller->setChannel(parameters.getChannel());
				status = true;
			}
		}
		else if (this->currentController == this->esbController) {
			if (parameters.getChannel() >= 0 && parameters.getChannel() <= 100) {
				this->esbController->setAutofind(false);
				this->esbController->setChannel(parameters.getChannel());
				status = true;
			}
			else if (parameters.getChannel() == 0xFF) {
				this->esbController->setAutofind(true);
			}
		}
		else if (this->currentController == this->antController) {
			if (parameters.getChannel() >= 0 && parameters.getChannel() <= 100) {
				this->antController->setChannel(parameters.getChannel());
				status = true;
			}
		}
		else if (this->currentController == this->mosartController) {
			if (parameters.getChannel() >= 0 && parameters.getChannel() <= 100) {
				this->mosartController->setChannel(parameters.getChannel());
				status = true;
			}
		}
		else if (this->currentController == this->genericController) {
			if (parameters.getChannel() >= 0 && parameters.getChannel() <= 100) {
				this->genericController->setChannel(parameters.getChannel());
				status = true;
			}
		}
		this->pushMessageToQueue(new SetChannelResponse(status));
	}
	else if (cmd->getCommandType() == SET_FILTER) {
		bool status = false;
		if (this->currentController == this->bleController) {
			this->bleController->setFilter(cmd->getParameters()[0],cmd->getParameters()[1],cmd->getParameters()[2],cmd->getParameters()[3],cmd->getParameters()[4],cmd->getParameters()[5]);
			status = true;
		}
		else if (this->currentController == this->esbController) {
			this->esbController->setFilter(cmd->getParameters()[0],cmd->getParameters()[1],cmd->getParameters()[2],cmd->getParameters()[3],cmd->getParameters()[4]);
			status = true;
		}
		else if (this->currentController == this->antController) {
			uint16_t preamble = (cmd->getParameters()[0] << 8) | cmd->getParameters()[1];
			uint16_t deviceNumber = (cmd->getParameters()[2] << 8) | cmd->getParameters()[3];
			uint8_t deviceType = cmd->getParameters()[4];
			this->antController->setFilter(preamble, deviceNumber, deviceType);
		}

		else if (this->currentController == this->mosartController) {
			bool enableDonglePackets = (cmd->getParameters()[0] == 0x01);
			this->mosartController->setFilter(cmd->getParameters()[1],cmd->getParameters()[2],cmd->getParameters()[3],cmd->getParameters()[4]);
			if (enableDonglePackets) {
				this->mosartController->enableDonglePackets();
			}
			else {
				this->mosartController->disableDonglePackets();
			}
		}
		else if (this->currentController == this->genericController) {
			size_t preambleSize = (cmd->getParameters()[0] >> 4);
			GenericEndianness endianness = (GenericEndianness)((cmd->getParameters()[0] >> 2) & 1);
			GenericPhy phy = (GenericPhy)(cmd->getParameters()[0] & 3);
			size_t packetSize = cmd->getParameters()[1];
			uint8_t preamble[4];
			for (size_t i=0;i<preambleSize;i++) {
				preamble[i] = cmd->getParameters()[2+i];
			}
			this->genericController->configure(preamble, preambleSize, packetSize, phy, endianness);
		}
		this->pushMessageToQueue(new SetFilterResponse(status));
	}

	else if (cmd->getCommandType() == SET_FOLLOW_MODE) {
		BooleanParameters parameters(cmd->getParameters(),cmd->getParametersSize());
		bool status = false;
		if (this->currentController == this->bleController) {
			this->bleController->setFollowMode(parameters.getBoolean());
			status = true;
		}
		if (this->currentController == this->esbController) {
			this->esbController->setFollowMode(parameters.getBoolean());
			status = true;
		}
		this->pushMessageToQueue(new SetFollowModeResponse(status));
	}
	else if (cmd->getCommandType() == START_ATTACK) {
		bool status = false;
		if (this->currentController == this->bleController) {
			if (cmd->getParameters()[0] == 0x01) {
				this->bleController->startAttack(BLE_ATTACK_FRAME_INJECTION);
				status = true;
			}
			if (cmd->getParameters()[0] == 0x02) {
				this->bleController->startAttack(BLE_ATTACK_SLAVE_HIJACKING);
				status = true;
			}
			else if (cmd->getParameters()[0] == 0x03) {
				this->bleController->startAttack(BLE_ATTACK_MASTER_HIJACKING);
				status = true;
			}
			else if (cmd->getParameters()[0] == 0x04) {
				this->bleController->startAttack(BLE_ATTACK_MITM);
				status = true;
			}
		}
		else if (this->currentController == this->dot15d4Controller) {
			if (cmd->getParameters()[0] == 0x00) {
				this->dot15d4Controller->startAttack(DOT15D4_ATTACK_NONE);
				status = true;
			}
			else if (cmd->getParameters()[0] == 0x01) {
				this->dot15d4Controller->startAttack(DOT15D4_ATTACK_JAMMING);
				status = true;
			}
			else if (cmd->getParameters()[0] == 0x02) {
				this->dot15d4Controller->startAttack(DOT15D4_ATTACK_CORRECTION);
				status = true;
			}
		}
		else if (this->currentController == this->esbController) {
			if (cmd->getParameters()[0] == 0x00) {
				this->esbController->startAttack(ESB_ATTACK_NONE);
				status = true;
			}
			else if (cmd->getParameters()[0] == 0x01) {
				this->esbController->startAttack(ESB_ATTACK_SCANNING);
				status = true;
			}
			else if (cmd->getParameters()[0] == 0x02) {
				this->esbController->startAttack(ESB_ATTACK_SNIFF_LOGITECH_PAIRING);
				status = true;
			}
			else if (cmd->getParameters()[0] == 0x03) {
				this->esbController->startAttack(ESB_ATTACK_JAMMING);
				status = true;
			}

		}
		else if (this->currentController == this->antController) {
			if (cmd->getParameters()[0] == 0x00) {
				this->antController->startAttack(ANT_ATTACK_NONE);
				status = true;
			}
			else if (cmd->getParameters()[0] == 0x01) {
				this->antController->startAttack(ANT_ATTACK_JAMMING);
				status = true;
			}
			else if (cmd->getParameters()[0] == 0x02) {
				this->antController->startAttack(ANT_ATTACK_MASTER_HIJACKING);
				status = true;
			}
		}
		else if (this->currentController == this->genericController) {
			if (cmd->getParameters()[0] == 0x00) {
				this->genericController->startAttack(GENERIC_ATTACK_NONE);
				status = true;
			}
			else if (cmd->getParameters()[0] == 0x01) {
				this->genericController->startAttack(GENERIC_ATTACK_JAMMING);
				status = true;
			}
			else if (cmd->getParameters()[0] == 0x02) {
				this->genericController->startAttack(GENERIC_ATTACK_ENERGY_DETECTION);
				status = true;
			}
		}
		this->pushMessageToQueue(new StartAttackResponse(status));
	}
	else if (cmd->getCommandType() == SEND_PAYLOAD) {
		PayloadParameters parameters(cmd->getParameters(),cmd->getParametersSize());
		//bool status = false;
		if (this->currentController == this->bleController) {
			if (parameters.getPayloadDirection() == 0x00) {
				this->bleController->setAttackPayload(parameters.getPayloadContent(),parameters.getPayloadSize());
				this->pushMessageToQueue(new SendPayloadResponse(0x00,true));
			}
			else if (parameters.getPayloadDirection() == 0x01) {
				if (this->bleController->isMasterPayloadTransmitted()) {
					this->bleController->setMasterPayload(parameters.getPayloadContent(),parameters.getPayloadSize());
				}
			}
			else if (parameters.getPayloadDirection() == 0x02) {
				if (this->bleController->isMasterPayloadTransmitted()) {
					this->bleController->setSlavePayload(parameters.getPayloadContent(),parameters.getPayloadSize());
				}
			}
		}
		else if (this->currentController == this->dot15d4Controller) {
			this->dot15d4Controller->send(parameters.getPayloadContent(),parameters.getPayloadSize());
		}
		else if (this->currentController == this->esbController) {
			this->esbController->send(parameters.getPayloadContent(),parameters.getPayloadSize());
		}
		else if (this->currentController == this->antController) {
			if (parameters.getPayloadDirection() == 0x00) {
				this->antController->send(parameters.getPayloadContent(),parameters.getPayloadSize());
			}
			else if (parameters.getPayloadDirection() == 0x01) {
				this->antController->setAttackPayload(parameters.getPayloadContent(),parameters.getPayloadSize());
			}
			else if (parameters.getPayloadDirection() == 0x02) {
				this->antController->sendResponsePacket(parameters.getPayloadContent(),parameters.getPayloadSize());
			}
		}

		else if (this->currentController == this->genericController) {
			this->genericController->send(parameters.getPayloadContent(), parameters.getPayloadSize());
		}
	}

	else if (cmd->getCommandType() == CONFIGURE_LINK) {
		bool status = true;
		LinkParameters parameters(cmd->getParameters(),cmd->getParametersSize());
		this->linkModule->configureLink((LinkMode)parameters.getMode(), parameters.getControlPin());
		this->pushMessageToQueue(new ConfigureLinkResponse(status));
	}


	delete cmd;
}
*/

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
		__WFE();
	}
}
