#include "controller.h"
#include "core.h"
#include "whad.h"

Controller::Controller(Radio *radio) {
	this->radio = radio;
}

/**
 * @brief Add packet to the list of packets to be sent to the host
 * 
 * @param[in]   packet      Pointer to a `Packet` object containing the packet
 *                          information.
 */

void Controller::addPacket(Packet* packet) {
    /* Build a WHAD notification message from packet. */
    whad::NanoPbMsg *message = buildMessageFromPacket(packet);

    /* Add notification to our message queue. */
	Core::instance->pushMessageToQueue(message);

    /* Free the notification wrapper. */
    delete message;
}


/**
 * @brief   Send debug message to the host.
 * 
 * @param[in]   msg     Pointer to a text string to send as a debug message
 */

void Controller::sendDebug(const char* msg) {
    /* Craft a verbose message. */
    whad::NanoPbMsg *message = new whad::generic::Verbose(msg);

	/* Add notification to our message queue. */
    Core::instance->pushMessageToQueue(message);
    
    /* Free the notification wrapper. */
    delete message;
}


/**
 * @brief   Create a WHAD notification message from a received packet.
 * 
 * @param[in]   packet  Pointer to a received `Packet` object.
 * @retval      Pointer to a `whad::NanoPbMsg` ready to be sent to host
 */

whad::NanoPbMsg *Controller::buildMessageFromPacket(Packet* packet) {
  whad::NanoPbMsg *message = NULL;

  if (packet->getPacketType() == BLE_PACKET_TYPE) {
    BLEPacket *blePacket = static_cast<BLEPacket*>(packet);

    /* Craft a BLE Raw PDU packet notification. */
    message = new whad::ble::RawPdu(
        blePacket->getChannel(),
        blePacket->getRssi(),
        blePacket->getConnectionHandle(),
        blePacket->getAccessAddress(),
        whad::ble::PDU(packet->getPacketBuffer()+4, blePacket->extractPayloadLength() + 2),
        blePacket->getCrc(),
        blePacket->isCrcValid(),
        blePacket->getTimestamp(),
        (blePacket->getAccessAddress()==0x8e89bed6)?0:blePacket->getRelativeTimestamp(),
        (whad::ble::Direction)blePacket->getSource(),
        false,
        false
    );
  }
  else if (packet->getPacketType() == DOT15D4_PACKET_TYPE) {
    Dot15d4Packet *zigbeePacket = static_cast<Dot15d4Packet*>(packet);

    /* Craft a ZigBee raw PDU notification. */
    whad::zigbee::ZigbeePacket packet(
        zigbeePacket->getChannel(),
        zigbeePacket->getPacketBuffer()+1,
        zigbeePacket->getPacketSize()-3,
        zigbeePacket->getFCS()
    );
    packet.addLqi(zigbeePacket->getLQI());
    packet.addFcsValidity(zigbeePacket->isCrcValid());
    packet.addRssi(zigbeePacket->getRssi());
    packet.addTimestamp(zigbeePacket->getTimestamp());
    message = new whad::zigbee::RawPduReceived(packet);
  }
  else if (packet->getPacketType() == ESB_PACKET_TYPE) {
    ESBPacket *esbPacket = static_cast<ESBPacket*>(packet);
    whad::unifying::RawPduReceived *uniRawPduRecvd = NULL;

    /* Specific processing if ESB packet contains Logitech Unifying data. */
    if (esbPacket->isUnifying())
    {
        /* Create a Unifying packet from ESBPacket. */
        whad::unifying::PDU packet(esbPacket->getPacketBuffer(), esbPacket->getPacketSize());

        /* Create a Unifying address. */
        whad::unifying::UnifyingAddress address(esbPacket->getAddress(), 5);

        /* Create a Logitech Unifying packet. */
        whad::unifying::UnifyingPacket uniPacket(
            esbPacket->getChannel(),
            esbPacket->getPacketBuffer(),
            esbPacket->getPacketSize(),
            esbPacket->getCrc(),
            esbPacket->getRssi(),
            esbPacket->getTimestamp()
        );

        /* Add CRC validity if provided. */
        uniPacket.addCrcValidity(esbPacket->isCrcValid());

        /* Create a RawPacketReceived notification. */
        uniRawPduRecvd = new whad::unifying::RawPduReceived(uniPacket);
        uniRawPduRecvd->addAddress(address);

        /* Send our RawPacketReceived notification. */
        message = dynamic_cast<whad::NanoPbMsg*>(uniRawPduRecvd);
    } 
    else
    {
        whad::esb::RawPacketReceived *esbRawPacketRecvd = NULL;

        /* Create an ESB packet from ESBPacket. */
        whad::esb::Packet packet(esbPacket->getPacketBuffer(), esbPacket->getPacketSize());

        /* Extract ESB address. */
        whad::esb::EsbAddress address(esbPacket->getAddress(), 5);

        /* Create a RawPacketReceived notification. */
        esbRawPacketRecvd = new whad::esb::RawPacketReceived(esbPacket->getChannel(), packet);

        /* Set RSSI, CRC validity, timestamp and address. */
        esbRawPacketRecvd->setRssi(esbPacket->getRssi());
        esbRawPacketRecvd->setCrcValidity(esbPacket->isCrcValid());
        esbRawPacketRecvd->setTimestamp(esbPacket->getTimestamp());
        esbRawPacketRecvd->setAddress(address);

        /* Notification. */
        message = dynamic_cast<whad::NanoPbMsg*>(esbRawPacketRecvd);
    }
  }
  else if (packet->getPacketType() == GENERIC_PACKET_TYPE) {
    GenericPacket* genPacket = static_cast<GenericPacket*>(packet);
    
    /* Create PHY packet. */
    whad::phy::Packet phyPacket(genPacket->getPacketBuffer(), genPacket->getPacketSize());

    /* Create a PHY timestamp from packet timestamp. */
    whad::phy::Timestamp pktTimestamp(packet->getTimestamp()/1000, (packet->getTimestamp()%1000)*1000);

    /* Created a PHY packet notification. */
    message = new whad::phy::PacketReceived(
        2400 + packet->getChannel(),
        packet->getRssi(),
        pktTimestamp,
        phyPacket
    );
  }

  return message;
}
