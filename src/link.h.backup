#ifndef LINK_H
#define LINK_H
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "nrf.h"

#define START_SLAVE_RADIO 0
#define STOP_SLAVE_RADIO 1

typedef enum LinkMode {
	LINK_SLAVE = 0,
	LINK_MASTER = 1,
	RESET_LINK = 0xFF
} LinkMode;

class LinkModule {
	protected:
		uint8_t controlPin;
		LinkMode mode;

	public:
    LinkModule();
		void configureLink(LinkMode mode, uint8_t pin);

		void resetLink();

    void setupSlave();
    void setupMaster();

		void sendSignalToSlave(int signal);

    void initInput(int pin);
    void initOutput(int pin);
    void initPPI(int pin);

};
#endif
