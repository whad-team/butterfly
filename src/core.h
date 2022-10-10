#ifndef CORE_H
#define CORE_H

#include <stdlib.h>

#include "whad/nanopb/pb.h"
#include "whad/protocol/whad.pb.h"
#include "whad/whad.h"

#include "version.h"
#include "led.h"
#include "serial.h"
#include "timer.h"
#include "link.h"
#include "radio.h"

#include "messageQueue.h"

#include "controller.h"
#include "controllers/blecontroller.h"
#include "controllers/dot15d4controller.h"
#include "controllers/esbcontroller.h"
#include "controllers/antcontroller.h"
#include "controllers/mosartcontroller.h"
#include "controllers/genericcontroller.h"

class SerialComm;

class Core {
	private:
		LedModule *ledModule;
		SerialComm *serialModule;
		TimerModule *timerModule;
		//LinkModule *linkModule;
		Radio *radio;

		MessageQueue messageQueue;

		BLEController *bleController;
		Dot15d4Controller *dot15d4Controller;
		ESBController *esbController;
		ANTController *antController;
		MosartController *mosartController;

		GenericController *genericController;
		Controller* currentController;

	public:
		static Core *instance;

		Core();
		LedModule *getLedModule();
		SerialComm *getSerialModule();
		//LinkModule *getLinkModule();
		TimerModule *getTimerModule();
		Radio *getRadioModule();

		void setControllerChannel(int channel);

		#ifdef PA_ENABLED
			void configurePowerAmplifier(bool enabled);
		#endif

		void sendVerbose(const char* data);


		void sendDebug(const char* message);
		void sendDebug(uint8_t *buffer, uint8_t size);

		void processInputMessage(Message msg);
		void processGenericInputMessage(generic_Message msg);
		void processDiscoveryInputMessage(discovery_Message msg);
		void processZigbeeInputMessage(zigbee_Message msg);
		void processBLEInputMessage(ble_Message msg);
		void processESBInputMessage(esb_Message msg);
		void processUnifyingInputMessage(unifying_Message msg);

		bool selectController(Protocol controller);

		void pushMessageToQueue(Message *msg);
		Message* popMessageFromQueue();

		void handleInputData(uint8_t *buffer, size_t size);
		void sendMessage(Message *msg);

		//void handleCommand(Command *cmd);
		void init();
		void loop();

};
#endif
