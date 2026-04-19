#include "motion.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(motion, LOG_LEVEL_INF);

#define CALIB_SAMPLES 50
#define CALIB_DELAY_MS 20
#define CALIB_SETTLE_MS 500
#define GRAVITY_US 9810000LL /* 9.81 m/s² w µm/s² */
#define SAMPLE_PERIOD_MS 20
#define SAMPLE_STACK_SIZE 1024
#define SAMPLE_PRIORITY 6

static const struct device *accel_dev = DEVICE_DT_GET(DT_ALIAS(accel0));

static int64_t calib_offset_us[3] = {0, 0, 0};
static int64_t calib_scale_num = 1;
static int64_t calib_scale_den = 1;
static bool calibrated = false;
static bool sampler_started = false;
static bool sample_valid = false;
static uint32_t sample_seq = 0;

K_MUTEX_DEFINE(sample_lock);
K_SEM_DEFINE(sample_ready_sem, 0, 1);
K_THREAD_STACK_DEFINE(sample_stack, SAMPLE_STACK_SIZE);
static struct k_thread sample_thread;
static struct motion_sample latest_sample = {
	.x = 0,
	.y = 0,
	.z = 0,
	.activity = ACTIVITY_UNKNOWN,
	.seq = 0,
	.timestamp_ms = 0,
};

static int64_t sv_to_us(const struct sensor_value *v)
{
	return (int64_t)v->val1 * 1000000 + v->val2;
}

/* Newton — calkowitoliczbowy sqrt. */
static int64_t isqrt64(int64_t n)
{
	if (n <= 0)
		return 0;
	int64_t x = n / 2 + 1;
	int64_t y = (x + n / x) / 2;
	while (y < x)
	{
		x = y;
		y = (x + n / x) / 2;
	}
	return x;
}

const char *activity_str(enum activity a)
{
	switch (a)
	{
	case ACTIVITY_SMASH:
		return "smash";
	case ACTIVITY_FRONT:
		return "front";
	case ACTIVITY_ROTATION:
		return "rotation";
	case ACTIVITY_SIDES:
		return "sides";
	case ACTIVITY_UP:
		return "up";
	default:
		return "unknown";
	}
}

int motion_init(void)
{
	if (!device_is_ready(accel_dev))
	{
		LOG_ERR("ADXL345 nie gotowy");
		return -ENODEV;
	}
	LOG_INF("ADXL345 gotowy");
	return 0;
}

int motion_calibrate(void)
{
	if (!device_is_ready(accel_dev))
	{
		return -ENODEV;
	}

	LOG_INF("Kalibracja - poloz plasko, nie ruszaj (%d probek)", CALIB_SAMPLES);
	k_sleep(K_MSEC(CALIB_SETTLE_MS));

	int64_t sum[3] = {0, 0, 0};
	for (int i = 0; i < CALIB_SAMPLES; i++)
	{
		struct sensor_value a[3];
		if (sensor_sample_fetch(accel_dev) < 0)
		{
			LOG_ERR("fetch error podczas kalibracji");
			return -EIO;
		}
		sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_X, &a[0]);
		sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_Y, &a[1]);
		sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_Z, &a[2]);
		sum[0] += sv_to_us(&a[0]);
		sum[1] += sv_to_us(&a[1]);
		sum[2] += sv_to_us(&a[2]);
		k_sleep(K_MSEC(CALIB_DELAY_MS));
	}

	int64_t avg[3] = {
		sum[0] / CALIB_SAMPLES,
		sum[1] / CALIB_SAMPLES,
		sum[2] / CALIB_SAMPLES,
	};

	int64_t mag = isqrt64(avg[0] * avg[0] + avg[1] * avg[1] + avg[2] * avg[2]);
	if (mag == 0)
		mag = GRAVITY_US;

	calib_scale_num = GRAVITY_US;
	calib_scale_den = mag;
	calib_offset_us[0] = avg[0];
	calib_offset_us[1] = avg[1];
	calib_offset_us[2] = avg[2] - mag;
	calibrated = true;

	LOG_INF("Kalibracja OK: |g|=%lld um/s2, scale=%lld/%lld",
			mag, calib_scale_num, calib_scale_den);
	return 0;
}

static int motion_read_accel_hw(int16_t *x, int16_t *y, int16_t *z)
{
	if (!device_is_ready(accel_dev))
	{
		*x = *y = *z = 0;
		return -ENODEV;
	}

	struct sensor_value a[3];
	if (sensor_sample_fetch(accel_dev) < 0)
	{
		*x = *y = *z = 0;
		return -EIO;
	}
	sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_X, &a[0]);
	sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_Y, &a[1]);
	sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_Z, &a[2]);

	int64_t us[3] = {sv_to_us(&a[0]), sv_to_us(&a[1]), sv_to_us(&a[2])};
	if (calibrated)
	{
		us[0] = (us[0] - calib_offset_us[0]) * calib_scale_num / calib_scale_den;
		us[1] = (us[1] - calib_offset_us[1]) * calib_scale_num / calib_scale_den;
		us[2] = (us[2] - calib_offset_us[2]) * calib_scale_num / calib_scale_den;
	}

	/* µm/s² → mg : 1 mg = 9.81 mm/s² = 9810 µm/s² */
	*x = (int16_t)(us[0] / 9810);
	*y = (int16_t)(us[1] / 9810);
	*z = (int16_t)(us[2] / 9810);
	return 0;
}

static void sample_thread_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	while (1)
	{
		int16_t x, y, z;
		if (motion_read_accel_hw(&x, &y, &z) == 0)
		{
			sample_seq++;
			struct motion_sample s = {
				.x = x,
				.y = y,
				.z = z,
				.activity = ACTIVITY_UNKNOWN,
				.seq = sample_seq,
				.timestamp_ms = k_uptime_get(),
			};

			k_mutex_lock(&sample_lock, K_FOREVER);
			latest_sample = s;
			sample_valid = true;
			k_mutex_unlock(&sample_lock);
			k_sem_give(&sample_ready_sem);
		}
		k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
	}
}

int motion_start_sampling(void)
{
	if (!device_is_ready(accel_dev))
	{
		return -ENODEV;
	}
	if (sampler_started)
	{
		return 0;
	}

	k_thread_create(&sample_thread, sample_stack, K_THREAD_STACK_SIZEOF(sample_stack),
					sample_thread_fn, NULL, NULL, NULL,
					SAMPLE_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&sample_thread, "motion_sample");
	sampler_started = true;
	LOG_INF("Motion sampler started (%d ms)", SAMPLE_PERIOD_MS);
	return 0;
}

int motion_get_latest(struct motion_sample *out)
{
	if (out == NULL)
	{
		return -EINVAL;
	}

	k_mutex_lock(&sample_lock, K_FOREVER);
	if (!sample_valid)
	{
		k_mutex_unlock(&sample_lock);
		return -EAGAIN;
	}
	*out = latest_sample;
	k_mutex_unlock(&sample_lock);
	return 0;
}

int motion_wait_latest(struct motion_sample *out, int32_t timeout_ms)
{
	if (out == NULL)
	{
		return -EINVAL;
	}

	k_timeout_t timeout;
	if (timeout_ms < 0)
	{
		timeout = K_FOREVER;
	}
	else
	{
		timeout = K_MSEC(timeout_ms);
	}

	if (k_sem_take(&sample_ready_sem, timeout) != 0)
	{
		return -EAGAIN;
	}

	return motion_get_latest(out);
}

int motion_read_accel(int16_t *x, int16_t *y, int16_t *z)
{
	struct motion_sample s;
	if (motion_get_latest(&s) == 0)
	{
		*x = s.x;
		*y = s.y;
		*z = s.z;
		return 0;
	}

	return motion_read_accel_hw(x, y, z);
}
