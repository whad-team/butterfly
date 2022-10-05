#ifndef GENERICCONTROLLER_H
#define GENERICCONTROLLER_H
#include "../packet.h"
#include "../controller.h"
#include "bsp.h"

typedef enum GenericAttack {
	GENERIC_ATTACK_NONE,
	GENERIC_ATTACK_JAMMING,
	GENERIC_ATTACK_ENERGY_DETECTION
} GenericAttack;

typedef struct GenericAttackStatus {
	GenericAttack attack;
	bool running;
	bool successful;
	uint16_t samplesCount;
} GenericAttackStatus;

typedef enum GenericPhy {
  GENERIC_PHY_1MBPS_ESB = 0,
  GENERIC_PHY_2MBPS_ESB = 1,
  GENERIC_PHY_1MBPS_BLE = 2,
  GENERIC_PHY_2MBPS_BLE = 3,
} GenericPhy;

typedef enum GenericEndianness {
	GENERIC_ENDIANNESS_BIG = 0,
	GENERIC_ENDIANNESS_LITTLE = 1
} GenericEndianness;

class GenericController : public Controller {
  protected:

    int channel;

    uint8_t preamble[4];
    uint8_t preambleSize;
    uint8_t packetSize;
    GenericPhy phy;
		GenericEndianness endianness;

    GenericAttackStatus attackStatus;

	public:
		GenericController(Radio* radio);
    void start();
    void stop();

    bool configure(uint8_t *preamble, size_t preambleSize, size_t packetSize, GenericPhy phy, GenericEndianness endianness);

		int getChannel();
		void setChannel(int channel);

    void startAttack(GenericAttack attack);

    void setJammerConfiguration();
		void setHardwareConfiguration();
		void setEnergyDetectionConfiguration();

    void sendJammingReport(uint32_t timestamp);
		void sendEnergyDetectionReport(uint32_t timestamp, uint8_t value);

    void send(uint8_t* data, size_t size);

    // Reception callback
    void onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void onJam(uint32_t timestamp);
		void onMatch(uint8_t *buffer, size_t size);

		void onEnergyDetection(uint32_t timestamp, uint8_t value);
};

#endif
