#include "serial.h"
#include "bsp.h"
#include <whad.h>

uint8_t tmp_buf[64];

SerialComm* SerialComm::instance = NULL;

void SerialComm::cdcAcmHandler(app_usbd_class_inst_t const * p_inst, app_usbd_cdc_acm_user_event_t event) {

	switch (event)
	{
		case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN:
		{
            /*
                Start a read operation, if it succeeds then we will catch data in the
                APP_USBD_CDC_ACM_USER_EVT_RX_DONE event.

                We read at most (RX_BUFFER_SIZE - instance->rxState.index) bytes in
                order to avoid an overflow :)
            */
            ret_code_t ret = app_usbd_cdc_acm_read_any(
                &m_app_cdc_acm,
                instance->rxBuffer,
                RX_BUFFER_SIZE
            );

			UNUSED_VARIABLE(ret);
		}
  	    break;

		case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE:
			NVIC_SystemReset();
			break;

		case APP_USBD_CDC_ACM_USER_EVT_TX_DONE:
            ret_code_t ret;
			//SerialComm::instance->txState.done = true;
            //Core::instance->getLedModule()->off(LED2);
            SerialComm::instance->txInProgress = false;
            whad_transport_data_sent();

            /* Fetch up to RX_BUFFER_SIZE bytes from the internal buffer */
            ret = app_usbd_cdc_acm_read_any(
                &m_app_cdc_acm,
                instance->rxBuffer,
                RX_BUFFER_SIZE
            );
			break;

		case APP_USBD_CDC_ACM_USER_EVT_RX_DONE:
		{
            int size;
			ret_code_t ret;

            /* Exit if read operation did not read anything. */
            size = app_usbd_cdc_acm_rx_size(&m_app_cdc_acm);
            if (size > 0)
            {
                /* Forward read data to WHAD library. */
                whad_transport_data_received(instance->rxBuffer, size);
            }

            /* Send pending data, if any. */
            if (!SerialComm::instance->txInProgress)
            {
                whad_transport_send_pending();
            }

            if (!SerialComm::instance->txInProgress)
            {
                ret = app_usbd_cdc_acm_read_any(
                    &m_app_cdc_acm,
                    instance->rxBuffer,
                    RX_BUFFER_SIZE
                );
            }

            UNUSED_VARIABLE(ret);
        }
        break;


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
  this->txInProgress = false;
	this->init();
}

/**
 * @brief Old input byte processing method.
 *
 * @param byte Input byte
 */

void SerialComm::readInputByte(uint8_t byte) {
  uint8_t *p_pkt;
  if (this->rxState.decoding) {
    this->rxBuffer[this->rxState.index++] = byte;
    if (this->rxState.index > 3) {
      uint16_t message_size = (this->rxBuffer[2] | (this->rxBuffer[3] << 8));
      if (this->rxState.index >= message_size+4) {
        p_pkt = (uint8_t *)malloc(message_size);
        memcpy(p_pkt, this->rxBuffer+4, message_size);
				(this->coreInstance ->* this->inputCallback)(p_pkt, message_size);
        free(p_pkt);
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

/**
 * @brief Process input bytes from RX buffer.
 */

void SerialComm::readInputBytes(void)
{
  int i, start = -1;
  uint8_t *p_pkt;

  if (this->rxState.decoding)
  {
    /*
      If we already received our 2-byte magic value, then process the remaining
      data if we have at least 4 bytes (2-byte magic + 2-byte length field)
    */

    if (this->rxState.index > 3) {
      uint16_t message_size = (this->rxBuffer[2] | (this->rxBuffer[3] << 8));
      if (this->rxState.index >= message_size+4) {

        /*
          Disable IRQ processing while extracting the received message from
          memory, to avoid issues.
        */
        //__disable_irq();

        /* Allocate and copy message payload. */
        p_pkt = (uint8_t *)malloc(message_size);
        memcpy(p_pkt, this->rxBuffer+4, message_size);

        /* Re-enable IRQ processing. */
        //__enable_irq();

        /* Call our message processing callback. */
				(this->coreInstance ->* this->inputCallback)(p_pkt, message_size);

        /* Free previously allocated message. */
        free(p_pkt);

        /* Reset fields to decode a new message. */
        this->rxState.decoding = false;
        this->rxState.index = 0;
      }
    }
  }
  else
  {
    /* If we received at least two bytes, we search for our magic value. */
    if (this->rxState.index >= 2)
    {
      /* Do we have a magic already in buffer ? */
      for (i=0; i<(this->rxState.index - 1); i++)
      {
        if ((this->rxBuffer[i] == PREAMBLE_WHAD_1) && (this->rxBuffer[i+1] == PREAMBLE_WHAD_2) )
        {
          start = i;
          break;
        }
      }

      /* Magic value found ? */
      if (start >= 0)
      {
        /* We found a header, let's put it at the start of our RX buffer if required. */
        if (start > 0)
        {
          for (i=start; i<this->rxState.index; i++)
          {
            this->rxBuffer[i-start] = this->rxBuffer[i];
          }
          this->rxState.index -= start;
        }

        /* Start decoding. */
        this->rxState.decoding = true;
      }
      else
      {
        /* No magic, keep only the last byte if it matches PREAMBLE_WHAD_1. */
        if (this->rxBuffer[this->rxState.index-1] == PREAMBLE_WHAD_1)
        {
          this->rxBuffer[0] = PREAMBLE_WHAD_1;
          this->rxState.index = 1;
        }
        else
        {
          /* Or simply ditch everything. */
          this->rxState.index = 0;
        }
      }
    }
  }
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
  ret_code_t ret = app_usbd_cdc_acm_write(&m_app_cdc_acm,&this->txBuffer,this->txState.size);
	return ret == NRF_SUCCESS;
}

bool SerialComm::send_raw(uint8_t *buffer, size_t size) {
    ret_code_t ret;

    do
    {
        ret = app_usbd_cdc_acm_write(&m_app_cdc_acm, buffer, size);
    } while (ret != NRF_SUCCESS);

    //Core::instance->getLedModule()->on(LED2);
    this->txInProgress = true;
    return true;
}

void SerialComm::process() {
    int ret;
    /* Read awaiting data. */
		/*
    if (!this->txInProgress)
    {
        ret = app_usbd_cdc_acm_read_any(
            &m_app_cdc_acm,
            instance->rxBuffer,
            RX_BUFFER_SIZE
        );
    }*/

    whad_transport_send_pending();

    /*  Process events. */
    app_usbd_event_queue_process();
	//while (app_usbd_event_queue_process());
    //app_usbd_event_queue_process();
  /*
  if (this->txState.waiting && this->txState.done) {
		this->txState.done = false;

    this->txState.waiting = false;
  }*/

}
