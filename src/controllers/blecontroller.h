#ifndef BLECONTROLLER_H
#define BLECONTROLLER_H
#include "../packet.h"
#include "../controller.h"
#include "../timer.h"
#include "whad/whad.h"

#define ADV_REPORT_SIZE 25

#define DIRECTION_UNKNOWN 					0
#define DIRECTION_MASTER_TO_SLAVE 	1
#define DIRECTION_SLAVE_TO_MASTER 	2

#define BLE_1MBPS_PREAMBLE_SIZE 1
#define ACCESS_ADDRESS_SIZE 		4
#define CRC_SIZE								3
#define BLE_IFS 								150

#define MAX_AA_CANDIDATES 25

typedef enum ConnectionStatus {
	CONNECTION_STARTED = 0x00,
	CONNECTION_LOST = 0x01,
	ATTACK_STARTED = 0x02,
	ATTACK_SUCCESS = 0x03,
	ATTACK_FAILURE = 0x04
} ConnectionStatus;

typedef enum BLEControllerState {
	SNIFFING_ADVERTISEMENTS,
	COLLECTING_ADVINTERVAL,
	FOLLOWING_ADVERTISEMENTS,
	SNIFFING_ACCESS_ADDRESS,
	RECOVERING_CRC_INIT,
	RECOVERING_CHANNEL_MAP,
	RECOVERING_HOP_INTERVAL,
	RECOVERING_HOP_INCREMENT,
	ATTACH_TO_EXISTING_CONNECTION,
	SNIFFING_CONNECTION,
	INJECTING_TO_SLAVE,
	INJECTING_TO_MASTER,
	//INJECTING,
	//INJECTING_FROM_SLAVE,
	SIMULATING_SLAVE,
	SYNCHRONIZING_MASTER,
	SIMULATING_MASTER,
	SYNCHRONIZING_MITM,
	PERFORMING_MITM,
	JAMMING_CONNECT_REQ,
	REACTIVE_JAMMING
} BLEControllerState;

// Connection update definitions
typedef enum {
	UPDATE_TYPE_NONE,
	UPDATE_TYPE_CONNECTION_UPDATE_REQUEST,
	UPDATE_TYPE_CHANNEL_MAP_REQUEST
} BLEConnectionUpdateType;

typedef struct BLEConnectionUpdate {
	BLEConnectionUpdateType type;
	uint16_t instant;
	uint16_t hopInterval;
	uint8_t windowSize;
	uint8_t windowOffset;
	uint8_t channelMap[5];
} BLEConnectionUpdate;


// Sequence numbers definition
typedef struct BLESequenceNumbers {
	uint8_t sn;
	uint8_t nesn;
} BLESequenceNumbers;

// Attack specific definitions
typedef enum BLEAttack {
	BLE_ATTACK_NONE,
	BLE_ATTACK_FRAME_INJECTION_TO_SLAVE,
	BLE_ATTACK_FRAME_INJECTION_TO_MASTER,
	BLE_ATTACK_MASTER_HIJACKING,
	BLE_ATTACK_SLAVE_HIJACKING,
	BLE_ATTACK_MITM
} BLEAttack;

typedef struct BLEAttackStatus {
	BLEAttack attack;
	bool running;
	bool injecting;
	bool successful;
	uint8_t payload[64];
	size_t size;
	uint32_t injectionTimestamp;
	uint32_t injectionCounter;
	uint32_t hijackingCounter;
	uint16_t nextInstant;
	BLESequenceNumbers injectedSequenceNumbers;
} BLEAttackStatus;

typedef struct BLEPayload {
	uint8_t payload[64];
	size_t size;
	bool responseReceived;
	uint16_t lastTransmitInstant;
	bool transmitted;
} BLEPayload;

typedef struct CandidateAccessAddresses {
	uint32_t candidates[MAX_AA_CANDIDATES];
	uint8_t pointer;
} CandidateAccessAddresses;

typedef enum ChannelState {
	NOT_MAPPED = 0,
	MAPPED = 1,
	NOT_ANALYZED = 2,
	NOT_MONITORED = 3
} ChannelState;

typedef struct ActiveConnectionRecovery {
	bool monitoredChannels[37];
	int monitoredChannelsCount;

	uint8_t accessAddressPreamble;
	CandidateAccessAddresses accessAddressCandidates;

	int validPacketOccurences;

	ChannelState mappedChannels[37];
	uint32_t lastTimestamp;

	uint8_t* hoppingSequences[12];
	int lookUpTables[12];
	int reverseLookUpTables[37];
	int numberOfMeasures;

	uint8_t firstChannel;
	uint8_t secondChannel;
} ActiveConnectionRecovery;

typedef struct ReactiveJammingPattern {
	uint8_t pattern[20];
	size_t size;
	int position;
} ReactiveJammingPattern;

class BLEController : public Controller {
	protected:
		TimerModule *timerModule;

		Timer* connectionTimer;
		Timer* injectionTimer;
		Timer* masterTimer;
		Timer* discoveryTimer;

		uint32_t lastAnchorPoint;
		bool emptyTransmitIndicator;
		bool advertisementsTransmitIndicator;
		bool follow; // follow mode indicator

		// Channels related attributes
		int channel;
		int lastUnmappedChannel;
		int unmappedChannel;
		int numUsedChannels;
		int lastAdvertisingChannel;

		// General parameters
		uint32_t accessAddress;
		uint32_t crcInit;

		// Connection specific parameters
		uint16_t connectionEventCount;
		uint8_t hopIncrement;
		uint16_t hopInterval;
		bool channelsInUse[37];
		int *remappingTable;
		BLEConnectionUpdate connectionUpdate;
		bool sync;
		uint8_t desyncCounter;
		uint16_t latency;
		uint16_t latencyCounter;
		int packetCount;
		int lastPacketCount;
		int masterSCA;
		int slaveSCA;

		uint8_t channelMap[5];

		uint32_t lastMasterTimestamp;
		uint32_t lastSlaveTimestamp;

		ReactiveJammingPattern reactiveJammingPattern;
		// Sequence numbers
		BLESequenceNumbers masterSequenceNumbers;
		BLESequenceNumbers slaveSequenceNumbers;

		BLESequenceNumbers simulatedMasterSequenceNumbers;
		BLESequenceNumbers simulatedSlaveSequenceNumbers;

		// Attack related
		BLEAttackStatus attackStatus;

		BLEControllerState controllerState;

		BLEPayload slavePayload;
		BLEPayload masterPayload;

		// Advertising interval related
		uint32_t timestampsFirstChannel[ADV_REPORT_SIZE];
		uint32_t timestampsThirdChannel[ADV_REPORT_SIZE];
		int collectedIntervals;

		BLEAddress filter;

		// Discovery related data
		ActiveConnectionRecovery activeConnectionRecovery;
	public:
		static int channelToFrequency(int channel);

		BLEController(Radio *radio);
		void start();
		void stop();

		void sniff();

		void setAnchorPoint(uint32_t timestamp);

		bool whitelistAdvAddress(bool enable, uint8_t a, uint8_t b, uint8_t c,uint8_t d, uint8_t e, uint8_t f);
		bool whitelistInitAddress(bool enable, uint8_t a, uint8_t b, uint8_t c,uint8_t d, uint8_t e, uint8_t f);
		bool whitelistConnection(bool enable,uint8_t a, uint8_t b, uint8_t c,uint8_t d, uint8_t e, uint8_t f,uint8_t ap, uint8_t bp, uint8_t cp,uint8_t dp, uint8_t ep, uint8_t fp);

		void sendInjectionReport(bool status, uint32_t injectionCount);
		void sendAdvIntervalReport(uint32_t interval);
		void sendConnectionReport(ConnectionStatus status);
		void sendAccessAddressReport(uint32_t accessAddress, uint32_t timestamp, int32_t rssi);
		void sendExistingConnectionReport(uint32_t accessAddress, uint32_t crcInit, uint8_t *channelMap, uint16_t hopInterval, uint8_t hopIncrement);

		void setMonitoredChannels(uint8_t *channels);

		void resetAccessAddressesCandidates();
		bool isAccessAddressKnown(uint32_t accessAddress);
		void addCandidateAccessAddress(uint32_t accessAddress);

		void followAdvertisingDevice(uint8_t a,uint8_t b,uint8_t c,uint8_t d,uint8_t e,uint8_t f);
		void calculateAdvertisingIntervals();

		// Hardware configuration manager
		void setHardwareConfiguration(uint32_t accessAddress,uint32_t crcInit);
		void setJammerConfiguration();
		void setAccessAddressDiscoveryConfiguration(uint8_t preamble);
		void setCrcRecoveryConfiguration(uint32_t accessAddress);
		void setReactiveJammerConfiguration(uint8_t *pattern, size_t size, int position);

		// Follow mode setter
		void setFollowMode(bool follow);

		// Existing connections discovery
		void sniffAccessAddresses();
		void hopToNextDataChannel();
		void recoverCrcInit(uint32_t accessAddress);
		void recoverChannelMap(uint32_t accessAddress, uint32_t crcInit);
		void recoverHopInterval(uint32_t accessAddress, uint32_t crcInit, uint8_t *channelMap);
		void recoverHopIncrement(uint32_t accessAddress, uint32_t crcInit, uint8_t *channelMap, uint16_t interval);
		void attachToExistingConnection(uint32_t accessAddress, uint32_t crcInit, uint8_t *channelMap, uint16_t interval, uint8_t hopIncrement);

		// Connection specific methods
		void followConnection(uint16_t hopInterval, uint8_t hopIncrement, uint8_t *channelMap,uint32_t accessAddress,uint32_t crcInit,  int masterSCA,uint16_t latency);

		int getChannel();
		void setChannel(int channel);
		int nextChannel();

		void updateHopInterval(uint16_t hopInterval);
		void updateHopIncrement(uint8_t hopIncrement);
		void updateChannelsInUse(uint8_t* channelMap);

  	int findChannelIndexInHoppingSequence(uint8_t hopIncrement, uint8_t channel, uint8_t start);
		int computeDistanceBetweenChannels(uint8_t hopIncrement, uint8_t firstChannel, uint8_t secondChannel);
		void generateLegacyHoppingLookUpTables(uint8_t firstChannel, uint8_t secondChannel);
		void findUniqueChannels(uint8_t *firstChannel, uint8_t* secondChannel);

		void generateLegacyHoppingSequence(uint8_t hopIncrement, uint8_t *sequence);
		void generateAllHoppingSequences();

		void clearConnectionUpdate();
		void prepareConnectionUpdate(uint16_t instant, uint16_t hopInterval, uint8_t windowSize, uint8_t windowOffset,uint16_t latency);
		void prepareConnectionUpdate(uint16_t instant, uint8_t *channelMap);
		void applyConnectionUpdate();

		void updateMasterSequenceNumbers(uint8_t sn, uint8_t nesn);
		void updateSlaveSequenceNumbers(uint8_t sn, uint8_t nesn);

		void updateSimulatedMasterSequenceNumbers(uint8_t sn, uint8_t nesn);
		void updateSimulatedSlaveSequenceNumbers(uint8_t sn, uint8_t nesn);

		void updateInjectedSequenceNumbers(uint8_t sn, uint8_t nesn);

		void setAdvertisementsTransmitIndicator(bool advertisementsTransmitIndicator);
		void setEmptyTransmitIndicator(bool emptyTransmitIndicator);
		void setFilter(uint8_t a,uint8_t b,uint8_t c,uint8_t d,uint8_t e,uint8_t f);

		// Attack related methods
		void setAttackPayload(uint8_t *payload, size_t size);
		void startAttack(BLEAttack attack);
		void prepareInjectionToMaster();
		void prepareInjectionToSlave();
		void prepareSlaveHijacking();
		void prepareMasterRelatedHijacking();


		void executeAttack();
		void executeInjectionToSlave();
		void executeInjectionToMaster();
		void executeMasterRelatedHijacking();


		void checkAttackSuccess();

		void setSlavePayload(uint8_t *payload, size_t size);
		void setMasterPayload(uint8_t *payload, size_t size);

		void setConnectionJammingConfiguration(uint32_t accessAddress,uint32_t channel);

		bool isSlavePayloadTransmitted();
		bool isMasterPayloadTransmitted();

		// Slave simulation
		void enterSlaveInjectionMode(int ifs);
		void enterSlaveMode();
		void exitSlaveMode();

		// Role simulation callback
		bool slaveRoleCallback(BLEPacket *pkt);
		bool masterRoleCallback(BLEPacket *pkt);

		BLEControllerState getState();

		// Timers callbacks
		bool goToNextChannel();
		bool inject();
		bool connectionLost();

		void releaseTimers();

		// Packets processing methods
		void accessAddressProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void crcInitRecoveryProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void channelMapRecoveryProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void hopIntervalRecoveryProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void hopIncrementRecoveryProcessing(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);

		void advertisementPacketProcessing(BLEPacket *pkt);
		void connectionPacketProcessing(BLEPacket *pkt);

		void advertisementSniffingProcessing(BLEPacket *pkt);
		void advertisingIntervalEstimationProcessing(BLEPacket *pkt);

		void connectionManagementProcessing(BLEPacket *pkt);
		void connectionSynchronizationProcessing(BLEPacket *pkt);

		void masterPacketProcessing(BLEPacket *pkt);
		void slavePacketProcessing(BLEPacket *pkt);

		void attackSynchronizationProcessing(BLEPacket *pkt);
		void roleSimulationProcessing(BLEPacket* pkt);
		void masterSimulationControlFlowProcessing(BLEPacket *pkt);

		// Reception callback
		void onReceive(uint32_t timestamp, uint8_t size, uint8_t *buffer, CrcValue crcValue, uint8_t rssi);
		void onMatch(uint8_t *buffer, size_t size);
		void onJam(uint32_t timestamp);
		void onEnergyDetection(uint32_t timestamp, uint8_t value);
};
#endif
