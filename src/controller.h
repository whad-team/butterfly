#ifndef CONTROLLER_H
#define CONTROLLER_H
#include <stdint.h>
#include "radio_defs.h"
#include "radio.h"
#include "packet.h"
#include "whad.h"

class Radio;

class Controller {
	protected:
		Radio *radio;

	public:
		Controller(Radio* radio);
		void addPacket(Packet* packet);
		void sendDebug(const char* msg);
		virtual void start() = 0;
		virtual void stop() = 0;
		virtual void onReceive(uint32_t timestamp,uint8_t size,uint8_t *buffer,CrcValue crcValue,uint8_t rssi) = 0;
		virtual void onJam(uint32_t timestamp) = 0;
		virtual void onMatch(uint8_t *buffer, size_t size) = 0;
		virtual void onEnergyDetection(uint32_t timestamp, uint8_t value) = 0;

        static Message* buildMessageFromPacket(Packet* packet);

};

#endif
