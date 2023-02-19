#include "serial.h"
#include "bsp.h"

SerialComm* SerialComm::instance = NULL;

void SerialComm::cdcAcmHandler(app_usbd_class_inst_t const * p_inst,app_usbd_cdc_acm_user_event_t event) {

	switch (event)
	{
		case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN:
		{
			ret_code_t ret = app_usbd_cdc_acm_read(&m_app_cdc_acm,&SerialComm::instance->currentByte,1);
      SerialComm::instance->readInputByte(SerialComm::instance->currentByte);

			UNUSED_VARIABLE(ret);
			break;
		}
		case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE:
			NVIC_SystemReset();
			break;

		case APP_USBD_CDC_ACM_USER_EVT_TX_DONE:
			SerialComm::instance->txState.done = true;
			break;

		case APP_USBD_CDC_ACM_USER_EVT_RX_DONE:
		{
			ret_code_t ret;

			do
			{
       SerialComm::instance->readInputByte(SerialComm::instance->currentByte);

			 ret = app_usbd_cdc_acm_read(&m_app_cdc_acm,&SerialComm::instance->currentByte,1);
			} while (ret == NRF_SUCCESS);

			break;
		}
		default:
			break;
	}
}

void  SerialComm::usbdHandler(app_usbd_event_type_t event)
{
	switch (event)
	{
		case APP_USBD_EVT_DRV_SUSPEND:
			break;
		case APP_USBD_EVT_DRV_RESUME:
			break;
		case APP_USBD_EVT_STARTED:
			break;
		case APP_USBD_EVT_STOPPED:
			app_usbd_disable();
			break;
		case APP_USBD_EVT_POWER_DETECTED:
			if (!nrf_drv_usbd_is_enabled())
			{
				app_usbd_enable();
			}
			break;
		case APP_USBD_EVT_POWER_REMOVED:
			app_usbd_stop();
			break;
		case APP_USBD_EVT_POWER_READY:
			app_usbd_start();
			break;
		default:
			break;
	}
}


SerialComm::SerialComm(CoreCallback inputCallback,Core *coreInstance) {
	instance = this;
  this->coreInstance = coreInstance;
  this->inputCallback = inputCallback;
  this->rxState.index = 0;
  this->rxState.decoding = false;
  this->rxState.last_received_byte = 0x00;

  this->txState.size = 0;
  this->txState.waiting = false;
	this->txState.done = true;
  this->currentByte = 0x00;
	this->init();
}

void SerialComm::readInputByte(uint8_t byte) {
  if (this->rxState.decoding) {
    this->rxBuffer[this->rxState.index++] = byte;
    if (this->rxState.index > 3) {
      uint16_t message_size = (this->rxBuffer[2] | (this->rxBuffer[3] << 8));
      if (this->rxState.index >= message_size+4) {
				(this->coreInstance ->* this->inputCallback)(this->rxBuffer+4, message_size);
        this->rxState.decoding = false;
        this->rxState.index = 0;
      }
    }
  }
  else {
    if (this->rxState.last_received_byte == PREAMBLE_WHAD_1 && byte == PREAMBLE_WHAD_2) {
      this->rxState.decoding = true;
      this->rxBuffer[0] = PREAMBLE_WHAD_1;
      this->rxBuffer[1] = PREAMBLE_WHAD_2;
      this->rxState.index = 2;
    }
  }
  this->rxState.last_received_byte = byte;
}

void SerialComm::init() {

	ret_code_t ret;
	static const app_usbd_config_t usbdConfig = {
		.ev_state_proc = SerialComm::usbdHandler
	};
	ret = nrf_drv_clock_init();
	APP_ERROR_CHECK(ret);

	nrf_drv_clock_lfclk_request(NULL);

	while(!nrf_drv_clock_lfclk_is_running());

	ret = app_timer_init();
	APP_ERROR_CHECK(ret);

	app_usbd_serial_num_generate();

	ret = app_usbd_init(&usbdConfig);
	APP_ERROR_CHECK(ret);

	app_usbd_class_inst_t const * classCdcAcm = app_usbd_cdc_acm_class_inst_get(&m_app_cdc_acm);
	ret = app_usbd_class_append(classCdcAcm);
	APP_ERROR_CHECK(ret);
	if (USBD_POWER_DETECTION)
	{
		ret = app_usbd_power_events_enable();
		APP_ERROR_CHECK(ret);
	}
	else
	{
		app_usbd_enable();
		app_usbd_start();
	}
}

bool SerialComm::send(uint8_t *buffer, size_t size) {
	uint16_t message_size = size;
	this->txBuffer[0] = PREAMBLE_WHAD_1;
	this->txBuffer[1] = PREAMBLE_WHAD_2;
	this->txBuffer[2] = (uint8_t)(message_size & 0x00FF);
	this->txBuffer[3] = (uint8_t)((message_size & 0xFF00) >> 8);
  memcpy(this->txBuffer+4,buffer, size);
  this->txState.size = size+4;
	this->txState.done = false;
  ret_code_t ret = app_usbd_cdc_acm_write(&m_app_cdc_acm,&this->txBuffer,this->txState.size);
	return ret == NRF_SUCCESS;
}

void SerialComm::process() {
	while (app_usbd_event_queue_process());
  /*if (this->txState.waiting && this->txState.done) {
		this->txState.done = false;

    this->txState.waiting = false;
  }*/

}
