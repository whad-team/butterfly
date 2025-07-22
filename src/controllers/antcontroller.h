#ifndef ANTCONTROLLER_H
#define ANTCONTROLLER_H
#include <queue>
#include "../packet.h"
#include "../controller.h"
#include "../timer.h"
#include "bsp.h"

#define PREAMBLE_ANT_PLUS   0xc5a6
#define PREAMBLE_ANT_FS     0xa33b

#define NETWORK_KEY_SIZE    8
#define TIMESTAMP_REPORTS_NB 10

#define MAX_CHANNELS 4
#define MAX_NETWORKS 4

#define UNASSIGNED 0xFF
#define AS_SOON_AS_POSSIBLE 0

#define SCHEDULING_TICK_US  200

typedef struct TXPacket {
    uint8_t packet[17];
} TXPacket;

typedef enum ANTChannelType {
    BIDIRECTIONAL_TRANSMIT_CHANNEL = 0, 
    BIDIRECTIONAL_RECEIVE_CHANNEL = 1, 

    SHARED_BIDIRECTIONAL_TRANSMIT_CHANNEL = 2, 
    SHARED_BIDIRECTIONAL_RECEIVE_CHANNEL = 3, 

    TRANSMIT_ONLY_CHANNEL = 4, 
    RECEIVE_ONLY_CHANNEL = 5, 

    UNKNOWN_TYPE = 0xFF
} ANTChannelType;

typedef enum ANTMode {
    MASTER = 0, 
    SLAVE = 1, 
    SNIFFER = 2, 
    UNKNOWN_MODE = 0xFF
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

    uint32_t channelPeriod;
    Timer *masterTimer;
    bool synced;
    uint32_t lastSync;

    uint32_t packetCountSinceSync;
    TXPacket latestBroadcast;
    TXPacket latestAck;

	bool incomingBurst;
	bool outgoingBurst;

	std::queue<TXPacket> transmitQueue;
	std::queue<TXPacket> burstQueue;

	ANTChannelType type;
    ANTMode mode;
} ANTChannel;

class ANTController : public Controller {
  protected:
		TimerModule *timerModule;

		int rfChannel;
        
        ANTChannel channels[MAX_CHANNELS];
        ANTNetwork networks[MAX_NETWORKS];

        uint8_t networkKey[NETWORK_KEY_SIZE];

        uint8_t activeChannel;
		Timer *burstTimer;
	public:
		ANTController(Radio* radio);
		void start();
		void stop();

        void releaseTimers();
        void releaseTimer(uint32_t channelIndex);

        bool channelManagementCallback(uint8_t channelIndex);

        bool channel0Callback();
        bool channel1Callback();
        bool channel2Callback();
        bool channel3Callback();

        bool startChannelTimer(uint8_t channelIndex);
        bool startChannelTimer(uint8_t channelIndex, uint32_t timestamp);

		bool burstTimerCallback();
		void startBurstTimer();
        void startSlotBurstTimer();
		void releaseBurstTimer();

        bool addPacketToTransmitQueue(uint8_t channelIndex, uint8_t *packet);
        bool availablePacketsToTransmit(uint8_t channelIndex);
        TXPacket getPacketFromTransmitQueue(uint8_t channelIndex);

		bool addPacketToBurstQueue(uint8_t channelIndex, uint8_t *packet);
        bool isBurstReady(uint8_t channelIndex);
        TXPacket getPacketFromBurstQueue(uint8_t channelIndex);

        int getRFChannel(uint8_t channelIndex);
        bool setRFChannel(uint8_t channelIndex, int rfChannel);
        uint16_t getDeviceNumber(uint8_t channelIndex);
        bool setDeviceNumber(uint8_t channelIndex, uint16_t deviceNumber);
        uint8_t getDeviceType(uint8_t channelIndex);
        bool setDeviceType(uint8_t channelIndex, uint8_t deviceType);
        uint8_t getTransmissionType(uint8_t channelIndex);
        bool setTransmissionType(uint8_t channelIndex, uint8_t transmissionType);
        ANTMode getMode(uint8_t channelIndex);
        bool setMode(uint8_t channelIndex, ANTMode mode);
        ANTChannelType getChannelType(uint8_t channelIndex);
        bool setChannelType(uint8_t channelIndex, ANTChannelType type);
        bool assignNetwork(uint8_t channelIndex, uint8_t networkIndex);
        bool setLastSync(uint8_t channelIndex, uint32_t lastSync);
        uint32_t getLastSync(uint8_t channelIndex);
        bool setChannelPeriod(uint8_t channelIndex, uint32_t channelPeriod);
        uint32_t getChannelPeriod(uint8_t channelIndex);

        bool unassignNetwork(uint8_t channelIndex);
        bool openChannel(uint8_t channelIndex);
        bool closeChannel(uint8_t channelIndex);
        bool isChannelOpen(uint8_t channelIndex);

        bool setActiveChannel(uint8_t channelIndex);
        uint16_t getActivePreamble();
		int getActiveRFChannel();
        uint16_t getActiveDeviceNumber();
        uint8_t getActiveDeviceType();
        uint8_t getActiveTransmissionType();

        void sendChannelEvent(uint8_t channelIndex, whad::ant::ChannelEventCode event);
		void setActiveRFChannel(int rfChannel);

        bool setNetworkKey(uint8_t networkIndex, uint8_t *networkKey);

		void setHardwareConfiguration();

        bool checkFilter(ANTPacket* packet);

        // Reception callback
    	void onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void onJam(uint32_t timestamp);
		void onMatch(uint8_t *buffer, size_t size);

		void onEnergyDetection(uint32_t timestamp, uint8_t value);
};

#endif
