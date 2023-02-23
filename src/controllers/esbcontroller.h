#ifndef ESBCONTROLLER_H
#define ESBCONTROLLER_H

#include "../packet.h"
#include "../controller.h"
#include "bsp.h"
#include "../timer.h"

typedef enum ESBMode {
	ESB_PROMISCUOUS,
	ESB_SNIFF,
	ESB_JAM
} ESBMode;

typedef struct ESBAcknowledgementBuffer {
	uint8_t buffer[255];
	size_t size;
	bool available;
} ESBAcknowledgementBuffer;

typedef struct ESBLastTransmission {
	uint32_t timestamp;
	uint32_t acknowledged;
} ESBLastTransmission;

typedef struct ESBLastPacket {
	uint8_t pid;
	uint32_t crc;
	uint32_t timestamp;
} ESBLastPacket;

class ESBController : public Controller {
  protected:
		uint8_t channel;
		bool showAcknowledgements;
		bool sendAcknowledgements;
		ESBAcknowledgementBuffer preparedAck;
		ESBLastTransmission lastTransmission;
		ESBLastPacket lastPacket;

		ESBMode mode;
		ESBApplicativeLayer applicativeLayer;
		TimerModule *timerModule;

		ESBAddress filter;

	public:
		ESBController(Radio* radio);

		// Start and stop methods
    void start();
    void stop();

		// Channel manipulation methods
		void setChannel(uint8_t channel);
		uint8_t getChannel();

		// Filter configuration
		void setFilter(uint8_t a,uint8_t b,uint8_t c,uint8_t d,uint8_t e);

		// Acknowledgement related methods
		void enableAcknowledgementsSniffing();
  	void disableAcknowledgementsSniffing();

		void enableAcknowledgementsTransmission();
  	void disableAcknowledgementsTransmission();

		// Applicative layer
		ESBApplicativeLayer getApplicativeLayer();
		void setApplicativeLayer(ESBApplicativeLayer applicativeLayer);

		// Helpers
		ESBPacket* buildPseudoPacketFromPayload(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		bool buildPayloadFromPacket(uint8_t *packet, size_t packet_size, uint8_t *buffer, size_t* payload_size);

		// Send methods
		void sendAcknowledgement(uint8_t pid);
		bool send(uint8_t *data, size_t size, int retransmission_count);

		// Radio configuration functions
		void setPromiscuousConfiguration();
		void setSnifferConfiguration(uint8_t address[5]);
		void setJammerConfiguration();

    // Reception callback
    void onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);

		void onPRXPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void onPTXPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void onPromiscuousPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void onSniffPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);


		// Jamming callback
		void onJam(uint32_t timestamp);
		// Pattern matching callback
		void onMatch(uint8_t *buffer, size_t size);
		// Energy detection callback
		void onEnergyDetection(uint32_t timestamp, uint8_t value);
};

#endif
