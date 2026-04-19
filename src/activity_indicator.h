#ifndef ACTIVITY_INDICATOR_H_
#define ACTIVITY_INDICATOR_H_

/*
 * Watek tla: czyta akcelerometr z 5 Hz, klasyfikuje aktywnosc i:
 *   - na zmianie aktywnosci: ustawia kolor LED-paska + wysyla 6B przez BLE
 *
 * Format 6B payload (big-endian dla pol wielobajtowych):
 *   [0]   activity_id  (0=unknown, 1=still, 2=walking, 3=running)
 *   [1-3] RGB kolor przypisany do tej aktywnosci
 *   [4-5] |accel| w mg (uint16 BE)
 */
void activity_indicator_start(void);

/* Zwraca ostatni zasymulowany wynik modelu ('a'..'e'). */
char activity_indicator_get_model_label(void);

#endif /* ACTIVITY_INDICATOR_H_ */
