#include "activity_indicator.h"
#include "motion.h"
#include "led_strip.h"
#include "ble_led.h"
#include "ei_wrapper.h"
#include "model-parameters/model_metadata.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(activity_ind, LOG_LEVEL_INF);

#define STACK_SIZE 4096
#define PRIORITY 7
#define CONFIDENCE_THRESHOLD 0.6f
#define ANOMALY_THRESHOLD 0.5f
#define WINDOW_TIMEOUT_MS 1000

#define WINDOW_N EI_CLASSIFIER_RAW_SAMPLE_COUNT
#define FEATURES_N EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE

/* 'a'..'f' zgodnie z kolejnoscia z model_variables.h:
 * 0 front, 1 random, 2 rotation, 3 sides, 4 smash, 5 up */
static volatile char last_model_label = '-';
static float features[FEATURES_N];

static void model_color(char label, uint8_t *r, uint8_t *g, uint8_t *b)
{
	switch (label)
	{
	case 'a':
		*r = 0;
		*g = 0;
		*b = 32;
		break; /* front    */
	case 'b':
		*r = 0;
		*g = 0;
		*b = 0;
		break; /* random   */
	case 'c':
		*r = 32;
		*g = 16;
		*b = 0;
		break; /* rotation */
	case 'd':
		*r = 32;
		*g = 0;
		*b = 0;
		break; /* sides    */
	case 'e':
		*r = 32;
		*g = 0;
		*b = 32;
		break; /* smash    */
	case 'f':
		*r = 32;
		*g = 32;
		*b = 32;
		break; /* up       */
	case 'z':
		*r = 0;
		*g = 0;
		*b = 0;
		break; /* anomaly   */
	default:
		*r = 0;
		*g = 0;
		*b = 0;
		break;
	}
}

static void on_model_change(char label, float score, float anomaly,
							const struct motion_sample *sample)
{
	if (label == last_model_label)
	{
		return; /* bez zmian — nie spamuj BLE/LED */
	}
	last_model_label = label;

	uint8_t red, green, blue;
	model_color(label, &red, &green, &blue);
	led_strip_set_all(red, green, blue);

	uint8_t payload[3] = {red, green, blue};
	int err = ble_led_send(payload, sizeof(payload));

	if (sample != NULL)
	{
		LOG_INF("Label=%c score=%.2f anomaly=%.2f RGB=(%u,%u,%u) accel=(%d,%d,%d) ble=%d",
				label, (double)score, (double)anomaly, red, green, blue,
				sample->x, sample->y, sample->z, err);
	}
	else
	{
		LOG_INF("Label=%c score=%.2f anomaly=%.2f RGB=(%u,%u,%u) ble=%d",
				label, (double)score, (double)anomaly, red, green, blue, err);
	}
}

static int collect_window(void)
{
	for (int i = 0; i < WINDOW_N; i++)
	{
		struct motion_sample s;
		if (motion_wait_latest(&s, WINDOW_TIMEOUT_MS) != 0)
		{
			return -1;
		}
		/* UWAGA: jednostki muszą być zgodne z treningiem w EI Studio.
		 * Jeśli "Accelerometer (m/s^2)" — mnożnik 0.00981 (mg -> m/s^2).
		 * Jeśli "g"    — 0.001.
		 * Jeśli "mg"   — 1.0. */
		features[i * 3 + 0] = (float)s.x * 0.00981f;
		features[i * 3 + 1] = (float)s.y * 0.00981f;
		features[i * 3 + 2] = (float)s.z * 0.00981f;
	}
	return 0;
}

static void thread_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	while (1)
	{
		if (collect_window() != 0)
		{
			LOG_WRN("Window collect timeout");
			k_sleep(K_MSEC(200));
			continue;
		}

		int label_idx = -1;
		float score = 0.0f, anomaly = 0.0f;
		int r = ei_classify(features, FEATURES_N, &label_idx, &score, &anomaly);
		if (r != 0)
		{
			LOG_WRN("ei_classify err=%d", r);
			continue;
		}

		struct motion_sample s;
		const struct motion_sample *s_ptr =
			(motion_get_latest(&s) == 0) ? &s : NULL;

		char label;
		if (anomaly > ANOMALY_THRESHOLD)
		{
			label = 'z'; /* out-of-distribution */
		}
		else if (score >= CONFIDENCE_THRESHOLD &&
				 label_idx >= 0 && label_idx < EI_CLASSIFIER_LABEL_COUNT)
		{
			label = (char)('a' + label_idx);
		}
		else
		{
			continue; /* niska pewność i nie-anomalia — zostaw poprzednią */
		}
		on_model_change(label, score, anomaly, s_ptr);
	}
}

K_THREAD_STACK_DEFINE(act_stack, STACK_SIZE);
static struct k_thread act_thread;

void activity_indicator_start(void)
{
	k_thread_create(&act_thread, act_stack, K_THREAD_STACK_SIZEOF(act_stack),
					thread_fn, NULL, NULL, NULL,
					PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&act_thread, "act_ind");
}

char activity_indicator_get_model_label(void)
{
	return last_model_label;
}