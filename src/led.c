#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(led, LOG_LEVEL_INF);

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static void blink_thread(void)
{
	if (!gpio_is_ready_dt(&led0)) {
		LOG_ERR("LED0 not ready");
		return;
	}
	gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);

	while (1) {
		gpio_pin_toggle_dt(&led0);
		k_msleep(500);
	}
}

K_THREAD_DEFINE(blink_tid, 512, blink_thread, NULL, NULL, NULL, 7, 0, 0);
