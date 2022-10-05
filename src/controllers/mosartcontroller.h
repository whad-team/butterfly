#ifndef MOSARTCONTROLLER_H
#define MOSARTCONTROLLER_H
#include "../packet.h"
#include "../controller.h"
#include "bsp.h"

// Attack specific definitions
typedef enum MosartAttack {
	MOSART_ATTACK_NONE,
	MOSART_ATTACK_JAMMING
} MosartAttack;

typedef struct MosartAttackStatus {
	MosartAttack attack;
	bool running;
	bool successful;
} MosartAttackStatus;

class MosartController : public Controller {
  protected:
		int channel;
    MosartAttackStatus attackStatus;

		MosartAddress filter;
		bool donglePackets;

	public:
		MosartController(Radio* radio);
    void start();
    void stop();

		void startAttack(MosartAttack attack);

		void setFilter(uint8_t a,uint8_t b,uint8_t c,uint8_t d);
		void enableDonglePackets();
		void disableDonglePackets();

		bool checkAddress(MosartPacket *pkt);

		void sendJammingReport(uint32_t timestamp);


		int getChannel();
		void setChannel(int channel);

		void setHardwareConfiguration();
		MosartPacket *buildMosartPacket(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void setJammerConfiguration();

    void send(uint8_t* data, size_t size);

    // Reception callback
    void onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void onJam(uint32_t timestamp);
		void onMatch(uint8_t *buffer, size_t size);

		void onEnergyDetection(uint32_t timestamp, uint8_t value);
};

#endif
