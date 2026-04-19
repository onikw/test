#ifndef BLE_LED_H_
#define BLE_LED_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * BLE central + GATT client.
 *
 * Skanuje, laczy sie z peryferalem o adresie CONFIG_APP_BLE_TARGET_MAC,
 * znajduje serwis LED i jego charakterystyke, po czym udostepnia
 * ble_led_send() do zapisu pakietow.
 *
 * UUID charakterystyki:
 *   service:        12345678-1234-5678-1234-56789abcdef0
 *   characteristic: 12345678-1234-5678-1234-56789abcdef1
 */

/* Wlacza BT i zaczyna skanowanie. 0 = OK. */
int ble_led_start(void);

/* True kiedy connection + discovery zakonczone i mozna pisac. */
bool ble_led_is_ready(void);

/* Zapis (write without response). 0 = wystartowano, <0 = blad/no link. */
int ble_led_send(const uint8_t *data, size_t len);

#endif /* BLE_LED_H_ */
