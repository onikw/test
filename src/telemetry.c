#include "telemetry.h"
#include "battery.h"
#include "motion.h"
#include "activity_indicator.h"

#include <stdio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(telemetry, LOG_LEVEL_INF);

/*
 * Mapowanie napiecia LiPo (mV) -> procent stanu naladowania.
 * Grube przyblizenie liniowe: 3.3V=0%, 4.2V=100%, ciete do 0..100.
 */
static int8_t mv_to_pct(int32_t mv)
{
	if (mv <= 0)
	{
		return -1;
	}
	const int32_t mv_empty = 3300;
	const int32_t mv_full = 4200;
	if (mv <= mv_empty)
	{
		return 0;
	}
	if (mv >= mv_full)
	{
		return 100;
	}
	return (int8_t)(((mv - mv_empty) * 100) / (mv_full - mv_empty));
}

void telemetry_collect(struct telemetry *t, uint32_t counter)
{
	t->counter = counter;
	t->vdd_mv = -1;
	t->accel_x = 0;
	t->accel_y = 0;
	t->accel_z = 0;
	t->model_label = activity_indicator_get_model_label();
	(void)battery_read_mv(&t->vdd_mv);
	t->battery_pct = mv_to_pct(t->vdd_mv);

	struct motion_sample s;
	if (motion_get_latest(&s) == 0)
	{
		t->accel_x = s.x;
		t->accel_y = s.y;
		t->accel_z = s.z;
	}
	t->activity = activity_indicator_get_activity();
	t->confidence_pct = activity_indicator_get_confidence();

	LOG_INF("telemetry model_label=%c activity=%s conf=%u%% batt=%d%%",
			t->model_label, activity_str(t->activity),
			t->confidence_pct, t->battery_pct);
}

int telemetry_build_json(const struct telemetry *t, char *buf, size_t buf_len)
{
	LOG_DBG("Building JSON for telemetry data");

	if (t->battery_pct >= 0)
	{
		return snprintf(buf, buf_len,
						"{\"id\":\"" CONFIG_APP_DEVICE_ID "\","
						"\"accel\":{\"x\":%d,\"y\":%d,\"z\":%d},"
						"\"batt\":%d,"
						"\"activity\":\"%s\","
						"\"confidence\":%u}",
						t->accel_x, t->accel_y, t->accel_z,
						t->battery_pct,
						activity_str(t->activity),
						t->confidence_pct);
	}

	/* Bez wiarygodnego odczytu baterii pomijamy pole "batt". */
	return snprintf(buf, buf_len,
					"{\"id\":\"" CONFIG_APP_DEVICE_ID "\","
					"\"accel\":{\"x\":%d,\"y\":%d,\"z\":%d},"
					"\"activity\":\"%s\","
					"\"confidence\":%u}",
					t->accel_x, t->accel_y, t->accel_z,
					activity_str(t->activity),
					t->confidence_pct);
}
