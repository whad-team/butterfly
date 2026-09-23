#ifndef DOT15D4CONTROLLER_H
#define DOT15D4CONTROLLER_H
#include "../packet.h"
#include "../timer.h"
#include "../controller.h"
#include "../tsch/network.h"
#include "bsp.h"

#define MAX_TX_TASKS 10

# define MAX_DOT15D4_SIZE 128

// Attack specific definitions
typedef enum Dot15d4Attack {
	DOT15D4_ATTACK_NONE,
	DOT15D4_ATTACK_JAMMING,
	DOT15D4_ATTACK_CORRECTION,
} Dot15d4Attack;

typedef enum Dot15d4Source {
	RECEIVER = 0x00,
	CORRECTOR = 0x01
} Dot15d4Source;
typedef struct Dot15d4AttackStatus {
	Dot15d4Attack attack;
	bool running;
	bool correctorMode;
	bool successful;
} Dot15d4AttackStatus;

typedef enum Dot15d4ControllerState {
	RECEIVING,
	ENERGY_DETECTION_SCANNING
} Dot15d4ControllerState;

struct TxTask {
    uint64_t target_asn;
    uint32_t wait_offset;
    uint8_t buffer[MAX_DOT15D4_SIZE];
    size_t buffer_size;
    bool send_asap;
};

const uint16_t TsTxOffset = 2020; // lower bound fot the sending offset

class Dot15d4Controller : public Controller {
  protected:
		bool known_link;
		int channel;
    	Dot15d4AttackStatus attackStatus;
		Dot15d4ControllerState controllerState;
		bool started;
		bool autoAcknowledgement;
		uint16_t shortAddress;
		uint64_t extendedAddress;

		Timer* syncTimer;
		Timer* sendTimer;

		bool tsch;
		tsch::Network network;

		TxTask txQueue[MAX_TX_TASKS];
		int txTaskCount = 0;

		uint8_t txTmpBuffer[MAX_DOT15D4_SIZE];
    	size_t txTmpBufferSize;

	public:
		static int channelToFrequency(int channel);
		Dot15d4Controller(Radio* radio);

		void start();
		void stop();

		void setShortAddress(uint16_t shortAddress);
		void setExtendedAddress(uint64_t extendedAddress);
		void setAutoAcknowledgement(bool enable);

		int getChannel();
		void setChannel(int channel);

		void enterReceptionMode();
		void enterEDScanMode();

		void setWazabeeConfiguration();
		void setNativeConfiguration();
		void setJammerConfiguration();
		void setRawConfiguration();
		void setEnergyDetectionConfiguration();

		void startAttack(Dot15d4Attack attack);
		void sendJammingReport(uint32_t timestamp);

		Dot15d4Packet* wazabeeDecoder(uint8_t *buffer, uint8_t size, uint32_t timestamp, CrcValue crcValue, uint8_t rssi);

	    void send(uint8_t* data, size_t size, bool raw);

		/* Time-slotted Channel Hopping related commands */
		bool isTSCHEnabled();

		void scheduleFrameTx(uint64_t asn, uint8_t* frame, size_t size, uint32_t wait_offset, bool asap);
		void checkAndExecuteTx(uint64_t current_slot);

		void sendNow(); 

		void configureTSCH(bool enable);
		tsch::Network* getNetwork();
		bool handleTSCHSynchronisation(Dot15d4Packet* pkt, uint32_t timestamp);

		bool frequencyHop();
		

    	// Reception callback
    	void onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void onJam(uint32_t timestamp);
		void onMatch(uint8_t *buffer, size_t size);

		void onEnergyDetection(uint32_t timestamp, uint8_t value);
};

#endif
