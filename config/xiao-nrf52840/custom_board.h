#ifndef CUSTOM_BOARD_H
#define CUSTOM_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

// Seeed XIAO nRF52840 RGB LED (active low)
// LED indices match firmware expectations:
//   LED1      = BSP index 0 -> Red
//   LED2_RED  = BSP index 1 -> Red  (same pin as LED1)
//   LED2_GREEN= BSP index 2 -> Green
//   LED2_BLUE = BSP index 3 -> Blue
#define LEDS_NUMBER    4

#define LED_1          NRF_GPIO_PIN_MAP(0, 26)  // Red
#define LED_2          NRF_GPIO_PIN_MAP(0, 26)  // Red  (LED2_RED, shared with LED1)
#define LED_3          NRF_GPIO_PIN_MAP(0, 30)  // Green
#define LED_4          NRF_GPIO_PIN_MAP(0, 6)   // Blue
#define LED_START      LED_1
#define LED_STOP       LED_4

#define LEDS_ACTIVE_STATE 0

#define LEDS_LIST { LED_1, LED_2, LED_3, LED_4 }
#define LEDS_INV_MASK  LEDS_MASK

#define BSP_LED_0      26
#define BSP_LED_1      26
#define BSP_LED_2      30
#define BSP_LED_3      6

// No user button on XIAO nRF52840
#define BUTTONS_NUMBER       0
#define BUTTONS_ACTIVE_STATE 0
#define BUTTONS_LIST         {}

// UART: D6=TX (P1.11), D7=RX (P1.12)
#define RX_PIN_NUMBER  NRF_GPIO_PIN_MAP(1, 12)
#define TX_PIN_NUMBER  NRF_GPIO_PIN_MAP(1, 11)
#define CTS_PIN_NUMBER 0
#define RTS_PIN_NUMBER 0
#define HWFC           false

#ifdef __cplusplus
}
#endif

#endif // CUSTOM_BOARD_H
