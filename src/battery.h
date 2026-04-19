#ifndef BATTERY_H_
#define BATTERY_H_

#include <stdint.h>

/* Przygotowuje ADC (sprawdza gotowość + konfiguruje kanał). */
int battery_init(void);

/* Odczyt napięcia baterii w mV (po przeliczeniu przez dzielnik). */
int battery_read_mv(int32_t *mv_out);

#endif /* BATTERY_H_ */
