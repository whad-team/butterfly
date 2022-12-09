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
      CMD(ble_BleCommand_ReactiveJam) |
      CMD(ble_BleCommand_ConnectTo) |
      CMD(ble_BleCommand_Disconnect) |
      CMD(ble_BleCommand_PrepareSequence) |
      CMD(ble_BleCommand_TriggerSequence) |
      CMD(ble_BleCommand_ScanMode) |
      CMD(ble_BleCommand_SetBdAddress)

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
  {
    discovery_Domain_LogitechUnifying,
    (discovery_Capability)(discovery_Capability_Sniff | discovery_Capability_Inject | discovery_Capability_Jam | discovery_Capability_SimulateRole),
    (
      CMD(unifying_UnifyingCommand_Sniff) |
      CMD(unifying_UnifyingCommand_Send) |
      CMD(unifying_UnifyingCommand_Start) |
      CMD(unifying_UnifyingCommand_Stop) |
      CMD(unifying_UnifyingCommand_SetNodeAddress) |
      CMD(unifying_UnifyingCommand_LogitechDongleMode) |
      CMD(unifying_UnifyingCommand_LogitechKeyboardMode) |
      CMD(unifying_UnifyingCommand_LogitechMouseMode) |
      CMD(unifying_UnifyingCommand_SniffPairing)


      )
  },
  {discovery_Domain_Phy,
  (discovery_Capability)(discovery_Capability_Sniff | discovery_Capability_Inject | discovery_Capability_Jam | discovery_Capability_NoRawData),
  (
    CMD(phy_PhyCommand_SetGFSKModulation) |
    CMD(phy_PhyCommand_SetTwoDotFourGhzFrequency) |
    CMD(phy_PhyCommand_SetDataRate) |
    CMD(phy_PhyCommand_SetEndianness) |
    CMD(phy_PhyCommand_SetTXPower) |
    CMD(phy_PhyCommand_SetPacketSize) |
    CMD(phy_PhyCommand_SetSyncWord) |
    CMD(phy_PhyCommand_Sniff) |
    CMD(phy_PhyCommand_Send) |
    CMD(phy_PhyCommand_Start) |
    CMD(phy_PhyCommand_Stop) |
    CMD(phy_PhyCommand_Jam) |
    CMD(phy_PhyCommand_Monitor)

    )
},
  {discovery_Domain__DomainNone, discovery_Capability__CapNone, 0x00000000}
};

#endif
