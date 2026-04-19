#ifndef TELEMETRY_H_
#define TELEMETRY_H_

#include <stdint.h>
#include <stddef.h>
#include "motion.h"

struct telemetry
{
	uint32_t counter;
	int32_t vdd_mv;
	int16_t accel_x;
	int16_t accel_y;
	int16_t accel_z;
	enum activity activity;
	uint8_t confidence_pct;
	int8_t battery_pct;
	char model_label;
};

/* Zbiera bieżące dane z baterii i akcelerometru do struktury. */
void telemetry_collect(struct telemetry *t, uint32_t counter);

/*
 * Serializuje strukturę do JSON-a zgodnie z Mjolnir Ingest API:
 *   { "id": "...", "accel": {x,y,z}, "batt": 0-100,
 *     "activity": "smash|front|rotation|sides|up|unknown",
 *     "confidence": 0-100 }
 * Zwraca długość napisu (bez \0).
 */
int telemetry_build_json(const struct telemetry *t, char *buf, size_t buf_len);

#endif /* TELEMETRY_H_ */
