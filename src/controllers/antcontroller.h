#ifndef ANTCONTROLLER_H
#define ANTCONTROLLER_H
#include "../packet.h"
#include "../controller.h"
#include "../timer.h"
#include "bsp.h"

#define PREAMBLE_ANT_PLUS   0xc5a6
#define PREAMBLE_ANT_FS     0xa33b

#define NETWORK_KEY_SIZE    8
#define TIMESTAMP_REPORTS_NB 10

#define MAX_CHANNELS 8
#define MAX_NETWORKS 4

typedef enum ANTChannelType {
    BIDIRECTIONAL_TRANSMIT_CHANNEL = 0, 
    BIDIRECTIONAL_RECEIVE_CHANNEL = 1, 

    SHARED_BIDIRECTIONAL_TRANSMIT_CHANNEL = 2, 
    SHARED_BIDIRECTIONAL_RECEIVE_CHANNEL = 3, 

    TRANSMIT_ONLY_CHANNEL = 4, 
    RECEIVE_ONLY_CHANNEL = 5, 
} ANTChannelType;

typedef enum ANTMode {
    MASTER = 0, 
    SLAVE = 1
} ANTMode;

typedef struct ANTNetwork {
    uint8_t networkKey[NETWORK_KEY_SIZE];
    uint16_t preamble;
} ANTNetwork;

typedef struct ANTChannel {
    bool enabled;

    int rfChannel;

    uint16_t deviceNumber;
    uint8_t deviceType;
    uint8_t transmissionType;
    uint8_t networkIndex;

    ANTChannelType type;
    ANTMode mode;
} ANTChannel;

class ANTController : public Controller {
  protected:
		TimerModule *timerModule;

		int rfChannel;
        
        ANTChannel channels[MAX_CHANNELS];
        ANTNetwork networks[MAX_NETWORKS];

		uint16_t deviceNumber;
		uint8_t deviceType;
		uint8_t transmissionType;
        
        uint8_t networkKey[NETWORK_KEY_SIZE];
		uint16_t preamble;
        uint8_t selectedNetwork;

	public:
		ANTController(Radio* radio);
		void start();
		void stop();

		int getRFChannel();
		void setRFChannel(int rfChannel);

        uint16_t getDeviceNumber();
        void setDeviceNumber(uint16_t deviceNumber);
        uint8_t getDeviceType();
        void setDeviceType(uint8_t deviceType);
        uint8_t getTransmissionType();
        void setTransmissionType(uint8_t transmissionType);
        bool setNetworkKey(uint8_t networkIndex, uint8_t *networkKey);
        bool useNetwork(uint8_t networkIndex);

		void setHardwareConfiguration();

    	// Reception callback
    	void onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void onJam(uint32_t timestamp);
		void onMatch(uint8_t *buffer, size_t size);

		void onEnergyDetection(uint32_t timestamp, uint8_t value);
};

#endif
