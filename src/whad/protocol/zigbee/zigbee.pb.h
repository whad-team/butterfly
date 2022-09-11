/* Automatically generated nanopb header */
/* Generated by nanopb-0.4.7-dev */

#ifndef PB_ZIGBEE_PROTOCOL_ZIGBEE_ZIGBEE_PB_H_INCLUDED
#define PB_ZIGBEE_PROTOCOL_ZIGBEE_ZIGBEE_PB_H_INCLUDED
#include <pb.h>

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Enum definitions */
typedef enum _zigbee_ZigbeeCommand { /* *
 Low-level commands */
    /* Set Node address. */
    zigbee_ZigbeeCommand_SetNodeAddress = 0, 
    /* Sniff packets. */
    zigbee_ZigbeeCommand_Sniff = 1, 
    /* Jam packets. */
    zigbee_ZigbeeCommand_Jam = 2, 
    /* Energy detection */
    zigbee_ZigbeeCommand_EnergyDetection = 3, 
    /* Send packets. */
    zigbee_ZigbeeCommand_Send = 4, 
    zigbee_ZigbeeCommand_SendRaw = 5, 
    /* End Device mode. */
    zigbee_ZigbeeCommand_EndDeviceMode = 6, 
    /* Coordinator mode. */
    zigbee_ZigbeeCommand_CoordinatorMode = 7, 
    /* Router mode. */
    zigbee_ZigbeeCommand_RouterMode = 8, 
    /* Start and Stop commands shared with node-related mode. */
    zigbee_ZigbeeCommand_Start = 9, 
    zigbee_ZigbeeCommand_Stop = 10, 
    /* Man-in-the-Middle mode */
    zigbee_ZigbeeCommand_ManInTheMiddle = 11 
} zigbee_ZigbeeCommand;

typedef enum _zigbee_ZigbeeMitmRole { 
    zigbee_ZigbeeMitmRole_REACTIVE_JAMMER = 0, 
    zigbee_ZigbeeMitmRole_CORRECTOR = 1 
} zigbee_ZigbeeMitmRole;

typedef enum _zigbee_AddressType { 
    zigbee_AddressType_SHORT = 0, 
    zigbee_AddressType_EXTENDED = 1 
} zigbee_AddressType;

/* Struct definitions */
/* *
 StartCmd

 Enable node-related modes. */
typedef struct _zigbee_StartCmd { 
    char dummy_field;
} zigbee_StartCmd;

/* *
 StopCmd

 Disable node-related modes. */
typedef struct _zigbee_StopCmd { 
    char dummy_field;
} zigbee_StopCmd;

/* *
 CoordinatorCmd

 Enable Coordinator mode. */
typedef struct _zigbee_CoordinatorCmd { 
    uint32_t channel;
} zigbee_CoordinatorCmd;

/* *
 EndDeviceCmd

 Enable End Device mode. */
typedef struct _zigbee_EndDeviceCmd { 
    uint32_t channel;
} zigbee_EndDeviceCmd;

/* *
 EnergyDetectionCmd

 Enable energy detection mode. */
typedef struct _zigbee_EnergyDetectionCmd { 
    uint32_t channel;
} zigbee_EnergyDetectionCmd;

typedef struct _zigbee_EnergyDetectionSample { 
    uint32_t sample;
    uint32_t timestamp;
} zigbee_EnergyDetectionSample;

typedef struct _zigbee_JamCmd { 
    uint32_t channel;
} zigbee_JamCmd;

typedef struct _zigbee_Jammed { 
    uint32_t timestamp;
} zigbee_Jammed;

typedef struct _zigbee_ManInTheMiddleCmd { 
    zigbee_ZigbeeMitmRole role;
} zigbee_ManInTheMiddleCmd;

typedef PB_BYTES_ARRAY_T(255) zigbee_PduReceived_pdu_t;
typedef struct _zigbee_PduReceived { 
    uint32_t channel;
    bool has_rssi;
    int32_t rssi;
    bool has_timestamp;
    uint32_t timestamp;
    bool has_fcs_validity;
    bool fcs_validity;
    zigbee_PduReceived_pdu_t pdu;
} zigbee_PduReceived;

typedef PB_BYTES_ARRAY_T(255) zigbee_RawPduReceived_pdu_t;
typedef struct _zigbee_RawPduReceived { 
    uint32_t channel;
    bool has_rssi;
    int32_t rssi;
    bool has_timestamp;
    uint32_t timestamp;
    bool has_fcs_validity;
    bool fcs_validity;
    zigbee_RawPduReceived_pdu_t pdu;
    uint32_t fcs;
} zigbee_RawPduReceived;

/* *
 RouterCmd

 Enable Router mode. */
typedef struct _zigbee_RouterCmd { 
    uint32_t channel;
} zigbee_RouterCmd;

typedef PB_BYTES_ARRAY_T(255) zigbee_SendCmd_pdu_t;
/* *
 SendCmd

 Transmit Zigbee packets on a single channel. */
typedef struct _zigbee_SendCmd { 
    uint32_t channel;
    zigbee_SendCmd_pdu_t pdu;
} zigbee_SendCmd;

typedef PB_BYTES_ARRAY_T(255) zigbee_SendRawCmd_pdu_t;
typedef struct _zigbee_SendRawCmd { 
    uint32_t channel;
    zigbee_SendRawCmd_pdu_t pdu;
    uint32_t fcs;
} zigbee_SendRawCmd;

typedef struct _zigbee_SetNodeAddressCmd { 
    uint64_t address;
    zigbee_AddressType address_type;
} zigbee_SetNodeAddressCmd;

typedef struct _zigbee_SniffCmd { 
    /* Channel must be specified, the device will only
listen on this specific channel. */
    uint32_t channel;
} zigbee_SniffCmd;

typedef struct _zigbee_Message { 
    pb_size_t which_msg;
    union {
        /* Messages */
        zigbee_SetNodeAddressCmd set_node_addr;
        zigbee_SniffCmd sniff;
        zigbee_JamCmd jam;
        zigbee_EnergyDetectionCmd ed;
        zigbee_SendCmd send;
        zigbee_SendRawCmd send_raw;
        zigbee_EndDeviceCmd end_device;
        zigbee_RouterCmd router;
        zigbee_CoordinatorCmd coordinator;
        zigbee_StartCmd start;
        zigbee_StopCmd stop;
        zigbee_ManInTheMiddleCmd mitm;
        /* Notifications */
        zigbee_Jammed jammed;
        zigbee_EnergyDetectionSample ed_sample;
        zigbee_RawPduReceived raw_pdu;
        zigbee_PduReceived pdu;
    } msg;
} zigbee_Message;


/* Helper constants for enums */
#define _zigbee_ZigbeeCommand_MIN zigbee_ZigbeeCommand_SetNodeAddress
#define _zigbee_ZigbeeCommand_MAX zigbee_ZigbeeCommand_ManInTheMiddle
#define _zigbee_ZigbeeCommand_ARRAYSIZE ((zigbee_ZigbeeCommand)(zigbee_ZigbeeCommand_ManInTheMiddle+1))

#define _zigbee_ZigbeeMitmRole_MIN zigbee_ZigbeeMitmRole_REACTIVE_JAMMER
#define _zigbee_ZigbeeMitmRole_MAX zigbee_ZigbeeMitmRole_CORRECTOR
#define _zigbee_ZigbeeMitmRole_ARRAYSIZE ((zigbee_ZigbeeMitmRole)(zigbee_ZigbeeMitmRole_CORRECTOR+1))

#define _zigbee_AddressType_MIN zigbee_AddressType_SHORT
#define _zigbee_AddressType_MAX zigbee_AddressType_EXTENDED
#define _zigbee_AddressType_ARRAYSIZE ((zigbee_AddressType)(zigbee_AddressType_EXTENDED+1))


#ifdef __cplusplus
extern "C" {
#endif

/* Initializer values for message structs */
#define zigbee_SetNodeAddressCmd_init_default    {0, _zigbee_AddressType_MIN}
#define zigbee_SniffCmd_init_default             {0}
#define zigbee_EnergyDetectionCmd_init_default   {0}
#define zigbee_JamCmd_init_default               {0}
#define zigbee_SendCmd_init_default              {0, {0, {0}}}
#define zigbee_SendRawCmd_init_default           {0, {0, {0}}, 0}
#define zigbee_EndDeviceCmd_init_default         {0}
#define zigbee_RouterCmd_init_default            {0}
#define zigbee_CoordinatorCmd_init_default       {0}
#define zigbee_StartCmd_init_default             {0}
#define zigbee_StopCmd_init_default              {0}
#define zigbee_ManInTheMiddleCmd_init_default    {_zigbee_ZigbeeMitmRole_MIN}
#define zigbee_Jammed_init_default               {0}
#define zigbee_EnergyDetectionSample_init_default {0, 0}
#define zigbee_RawPduReceived_init_default       {0, false, 0, false, 0, false, 0, {0, {0}}, 0}
#define zigbee_PduReceived_init_default          {0, false, 0, false, 0, false, 0, {0, {0}}}
#define zigbee_Message_init_default              {0, {zigbee_SetNodeAddressCmd_init_default}}
#define zigbee_SetNodeAddressCmd_init_zero       {0, _zigbee_AddressType_MIN}
#define zigbee_SniffCmd_init_zero                {0}
#define zigbee_EnergyDetectionCmd_init_zero      {0}
#define zigbee_JamCmd_init_zero                  {0}
#define zigbee_SendCmd_init_zero                 {0, {0, {0}}}
#define zigbee_SendRawCmd_init_zero              {0, {0, {0}}, 0}
#define zigbee_EndDeviceCmd_init_zero            {0}
#define zigbee_RouterCmd_init_zero               {0}
#define zigbee_CoordinatorCmd_init_zero          {0}
#define zigbee_StartCmd_init_zero                {0}
#define zigbee_StopCmd_init_zero                 {0}
#define zigbee_ManInTheMiddleCmd_init_zero       {_zigbee_ZigbeeMitmRole_MIN}
#define zigbee_Jammed_init_zero                  {0}
#define zigbee_EnergyDetectionSample_init_zero   {0, 0}
#define zigbee_RawPduReceived_init_zero          {0, false, 0, false, 0, false, 0, {0, {0}}, 0}
#define zigbee_PduReceived_init_zero             {0, false, 0, false, 0, false, 0, {0, {0}}}
#define zigbee_Message_init_zero                 {0, {zigbee_SetNodeAddressCmd_init_zero}}

/* Field tags (for use in manual encoding/decoding) */
#define zigbee_CoordinatorCmd_channel_tag        1
#define zigbee_EndDeviceCmd_channel_tag          1
#define zigbee_EnergyDetectionCmd_channel_tag    1
#define zigbee_EnergyDetectionSample_sample_tag  1
#define zigbee_EnergyDetectionSample_timestamp_tag 2
#define zigbee_JamCmd_channel_tag                1
#define zigbee_Jammed_timestamp_tag              1
#define zigbee_ManInTheMiddleCmd_role_tag        1
#define zigbee_PduReceived_channel_tag           1
#define zigbee_PduReceived_rssi_tag              2
#define zigbee_PduReceived_timestamp_tag         3
#define zigbee_PduReceived_fcs_validity_tag      4
#define zigbee_PduReceived_pdu_tag               5
#define zigbee_RawPduReceived_channel_tag        1
#define zigbee_RawPduReceived_rssi_tag           2
#define zigbee_RawPduReceived_timestamp_tag      3
#define zigbee_RawPduReceived_fcs_validity_tag   4
#define zigbee_RawPduReceived_pdu_tag            5
#define zigbee_RawPduReceived_fcs_tag            6
#define zigbee_RouterCmd_channel_tag             1
#define zigbee_SendCmd_channel_tag               1
#define zigbee_SendCmd_pdu_tag                   2
#define zigbee_SendRawCmd_channel_tag            1
#define zigbee_SendRawCmd_pdu_tag                2
#define zigbee_SendRawCmd_fcs_tag                3
#define zigbee_SetNodeAddressCmd_address_tag     1
#define zigbee_SetNodeAddressCmd_address_type_tag 2
#define zigbee_SniffCmd_channel_tag              1
#define zigbee_Message_set_node_addr_tag         1
#define zigbee_Message_sniff_tag                 2
#define zigbee_Message_jam_tag                   3
#define zigbee_Message_ed_tag                    4
#define zigbee_Message_send_tag                  5
#define zigbee_Message_send_raw_tag              6
#define zigbee_Message_end_device_tag            7
#define zigbee_Message_router_tag                8
#define zigbee_Message_coordinator_tag           9
#define zigbee_Message_start_tag                 10
#define zigbee_Message_stop_tag                  11
#define zigbee_Message_mitm_tag                  12
#define zigbee_Message_jammed_tag                13
#define zigbee_Message_ed_sample_tag             14
#define zigbee_Message_raw_pdu_tag               15
#define zigbee_Message_pdu_tag                   16

/* Struct field encoding specification for nanopb */
#define zigbee_SetNodeAddressCmd_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT64,   address,           1) \
X(a, STATIC,   SINGULAR, UENUM,    address_type,      2)
#define zigbee_SetNodeAddressCmd_CALLBACK NULL
#define zigbee_SetNodeAddressCmd_DEFAULT NULL

#define zigbee_SniffCmd_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   channel,           1)
#define zigbee_SniffCmd_CALLBACK NULL
#define zigbee_SniffCmd_DEFAULT NULL

#define zigbee_EnergyDetectionCmd_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   channel,           1)
#define zigbee_EnergyDetectionCmd_CALLBACK NULL
#define zigbee_EnergyDetectionCmd_DEFAULT NULL

#define zigbee_JamCmd_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   channel,           1)
#define zigbee_JamCmd_CALLBACK NULL
#define zigbee_JamCmd_DEFAULT NULL

#define zigbee_SendCmd_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   channel,           1) \
X(a, STATIC,   SINGULAR, BYTES,    pdu,               2)
#define zigbee_SendCmd_CALLBACK NULL
#define zigbee_SendCmd_DEFAULT NULL

#define zigbee_SendRawCmd_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   channel,           1) \
X(a, STATIC,   SINGULAR, BYTES,    pdu,               2) \
X(a, STATIC,   SINGULAR, UINT32,   fcs,               3)
#define zigbee_SendRawCmd_CALLBACK NULL
#define zigbee_SendRawCmd_DEFAULT NULL

#define zigbee_EndDeviceCmd_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   channel,           1)
#define zigbee_EndDeviceCmd_CALLBACK NULL
#define zigbee_EndDeviceCmd_DEFAULT NULL

#define zigbee_RouterCmd_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   channel,           1)
#define zigbee_RouterCmd_CALLBACK NULL
#define zigbee_RouterCmd_DEFAULT NULL

#define zigbee_CoordinatorCmd_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   channel,           1)
#define zigbee_CoordinatorCmd_CALLBACK NULL
#define zigbee_CoordinatorCmd_DEFAULT NULL

#define zigbee_StartCmd_FIELDLIST(X, a) \

#define zigbee_StartCmd_CALLBACK NULL
#define zigbee_StartCmd_DEFAULT NULL

#define zigbee_StopCmd_FIELDLIST(X, a) \

#define zigbee_StopCmd_CALLBACK NULL
#define zigbee_StopCmd_DEFAULT NULL

#define zigbee_ManInTheMiddleCmd_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UENUM,    role,              1)
#define zigbee_ManInTheMiddleCmd_CALLBACK NULL
#define zigbee_ManInTheMiddleCmd_DEFAULT NULL

#define zigbee_Jammed_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   timestamp,         1)
#define zigbee_Jammed_CALLBACK NULL
#define zigbee_Jammed_DEFAULT NULL

#define zigbee_EnergyDetectionSample_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   sample,            1) \
X(a, STATIC,   SINGULAR, UINT32,   timestamp,         2)
#define zigbee_EnergyDetectionSample_CALLBACK NULL
#define zigbee_EnergyDetectionSample_DEFAULT NULL

#define zigbee_RawPduReceived_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   channel,           1) \
X(a, STATIC,   OPTIONAL, INT32,    rssi,              2) \
X(a, STATIC,   OPTIONAL, UINT32,   timestamp,         3) \
X(a, STATIC,   OPTIONAL, BOOL,     fcs_validity,      4) \
X(a, STATIC,   SINGULAR, BYTES,    pdu,               5) \
X(a, STATIC,   SINGULAR, UINT32,   fcs,               6)
#define zigbee_RawPduReceived_CALLBACK NULL
#define zigbee_RawPduReceived_DEFAULT NULL

#define zigbee_PduReceived_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   channel,           1) \
X(a, STATIC,   OPTIONAL, INT32,    rssi,              2) \
X(a, STATIC,   OPTIONAL, UINT32,   timestamp,         3) \
X(a, STATIC,   OPTIONAL, BOOL,     fcs_validity,      4) \
X(a, STATIC,   SINGULAR, BYTES,    pdu,               5)
#define zigbee_PduReceived_CALLBACK NULL
#define zigbee_PduReceived_DEFAULT NULL

#define zigbee_Message_FIELDLIST(X, a) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,set_node_addr,msg.set_node_addr),   1) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,sniff,msg.sniff),   2) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,jam,msg.jam),   3) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,ed,msg.ed),   4) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,send,msg.send),   5) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,send_raw,msg.send_raw),   6) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,end_device,msg.end_device),   7) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,router,msg.router),   8) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,coordinator,msg.coordinator),   9) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,start,msg.start),  10) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,stop,msg.stop),  11) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,mitm,msg.mitm),  12) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,jammed,msg.jammed),  13) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,ed_sample,msg.ed_sample),  14) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,raw_pdu,msg.raw_pdu),  15) \
X(a, STATIC,   ONEOF,    MESSAGE,  (msg,pdu,msg.pdu),  16)
#define zigbee_Message_CALLBACK NULL
#define zigbee_Message_DEFAULT NULL
#define zigbee_Message_msg_set_node_addr_MSGTYPE zigbee_SetNodeAddressCmd
#define zigbee_Message_msg_sniff_MSGTYPE zigbee_SniffCmd
#define zigbee_Message_msg_jam_MSGTYPE zigbee_JamCmd
#define zigbee_Message_msg_ed_MSGTYPE zigbee_EnergyDetectionCmd
#define zigbee_Message_msg_send_MSGTYPE zigbee_SendCmd
#define zigbee_Message_msg_send_raw_MSGTYPE zigbee_SendRawCmd
#define zigbee_Message_msg_end_device_MSGTYPE zigbee_EndDeviceCmd
#define zigbee_Message_msg_router_MSGTYPE zigbee_RouterCmd
#define zigbee_Message_msg_coordinator_MSGTYPE zigbee_CoordinatorCmd
#define zigbee_Message_msg_start_MSGTYPE zigbee_StartCmd
#define zigbee_Message_msg_stop_MSGTYPE zigbee_StopCmd
#define zigbee_Message_msg_mitm_MSGTYPE zigbee_ManInTheMiddleCmd
#define zigbee_Message_msg_jammed_MSGTYPE zigbee_Jammed
#define zigbee_Message_msg_ed_sample_MSGTYPE zigbee_EnergyDetectionSample
#define zigbee_Message_msg_raw_pdu_MSGTYPE zigbee_RawPduReceived
#define zigbee_Message_msg_pdu_MSGTYPE zigbee_PduReceived

extern const pb_msgdesc_t zigbee_SetNodeAddressCmd_msg;
extern const pb_msgdesc_t zigbee_SniffCmd_msg;
extern const pb_msgdesc_t zigbee_EnergyDetectionCmd_msg;
extern const pb_msgdesc_t zigbee_JamCmd_msg;
extern const pb_msgdesc_t zigbee_SendCmd_msg;
extern const pb_msgdesc_t zigbee_SendRawCmd_msg;
extern const pb_msgdesc_t zigbee_EndDeviceCmd_msg;
extern const pb_msgdesc_t zigbee_RouterCmd_msg;
extern const pb_msgdesc_t zigbee_CoordinatorCmd_msg;
extern const pb_msgdesc_t zigbee_StartCmd_msg;
extern const pb_msgdesc_t zigbee_StopCmd_msg;
extern const pb_msgdesc_t zigbee_ManInTheMiddleCmd_msg;
extern const pb_msgdesc_t zigbee_Jammed_msg;
extern const pb_msgdesc_t zigbee_EnergyDetectionSample_msg;
extern const pb_msgdesc_t zigbee_RawPduReceived_msg;
extern const pb_msgdesc_t zigbee_PduReceived_msg;
extern const pb_msgdesc_t zigbee_Message_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define zigbee_SetNodeAddressCmd_fields &zigbee_SetNodeAddressCmd_msg
#define zigbee_SniffCmd_fields &zigbee_SniffCmd_msg
#define zigbee_EnergyDetectionCmd_fields &zigbee_EnergyDetectionCmd_msg
#define zigbee_JamCmd_fields &zigbee_JamCmd_msg
#define zigbee_SendCmd_fields &zigbee_SendCmd_msg
#define zigbee_SendRawCmd_fields &zigbee_SendRawCmd_msg
#define zigbee_EndDeviceCmd_fields &zigbee_EndDeviceCmd_msg
#define zigbee_RouterCmd_fields &zigbee_RouterCmd_msg
#define zigbee_CoordinatorCmd_fields &zigbee_CoordinatorCmd_msg
#define zigbee_StartCmd_fields &zigbee_StartCmd_msg
#define zigbee_StopCmd_fields &zigbee_StopCmd_msg
#define zigbee_ManInTheMiddleCmd_fields &zigbee_ManInTheMiddleCmd_msg
#define zigbee_Jammed_fields &zigbee_Jammed_msg
#define zigbee_EnergyDetectionSample_fields &zigbee_EnergyDetectionSample_msg
#define zigbee_RawPduReceived_fields &zigbee_RawPduReceived_msg
#define zigbee_PduReceived_fields &zigbee_PduReceived_msg
#define zigbee_Message_fields &zigbee_Message_msg

/* Maximum encoded size of messages (where known) */
#define zigbee_CoordinatorCmd_size               6
#define zigbee_EndDeviceCmd_size                 6
#define zigbee_EnergyDetectionCmd_size           6
#define zigbee_EnergyDetectionSample_size        12
#define zigbee_JamCmd_size                       6
#define zigbee_Jammed_size                       6
#define zigbee_ManInTheMiddleCmd_size            2
#define zigbee_Message_size                      292
#define zigbee_PduReceived_size                  283
#define zigbee_RawPduReceived_size               289
#define zigbee_RouterCmd_size                    6
#define zigbee_SendCmd_size                      264
#define zigbee_SendRawCmd_size                   270
#define zigbee_SetNodeAddressCmd_size            13
#define zigbee_SniffCmd_size                     6
#define zigbee_StartCmd_size                     0
#define zigbee_StopCmd_size                      0

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
