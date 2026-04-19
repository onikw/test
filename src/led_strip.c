#include "led_strip.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_strip, LOG_LEVEL_INF);

#define NUM_LEDS 8  /* musi sie zgadzac z chain-length w overlay */

static const struct device *strip = DEVICE_DT_GET(DT_ALIAS(led_strip));
static struct led_rgb buf[NUM_LEDS];

int led_strip_module_init(void)
{
	if (!device_is_ready(strip)) {
		LOG_ERR("LED strip nie gotowy");
		return -ENODEV;
	}
	led_strip_set_all(0, 0, 0);
	LOG_INF("LED strip gotowy (%d LEDow)", NUM_LEDS);
	return 0;
}

int led_strip_set_all(uint8_t r, uint8_t g, uint8_t b)
{
	for (int i = 0; i < NUM_LEDS; i++) {
		buf[i].r = r;
		buf[i].g = g;
		buf[i].b = b;
	}
	int err = led_strip_update_rgb(strip, buf, NUM_LEDS);
	if (err) {
		LOG_ERR("update_rgb err=%d", err);
	}
	printk("LED strip -> RGB=(%u,%u,%u)\n", r, g, b);
	return err;
}
