/*
#ifndef LINK_H
#define LINK_H
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "nrf_drv_spis.h"
#include "nrf_drv_spi.h"
#include "nrf.h"

#define LINK_CS 47
#define LINK_MOSI 29
#define LINK_MISO 2
#define LINK_SCK 31

#define MASTER_SPI_INSTANCE 0
#define SLAVE_SPI_INSTANCE 1

typedef enum LinkMode {
	LINK_SLAVE = 0,
	LINK_MASTER = 1,
	RESET_LINK = 0xFF
} LinkMode;


static const nrf_drv_spi_t master_instance = NRF_DRV_SPI_INSTANCE(MASTER_SPI_INSTANCE);
static const nrf_drv_spis_t slave_instance = NRF_DRV_SPIS_INSTANCE(SLAVE_SPI_INSTANCE);

static volatile bool spiTransferDone;

static uint8_t spiTxBuffer[4];
static uint8_t spiRxBuffer[4];

class LinkModule {
	protected:
		LinkMode mode;

	public:

    LinkModule();
		void configureLink(LinkMode mode);

		void resetLink();

    void setupSlave();
    void setupMaster();

		void sendSignalToSlave(int signal);

    static void spiMasterEventHandler(nrf_drv_spi_evt_t const* event, void * context);
    static void spiSlaveEventHandler(nrf_drv_spis_event_t event);
};
#endif
*/
