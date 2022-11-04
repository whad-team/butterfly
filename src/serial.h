#ifndef SERIAL_H
#define SERIAL_H

#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_drv_usbd.h"
#include "nrf_drv_clock.h"
#include "nrf_drv_power.h"

#include "app_usbd_core.h"
#include "app_usbd.h"
#include "app_usbd_string_desc.h"
#include "app_usbd_cdc_acm.h"
#include "app_usbd_serial_num.h"
#include "nrf_cli_uart.h"

#include "core.h"

class Core;

#define RX_BUFFER_SIZE 1024
#define TX_BUFFER_SIZE 1024

#define PREAMBLE_WHAD_1 0xAC
#define PREAMBLE_WHAD_2 0xBE

#define USBD_POWER_DETECTION    true
#define CDC_ACM_COMM_INTERFACE  0
#define CDC_ACM_COMM_EPIN       NRF_DRV_USBD_EPIN2

#define CDC_ACM_DATA_INTERFACE  1
#define CDC_ACM_DATA_EPIN       NRF_DRV_USBD_EPIN1
#define CDC_ACM_DATA_EPOUT      NRF_DRV_USBD_EPOUT1

typedef void (Core::*CoreCallback) (uint8_t*, size_t);

typedef struct rxUartState {
  int index;
  bool decoding;
  uint8_t last_received_byte;
} rxUartState;

typedef struct txUartState {
  size_t size;
  bool waiting;
  bool done;
} txUartState;

class SerialComm {
public:
    CoreCallback inputCallback;
		Core *coreInstance;

    rxUartState rxState;
    txUartState txState;
    uint8_t rxBuffer[RX_BUFFER_SIZE];
    uint8_t txBuffer[TX_BUFFER_SIZE];

    uint8_t currentByte;
		static SerialComm *instance;

		static void cdcAcmHandler(app_usbd_class_inst_t const * p_inst,app_usbd_cdc_acm_user_event_t event);
		static void usbdHandler(app_usbd_event_type_t event);


		SerialComm(CoreCallback inputCallback,Core *coreInstance);

    void readInputByte(uint8_t byte);
    bool send(uint8_t *buffer, size_t size);
		void init();
		void process();

};

APP_USBD_CDC_ACM_GLOBAL_DEF(m_app_cdc_acm,
                            SerialComm::cdcAcmHandler,
                            CDC_ACM_COMM_INTERFACE,
                            CDC_ACM_DATA_INTERFACE,
                            CDC_ACM_COMM_EPIN,
                            CDC_ACM_DATA_EPIN,
                            CDC_ACM_DATA_EPOUT,
                            APP_USBD_CDC_COMM_PROTOCOL_NONE
);
#endif
