#include "controller.h"
#include "core.h"
#include "whad.h"

Controller::Controller(Radio *radio) {
	this->radio = radio;
}

void Controller::addPacket(Packet* packet) {
	Core::instance->pushMessageToQueue(buildMessageFromPacket(packet));
}

void Controller::sendDebug(const char* msg) {
    /* Craft a verbose message. */
    whad::generic::Verbose verb(msg);

	//Core::instance->pushMessageToQueue(Whad::buildVerboseMessage(msg));
    Core::instance->pushMessageToQueue(verb.getRaw());
}

Message* Controller::buildMessageFromPacket(Packet* packet) {
  Message *msg = NULL;
  if (packet->getPacketType() == BLE_PACKET_TYPE) {
    BLEPacket *blePacket = static_cast<BLEPacket*>(packet);
    //msg = Whad::buildBLERawPduMessage((BLEPacket*)packet);
    msg = whad::ble::RawPdu(
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
    ).getRaw();
  }
  else if (packet->getPacketType() == DOT15D4_PACKET_TYPE) {
    Dot15d4Packet *zigbeePacket = static_cast<Dot15d4Packet*>(packet);
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
    //msg = Whad::buildDot15d4RawPduMessage((Dot15d4Packet*)packet);
    msg = whad::zigbee::RawPduReceived(packet).getRaw();
  }
  else if (packet->getPacketType() == ESB_PACKET_TYPE) {
    ESBPacket *esbPacket = static_cast<ESBPacket*>(packet);
    
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
        whad::unifying::RawPduReceived packetRecvd(uniPacket);
        packetRecvd.addAddress(address);

        /* Send our RawPacketReceived notification. */
        msg = packetRecvd.getRaw();
    }
    else
    {
        /* Create an ESB packet from ESBPacket. */
        whad::esb::Packet packet(esbPacket->getPacketBuffer(), esbPacket->getPacketSize());

        /* Extract ESB address. */
        whad::esb::EsbAddress address(esbPacket->getAddress(), 5);

        /* Create a RawPacketReceived notification. */
        whad::esb::RawPacketReceived packetRecvd(esbPacket->getChannel(), packet);

        /* Set RSSI, CRC validity, timestamp and address. */
        packetRecvd.setRssi(esbPacket->getRssi());
        packetRecvd.setCrcValidity(esbPacket->isCrcValid());
        packetRecvd.setTimestamp(esbPacket->getTimestamp());
        packetRecvd.setAddress(address);

        /* Notification. */
        msg = packetRecvd.getRaw();
    }
  }
  else if (packet->getPacketType() == GENERIC_PACKET_TYPE) {
    //msg = Whad::buildPhyPacketMessage((GenericPacket*)packet);
    GenericPacket* genPacket = static_cast<GenericPacket*>(packet);
    
    /* Create PHY packet. */
    whad::phy::Packet phyPacket(genPacket->getPacketBuffer(), genPacket->getPacketSize());

    /* Create a PHY timestamp from packet timestamp. */
    whad::phy::Timestamp pktTimestamp(packet->getTimestamp()/1000, (packet->getTimestamp()%1000)*1000);

    /* Created a PHY packet notification. */
    whad::phy::PacketReceived packetRecvd (
        2400 + packet->getChannel(),
        packet->getRssi(),
        pktTimestamp,
        phyPacket
    );

    /* Send packet to host. */
    msg = packetRecvd.getRaw();
  }
  return msg;
}
