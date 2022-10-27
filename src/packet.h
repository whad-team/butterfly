#ifndef PACKET_H
#define PACKET_H
#include "helpers.h"
#include "radio_defs.h"
#include "stdint.h"
#include "stddef.h"
#include "string.h"

typedef enum PacketType {
	BLE_PACKET_TYPE = 0x00,
	DOT15D4_PACKET_TYPE = 0x01,
	ESB_PACKET_TYPE = 0x02,
	ANT_PACKET_TYPE = 0x03,
	MOSART_PACKET_TYPE = 0x04,
	GENERIC_PACKET_TYPE = 0xFF
} PacketType;

class Packet {
	protected:
    uint8_t *payload;
		uint8_t *packetPointer;
		size_t packetSize;
		PacketType packetType;
		uint32_t timestamp;
		int8_t rssi;
		uint8_t channel;
		uint8_t source;
		CrcValue crcValue;

	public:
		Packet(PacketType packetType,uint8_t *packetBuffer, size_t packetSize, uint32_t timestamp, uint8_t source, uint8_t channel, int8_t rssi, CrcValue crcValue);
		~Packet();
		uint8_t *getPacketBuffer();
		size_t getPacketSize();
		PacketType getPacketType();
		void updateSource(uint8_t source);
		uint8_t getSource();
		uint8_t getChannel();
		int8_t getRssi();
		uint32_t getTimestamp();
		bool isCrcValid();
};



typedef enum BLEAdvertisementType {
	ADV_IND = 0,
	ADV_DIRECT_IND = 1,
	ADV_NONCONN_IND = 2,
	SCAN_REQ = 3,
	SCAN_RSP = 4,
	CONNECT_REQ = 5,
	ADV_SCAN_IND = 6,
	ADV_UNKNOWN = 0xFF
} BLEAdvertisementType;

typedef enum Dot15d4AddressMode {
	ADDR_NONE = 0,
	ADDR_RESERVED = 1,
	ADDR_SHORT = 2,
	ADDR_EXTENDED = 3
} Dot15d4AddressMode;

class BLEPacket : public Packet {
	protected:
		uint32_t accessAddress;
		uint32_t timestampRelative;
		int connectionHandle;

	public:
		static bool needResponse(uint8_t *payload, size_t size);
		static void forgeConnectionRequest(uint8_t **payload,size_t *size, uint8_t *initiator, bool initiatorRandom,  uint8_t *responder, bool responderRandom, uint32_t accessAddress,  uint32_t crcInit, uint8_t windowSize, uint16_t windowOffset, uint16_t hopInterval, uint16_t slaveLatency, uint16_t timeout, uint8_t *channelMap, uint8_t sca, uint8_t hopIncrement);
		static void forgeTerminateInd(uint8_t **payload, size_t *size, uint8_t code);
		static void forgeConnectionUpdateRequest(uint8_t **payload,size_t *size, uint8_t winSize, uint16_t winOffset, uint16_t interval, uint16_t latency, uint16_t timeout, uint16_t instant);
		static void forgeChannelMapRequest(uint8_t **payload,size_t *size,uint16_t instant, uint8_t *channelMap);

		BLEPacket(uint32_t accessAddress,uint8_t *packetBuffer, size_t packetSize, uint32_t timestamp, uint32_t timestampRelative, uint8_t source, uint8_t channel,int8_t rssi,CrcValue crcValue);

		int getConnectionHandle();
		void setConnectionHandle(int connectionHandle);
		uint32_t getCrc();
		uint32_t getRelativeTimestamp();

		bool isAdvertisement();
		bool isEncryptionRequest();
		bool isPairingRequest();
		bool isReadRequest();
		bool isLinkLayerConnectionUpdateRequest();
		bool isLinkLayerChannelMapRequest();
		bool isLinkLayerTerminateInd();
		uint32_t extractAccessAddress();
		uint32_t extractCrcInit();
		uint16_t extractHopInterval();
		uint8_t extractHopIncrement();
		uint16_t extractLatency();
		uint8_t* extractChannelMap();
		uint16_t extractInstant();
		int extractSCA();

		bool checkAdvertiserAddress(BLEAddress address);

		uint8_t extractPayloadLength();

		uint8_t extractWindowSize();
		uint16_t extractWindowOffset();

		uint8_t extractNESN();
		uint8_t extractSN();
		uint8_t extractMD();

		BLEAdvertisementType extractAdvertisementType();
		uint32_t getAccessAddress();
};

class Dot15d4Packet : public Packet {
	protected:
		uint8_t lqi;

	public:
		Dot15d4Packet(uint8_t *packetBuffer, size_t packetSize, uint32_t timestamp, uint8_t source, uint8_t channel, int8_t rssi, CrcValue crcValue, uint8_t lqi);
		bool extractAcknowledgmentRequest();
		uint8_t extractSequenceNumber();
		uint8_t getLQI();
		Dot15d4AddressMode extractDestinationAddressMode();
		uint16_t extractShortDestinationAddress();
		uint64_t extractExtendedDestinationAddress();
		uint32_t getFCS();

};


class ESBPacket : public Packet {
	protected:
		bool unifying;

	public:
		static uint16_t updateCrc(uint16_t crc, uint8_t byte, uint8_t bits);
		static uint16_t calculateCrc(uint8_t *data, uint8_t total_size);
		ESBPacket(uint8_t *packetBuffer, size_t packetSize, uint32_t timestamp, uint8_t source, uint8_t channel, int8_t rssi, CrcValue crcValue, bool unifying);
		uint8_t getSize();
		uint16_t getCrc();
		bool checkCrc();
		uint8_t getPID();
		uint8_t* getAddress();
		bool isUnifying();
};

class ANTPacket : public Packet {
	public:
		ANTPacket(uint8_t *packetBuffer, size_t packetSize, uint32_t timestamp, uint8_t source, uint8_t channel, int8_t rssi, CrcValue crcValue, uint16_t preamble);

		uint16_t getDeviceNumber();
		uint8_t getDeviceType();
};

class MosartPacket : public Packet {
	public:
		MosartPacket(uint8_t *packetBuffer, size_t packetSize, uint32_t timestamp, uint8_t source, uint8_t channel, int8_t rssi, CrcValue crcValue);
		uint8_t* getAddress();
};

class GenericPacket : public Packet {
	public:
		GenericPacket(uint8_t *packetBuffer, size_t packetSize, uint32_t timestamp, uint8_t source, uint8_t channel, int8_t rssi,CrcValue crcValue, uint8_t* preamble, size_t preambleSize);
};
#endif
