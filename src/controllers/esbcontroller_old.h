#ifndef ESBCONTROLLER_H
#define ESBCONTROLLER_H

#include "../packet.h"
#include "../controller.h"
#include "bsp.h"
#include "../timer.h"

// Attack specific definitions
typedef enum ESBAttack {
	ESB_ATTACK_NONE,
	ESB_ATTACK_SCANNING,
	ESB_ATTACK_SNIFF_LOGITECH_PAIRING,
	ESB_ATTACK_JAMMING
} ESBAttack;

typedef struct ESBAttackStatus {
	ESBAttack attack;
	bool running;
	int index;
	uint32_t timestamps[24];
	uint32_t channels[24];
	bool successful;
} ESBAttackStatus;

typedef enum ESBMode {
	ESB_PROMISCUOUS,
	ESB_FOLLOW,
	ESB_JAM
} ESBMode;

typedef struct ESBRetransmitBuffer {
	uint8_t buffer[255];
	size_t size;
	uint32_t timestamp;
	bool acked;
} ESBRetransmitBuffer;

typedef struct ESBAcknowledgementBuffer {
	uint8_t buffer[255];
	size_t size;
	bool available;
} ESBAcknowledgementBuffer;

class ESBController : public Controller {
  protected:

		TimerModule *timerModule;
		Timer *scanTimer;
		Timer *timeoutTimer;
		Timer *pairingTimer;

		int channel;

		bool lastTransmissionAcknowledged;
		uint32_t lastTransmissionTimestamp;

		ESBAcknowledgementBuffer preparedAck;
		ESBRetransmitBuffer lastReceivedPacket;
    ESBAttackStatus attackStatus;
		ESBAddress filter;
		ESBMode mode;

		bool follow;
		bool autofind;
		bool syncing;
		bool activeScanning;

		bool sendAcknowledgements;
		bool showAcknowledgements;

		bool unifying;

		bool stopTransmitting;
	public:
		ESBController(Radio* radio);
    void start();
    void stop();

		void enableUnifying();
		void disableUnifying();
		bool isUnifyingEnabled();

		void sendAck(uint8_t pid);
		void enableAcknowledgementsSniffing();
		void disableAcknowledgementsSniffing();

		void enableAcknowledgementsTransmission();
		void disableAcknowledgementsTransmission();

		void pairing();
		void startPairingSniffing();
		void stopPairingSniffing();

		void sendPing();
		void setFilter(uint8_t a,uint8_t b,uint8_t c,uint8_t d,uint8_t e);
	  void setFollowMode(bool follow);

		void checkPresence();
		void nextPairingChannel();
		bool timeout();
    bool goToNextChannel();

		void setAutofind(bool autofind);

		void startTimeout();
    void stopTimeout();
		void expandTimeout(int timestamp);

		void startScanning();
    void stopScanning();

		void startAttack(ESBAttack attack);

		int getChannel();
		void setChannel(int channel);

		void setPromiscuousConfiguration();
		void setFollowConfiguration(uint8_t address[5]);
		void setJammerConfiguration();


		void sendJammingReport(uint32_t timestamp);

		ESBPacket* buildPseudoPacketFromPayload(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		bool send(uint8_t *data, size_t size, int retransmission_count);

    // Reception callback
    void onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void onMatch(uint8_t *buffer, size_t size);

		void onPromiscuousPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void onFollowPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void onPRXPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void onPTXPacketProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);


		// Jamming callback
		void onJam(uint32_t timestamp);
		void onEnergyDetection(uint32_t timestamp, uint8_t value);
};

#endif
