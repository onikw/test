#ifndef ACTIVITY_INDICATOR_H_
#define ACTIVITY_INDICATOR_H_

/*
 * Watek tla: zbiera okno probek akcelerometru, wola model Edge Impulse,
 * i na zmianie etykiety ustawia kolor LED-paska oraz wysyla 3B RGB przez BLE.
 *
 * Etykiety:
 *   'a' front, 'b' random, 'c' rotation, 'd' sides, 'e' smash, 'f' up,
 *   'z' anomalia (KNN OOD).
 */
void activity_indicator_start(void);

/* Zwraca ostatnia etykiete z modelu ('a'..'f', 'z', lub '-' jesli nic). */
char activity_indicator_get_model_label(void);

/* Zwraca ostatnia aktywnosc jako enum (dla telemetrii). */
enum activity activity_indicator_get_activity(void);

#endif /* ACTIVITY_INDICATOR_H_ */
