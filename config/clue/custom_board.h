#ifndef CUSTOM_BOARD_H
#define CUSTOM_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

#define LEDS_NUMBER    2

#define LED_1          NRF_GPIO_PIN_MAP(1,1)   /* P1.01 red status LED */
#define LED_2          NRF_GPIO_PIN_MAP(0,10)  /* P0.10 white LEDs */
#define LED_START      LED_1
#define LED_STOP       LED_2

#define LEDS_ACTIVE_STATE 1

#define LEDS_LIST { LED_1, LED_2 }
#define LEDS_INV_MASK  LEDS_MASK

#define BSP_LED_0      NRF_GPIO_PIN_MAP(1,1)
#define BSP_LED_1      NRF_GPIO_PIN_MAP(0,10)

#define BUTTONS_NUMBER 2

#define BUTTON_1       NRF_GPIO_PIN_MAP(1,2)   /* P1.02 Button A */
#define BUTTON_2       NRF_GPIO_PIN_MAP(1,10)  /* P1.10 Button B */
#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST { BUTTON_1, BUTTON_2 }

#define BSP_BUTTON_0   BUTTON_1
#define BSP_BUTTON_1   BUTTON_2

#define RX_PIN_NUMBER  9
#define TX_PIN_NUMBER  10
#define CTS_PIN_NUMBER 7
#define RTS_PIN_NUMBER 5
#define HWFC           false

#ifdef __cplusplus
}
#endif

#endif
