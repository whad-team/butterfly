#include <stdbool.h>
#include <stdint.h>
#include "core.h"

#ifdef BOARD_CLUE
#include "nrf.h"
#include "core_cm4.h"

#define SD_INFO_ADDR    0x1000
#define SD_MAGIC        0x51B1E5DB
#define APP_VTOR_ADDR   0x26000

static inline void clue_softdevice_disable(void) {
	volatile uint32_t *sd_magic = (volatile uint32_t *)SD_INFO_ADDR;
	if (*sd_magic == SD_MAGIC) {
		register uint32_t ret __asm("r0");
		__asm volatile("svc %1\n" : "=r"(ret) : "I"(0x11) : "memory");
		(void)ret;
	}
	SCB->VTOR = APP_VTOR_ADDR;
	__DSB();
	__ISB();
}
#endif

int main(void) {
#ifdef BOARD_CLUE
	clue_softdevice_disable();
#endif
	Core core;
	core.init();
	core.loop();
}
