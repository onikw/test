/*
 * Orchestration: LED strip + BLE GATT (LED control) + WiFi STA +
 * akcelerometr ADXL345 + HTTP POST telemetrii.
 * Logika w modulach: led_strip, ble_led, wifi_sta, battery, motion,
 * telemetry, http_post, led (blink).
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "led_strip.h"
#include "ble_led.h"
#include "wifi_sta.h"
#include "battery.h"
#include "motion.h"
#include "activity_indicator.h"
#include "telemetry.h"
#include "http_post.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

int main(void)
{
	k_sleep(K_SECONDS(1));
	LOG_INF("Booting on %s @ %d MHz", CONFIG_BOARD, SystemCoreClock / MHZ(1));

	(void)led_strip_module_init();
	(void)ble_led_start();
	activity_indicator_start();

	if (motion_init() == 0)
	{
		(void)motion_calibrate();
		if (motion_start_sampling() != 0)
		{
			LOG_WRN("motion sampler start failed");
		}
	}
	else
	{
		LOG_WRN("motion_init failed, activity uses simulation only");
	}

	wifi_sta_start();
	if (wifi_sta_wait_ready(30) != 0)
	{
		return -ETIMEDOUT;
	}

	(void)battery_init();

	if (http_post_init_tls() != 0)
	{
		LOG_ERR("TLS init failed; POST will fail until recovered");
	}

	LOG_INF("Posting to https://%s:%d%s every %d s",
			CONFIG_APP_SERVER_HOST, CONFIG_APP_SERVER_PORT,
			CONFIG_APP_SERVER_URL_PATH, CONFIG_APP_POST_INTERVAL_S);

	uint32_t counter = 0;
	while (1)
	{
		struct telemetry t;
		telemetry_collect(&t, counter);

		if (http_post_telemetry(&t) == 0)
		{
			LOG_INF("POST counter=%u OK", counter);
		}
		counter++;
		k_sleep(K_SECONDS(CONFIG_APP_POST_INTERVAL_S));
	}
}
