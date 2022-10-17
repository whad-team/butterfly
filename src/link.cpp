/*
#include "link.h"
#include "bsp.h"
#include "core.h"

void LinkModule::spiMasterEventHandler(nrf_drv_spi_evt_t const* event, void * context) {
    spiTransferDone = true;
}

void LinkModule::spiSlaveEventHandler(nrf_drv_spis_event_t event) {
  spiTransferDone = true;

  if (event.evt_type == NRF_DRV_SPIS_XFER_DONE)
  {
    int signal = spiRxBuffer[0] | (spiRxBuffer[1] << 8)| (spiRxBuffer[2] << 8)| (spiRxBuffer[3] << 24);
    Core::instance->setControllerChannel(signal);
    APP_ERROR_CHECK(nrf_drv_spis_buffers_set(&slave_instance, spiTxBuffer, 4, spiRxBuffer, 4));
  }
}

LinkModule::LinkModule() {
  this->mode = RESET_LINK;
}

void LinkModule::configureLink(LinkMode mode) {
  this->mode = mode;
  if (mode == RESET_LINK) {
    this->resetLink();
  }
  else if (mode == LINK_SLAVE) {
    this->setupSlave();
  }
  else if (mode == LINK_MASTER) {
    this->setupMaster();
  }
}


void LinkModule::resetLink() {
}

void LinkModule::setupSlave() {
  NRF_POWER->TASKS_CONSTLAT = 1;
  nrf_drv_spis_config_t spis_config = NRF_DRV_SPIS_DEFAULT_CONFIG;
  spis_config.csn_pin               = LINK_CS;
  spis_config.miso_pin              = LINK_MISO;
  spis_config.mosi_pin              = LINK_MOSI;
  spis_config.sck_pin               = LINK_SCK;
  APP_ERROR_CHECK(nrf_drv_spis_init(&slave_instance, &spis_config, LinkModule::spiSlaveEventHandler));
  APP_ERROR_CHECK(nrf_drv_spis_buffers_set(&slave_instance, spiTxBuffer, 4, spiRxBuffer, 4));


}

void LinkModule::setupMaster() {
  nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
  spi_config.ss_pin   = LINK_CS;
  spi_config.miso_pin = LINK_MISO;
  spi_config.mosi_pin = LINK_MOSI;
  spi_config.sck_pin  = LINK_SCK;
  APP_ERROR_CHECK(nrf_drv_spi_init(&master_instance, &spi_config, LinkModule::spiMasterEventHandler, NULL));
}

void LinkModule::sendSignalToSlave(int signal) {
  spiTransferDone = false;
  memset(spiRxBuffer, 4, 0);
  spiTxBuffer[0] = signal & 0xFF;
  spiTxBuffer[1] = (signal & 0xFF00) >> 8;
  spiTxBuffer[2] = (signal & 0xFF0000) >> 16;
  spiTxBuffer[3] = (signal & 0xFF000000) >> 24;
  APP_ERROR_CHECK(nrf_drv_spi_transfer(&master_instance, spiTxBuffer, 4, spiRxBuffer, 4));
  //while (!spiTransferDone) __WFE();
}

void LinkModule::process() {
  if (spiTransferDone) {

  }
}
*/
