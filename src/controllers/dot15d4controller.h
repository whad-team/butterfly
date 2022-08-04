#ifndef DOT15D4CONTROLLER_H
#define DOT15D4CONTROLLER_H
#include "../packet.h"
#include "../controller.h"
#include "bsp.h"

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

class Dot15d4Controller : public Controller {
  protected:
		int channel;
    Dot15d4AttackStatus attackStatus;
	public:
		static int channelToFrequency(int channel);
		Dot15d4Controller(Radio* radio);
    void start();
    void stop();

		int getChannel();
		void setChannel(int channel);

		void setWazabeeConfiguration();
		void setNativeConfiguration();
		void setJammerConfiguration();

		void startAttack(Dot15d4Attack attack);
		void sendJammingReport(uint32_t timestamp);

		Dot15d4Packet* wazabeeDecoder(uint8_t *buffer, uint8_t size, uint32_t timestamp, CrcValue crcValue, uint8_t rssi);

    void send(uint8_t* data, size_t size);

    // Reception callback
    void onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void onJam(uint32_t timestamp);
		void onEnergyDetection(uint32_t timestamp, uint8_t value);
};

#endif
