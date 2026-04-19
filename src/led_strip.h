#ifndef LED_STRIP_H_
#define LED_STRIP_H_

#include <stdint.h>

/* Inicjalizacja paska (sprawdza gotowosc device, gasi wszystkie). */
int led_strip_module_init(void);

/* Ustaw jednolity kolor RGB na calym pasku. */
int led_strip_set_all(uint8_t r, uint8_t g, uint8_t b);

#endif /* LED_STRIP_H_ */
