#include "telemetry.h"
#include "battery.h"
#include "motion.h"
#include "activity_indicator.h"

#include <stdio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(telemetry, LOG_LEVEL_INF);

void telemetry_collect(struct telemetry *t, uint32_t counter)
{
	t->counter = counter;
	t->vdd_mv = -1;
	t->accel_x = 0;
	t->accel_y = 0;
	t->accel_z = 0;
	t->model_label = activity_indicator_get_model_label();
	(void)battery_read_mv(&t->vdd_mv);

	struct motion_sample s;
	if (motion_get_latest(&s) == 0)
	{
		t->accel_x = s.x;
		t->accel_y = s.y;
		t->accel_z = s.z;
	}
	t->activity = activity_indicator_get_activity();

	LOG_INF("telemetry model_label=%c activity=%s",
			t->model_label, activity_str(t->activity));
}

int telemetry_build_json(const struct telemetry *t, char *buf, size_t buf_len)
{
	LOG_DBG("Building JSON for telemetry data");

	return snprintf(buf, buf_len,
					"{\"device\":\"nrf7002dk\","
					"\"counter\":%u,"
					"\"vdd_mv\":%d,"
					"\"accel\":{\"x\":%d,\"y\":%d,\"z\":%d},"
					"\"model_label\":\"%c\","
					"\"activity\":\"%s\"}",
					t->counter, t->vdd_mv,
					t->accel_x, t->accel_y, t->accel_z,
					t->model_label,
					activity_str(t->activity));
}
