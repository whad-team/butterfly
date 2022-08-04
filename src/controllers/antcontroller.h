#ifndef ANTCONTROLLER_H
#define ANTCONTROLLER_H
#include "../packet.h"
#include "../controller.h"
#include "../timer.h"
#include "bsp.h"

#define PREAMBLE_ANT_PLUS   0xc5a6
#define PREAMBLE_ANT_FS     0xa33b

#define TIMESTAMP_REPORTS_NB 10
// Attack specific definitions
typedef enum AntAttack {
	ANT_ATTACK_NONE,
	ANT_ATTACK_JAMMING,
	ANT_ATTACK_MASTER_HIJACKING
} AntAttack;

typedef struct AntAttackStatus {
	AntAttack attack;
	bool running;
	bool successful;
	uint32_t lastTimestamps[TIMESTAMP_REPORTS_NB];
	uint32_t currentTimestamp;
  uint8_t payload[64];
  size_t size;
} AntAttackStatus;


class ANTController : public Controller {
  protected:
		TimerModule *timerModule;

		int channel;
    uint16_t preamble;
    AntAttackStatus attackStatus;

		uint16_t deviceNumber;
		uint8_t deviceType;

		bool sendingResponse;

		Timer *masterTimer;
		Timer *slaveTimer;

	public:
		ANTController(Radio* radio);
    void start();
    void stop();

		void releaseTimers();

		void setFilter(uint16_t preamble, uint16_t deviceNumber, uint8_t deviceType);

		uint32_t calculateMasterInterval();
		bool transmitCallback();

		int getChannel();
		void setChannel(int channel);

    void startAttack(AntAttack attack);

    void setAttackPayload(uint8_t *payload, size_t size);
		void sendResponsePacket(uint8_t *payload, size_t size);

    void setJammerConfiguration();
		void setHardwareConfiguration();

    void sendJammingReport(uint32_t timestamp);
    void send(uint8_t* data, size_t size);

		bool checkFilter(ANTPacket* pkt);

    // Reception callback
    void onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void onJam(uint32_t timestamp);
		void onEnergyDetection(uint32_t timestamp, uint8_t value);
};

#endif
