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
	char model_label;
};

/* Zbiera bieżące dane z baterii i akcelerometru do struktury. */
void telemetry_collect(struct telemetry *t, uint32_t counter);

/* Serializuje strukturę do JSON-a. Zwraca długość napisu (bez \0). */
int telemetry_build_json(const struct telemetry *t, char *buf, size_t buf_len);

#endif /* TELEMETRY_H_ */
