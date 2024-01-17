#ifndef CORE_H
#define CORE_H

#include <stdlib.h>
#include "whad/whad.h"
#include <whad.h>

#include "version.h"
#include "led.h"
#include "serial.h"
#include "timer.h"
#include "radio.h"
#include "sequences/sequenceModule.h"

#include "messageQueue.h"

#include "controller.h"
#include "controllers/blecontroller.h"
#include "controllers/dot15d4controller.h"
#include "controllers/esbcontroller.h"
#include "controllers/antcontroller.h"
#include "controllers/mosartcontroller.h"
#include "controllers/genericcontroller.h"

class SerialComm;


extern "C" void core_send_bytes(uint8_t *p_bytes, int size);

class Core {
	private:
        whad_transport_cfg_t transportConfig;
		LedModule *ledModule;
		SerialComm *serialModule;
		TimerModule *timerModule;
		SequenceModule *sequenceModule;
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
		SequenceModule *getSequenceModule();
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
		void processGenericInputMessage(whad::NanoPbMsg msg);
		void processDiscoveryInputMessage(whad::discovery::DiscoveryMsg msg);
		void processDot15d4InputMessage(dot15d4_Message msg);
		void processBLEInputMessage(ble_Message msg);
		void processESBInputMessage(esb_Message msg);
		void processUnifyingInputMessage(unifying_Message msg);
		void processPhyInputMessage(phy_Message msg);

		bool selectController(Protocol controller);

		void pushMessageToQueue(Message *msg);
		Message* popMessageFromQueue();

		bool sendMessage(Message *msg);

		//void handleCommand(Command *cmd);
		void init();
		void loop();

};
#endif
