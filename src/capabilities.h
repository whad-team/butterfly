#ifndef CAPABILITIES_H
#define CAPABILITIES_H

#define CMD(X) (1 << (X))

typedef struct {
    discovery_Domain domain;
    discovery_Capability cap;
    uint64_t supported_commands;
} DeviceCapability;

/*
typedef struct {
    discovery_Domain domain;
} DomainCommands;
*/
const DeviceCapability CAPABILITIES[] = {
  {
    discovery_Domain_BtLE,
    (discovery_Capability)(discovery_Capability_Sniff | discovery_Capability_Inject | discovery_Capability_Hijack | discovery_Capability_SimulateRole),
    (
      CMD(ble_BleCommand_SniffAdv) |
      CMD(ble_BleCommand_SniffConnReq) |
      CMD(ble_BleCommand_SniffActiveConn) |
      CMD(ble_BleCommand_Start) |
      CMD(ble_BleCommand_Stop) |
      CMD(ble_BleCommand_SendRawPDU) |
      CMD(ble_BleCommand_SendPDU) |
      CMD(ble_BleCommand_HijackMaster) |
      CMD(ble_BleCommand_HijackSlave) |
      CMD(ble_BleCommand_HijackBoth) |
      CMD(ble_BleCommand_CentralMode) |
      CMD(ble_BleCommand_PeripheralMode) |
      CMD(ble_BleCommand_SniffAccessAddress) |
      CMD(ble_BleCommand_ReactiveJam)



      )
  },
  {
    discovery_Domain_Zigbee,
    (discovery_Capability)(discovery_Capability_Sniff | discovery_Capability_Inject | discovery_Capability_Jam | discovery_Capability_SimulateRole),
    (
      CMD(zigbee_ZigbeeCommand_Sniff) |
      CMD(zigbee_ZigbeeCommand_Jam) |
      CMD(zigbee_ZigbeeCommand_EnergyDetection) |
      CMD(zigbee_ZigbeeCommand_Send) |
      CMD(zigbee_ZigbeeCommand_Start) |
      CMD(zigbee_ZigbeeCommand_Stop) |
      CMD(zigbee_ZigbeeCommand_SetNodeAddress) |
      CMD(zigbee_ZigbeeCommand_EndDeviceMode) |
      CMD(zigbee_ZigbeeCommand_CoordinatorMode) |
      CMD(zigbee_ZigbeeCommand_RouterMode) |
      CMD(zigbee_ZigbeeCommand_ManInTheMiddle)
      )
  },
  {
    discovery_Domain_Esb,
    (discovery_Capability)(discovery_Capability_Sniff | discovery_Capability_Inject | discovery_Capability_Jam | discovery_Capability_SimulateRole),
    (
      CMD(esb_ESBCommand_Sniff) |
      CMD(esb_ESBCommand_Send) |
      CMD(esb_ESBCommand_Start) |
      CMD(esb_ESBCommand_Stop) |
      CMD(esb_ESBCommand_SetNodeAddress) |
      CMD(esb_ESBCommand_PrimaryReceiverMode) |
      CMD(esb_ESBCommand_PrimaryTransmitterMode)

      )
  },
  {discovery_Domain__DomainNone, discovery_Capability__CapNone, 0x00000000}
};

#endif
