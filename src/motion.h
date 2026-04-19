#ifndef MOTION_H_
#define MOTION_H_

#include <stdint.h>

enum activity
{
	ACTIVITY_UNKNOWN = 0,
	ACTIVITY_STILL,
	ACTIVITY_WALKING,
	ACTIVITY_RUNNING,
};

struct motion_sample
{
	int16_t x;
	int16_t y;
	int16_t z;
	uint32_t seq;
	int64_t timestamp_ms;
	enum activity activity;
};

/* Nazwa enumu jako string (do JSON-a). */
const char *activity_str(enum activity a);

/* Inicjalizuje akcelerometr (sprawdza gotowość device). 0 = OK. */
int motion_init(void);

/*
 * Kalibracja: poloz sensor plasko (Z w gore), nieruchomo.
 * Wylicza offset osi i wspolczynnik skali tak, by w spoczynku X=Y=0, Z=+g.
 * Bez kalibracji odczyty surowe (z biasem sensora).
 */
int motion_calibrate(void);

/* Startuje watek probkowania akcelerometru co 20 ms. */
int motion_start_sampling(void);

/* Zwraca ostatnia probke z bufora samplera. */
int motion_get_latest(struct motion_sample *out);

/* Czeka na nowa probke i zwraca ja (timeout_ms: 0=bez czekania, <0=bez limitu). */
int motion_wait_latest(struct motion_sample *out, int32_t timeout_ms);

/*
 * Odczyt akcelerometru w mg (milli-g, 1g = 1000 mg).
 * Po wywolaniu motion_calibrate dane sa skalibrowane.
 */
int motion_read_accel(int16_t *x, int16_t *y, int16_t *z);

/* Klasyfikacja ruchu po magnitudzie (mg). */
enum activity motion_detect_activity(int16_t ax, int16_t ay, int16_t az);

#endif /* MOTION_H_ */
