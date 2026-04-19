#include "battery.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <errno.h>

LOG_MODULE_REGISTER(battery, LOG_LEVEL_INF);

/* Kanał ADC skonfigurowany w overlay (zephyr,user → AIN0 na P0.04). */
static const struct adc_dt_spec adc_vbat =
	ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

int battery_init(void)
{
	if (!adc_is_ready_dt(&adc_vbat)) {
		LOG_ERR("ADC not ready");
		return -ENODEV;
	}
	if (adc_channel_setup_dt(&adc_vbat) < 0) {
		LOG_ERR("ADC channel setup failed");
		return -EIO;
	}
	LOG_INF("ADC ready (VBAT on AIN0)");
	return 0;
}

int battery_read_mv(int32_t *mv_out)
{
	int16_t sample_raw = 0;
	struct adc_sequence seq = {
		.buffer = &sample_raw,
		.buffer_size = sizeof(sample_raw),
	};

	int ret = adc_sequence_init_dt(&adc_vbat, &seq);
	if (ret < 0) {
		return ret;
	}

	ret = adc_read(adc_vbat.dev, &seq);
	if (ret < 0) {
		return ret;
	}

	int32_t val = sample_raw;
	ret = adc_raw_to_millivolts_dt(&adc_vbat, &val);
	if (ret < 0) {
		return ret;
	}

	/* Kalibracja: dzielnik ~1:2.47 (podmień jeśli zmienisz rezystory). */
	*mv_out = (int32_t)(val * 2.47f);
	return 0;
}
