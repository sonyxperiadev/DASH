#define LOG_TAG "DASH - ak897x_magnet"

#include <stdlib.h>
#include <string.h>
#include "sensors_log.h"
#include <linux/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <pthread.h>
#include <errno.h>
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_select.h"
#include "sensor_util.h"
#include "sensors_id.h"
#include "sensors_config.h"
#include "sensors_wrapper.h"
#include "sensor_xyz.h"
#include "libs/akm8972/SEMC_APIs.h"

#if defined(AK8975)
#define CLASS_DEVICE_NAME akm8975
#elif defined(AK8972)
#define CLASS_DEVICE_NAME akm8972
#else
#error No AKM chip is defined
#endif

#define DUMMY_DATA "1"

#define TO_STRING_(x) #x
#define TO_STRING(x) TO_STRING_(x)

static const char *ak897x_sysfs_path = TO_STRING(/sys/class/compass/CLASS_DEVICE_NAME);

/* Orientation */
#define EVENT_CODE_MAGV_X ABS_RX
#define EVENT_CODE_MAGV_Y ABS_RY
#define EVENT_CODE_MAGV_Z ABS_RZ
#define EVENT_CODE_ORIENT_STATUS ABS_RUDDER

#define AKM_MAX_INTERVAL INT_MAX

static void *ak897x_read(void *arg);
static int ak897x_set_interval(struct sensor_desc *d, int interval);

static int ak897x_set_delay(struct sensor_api_t *s, int64_t ns)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int err;

	if (ns < d->sensor.minDelay * 1000)
		ns = d->sensor.minDelay * 1000;

	d->applied_delay_ms = ns / (1000 * 1000);  /* ms */

	err = ak897x_set_interval(d, d->applied_delay_ms);
	if (err < 0) {
		ALOGE("%s: ecompass: failed to set interval "
			 "- err = %d\n", __func__, err);
		return -1;
	}

	return 0;
}

static int ak897x_singleshot(struct sensor_desc *d)
{
	return d->sysfs.write(&d->sysfs, "single", DUMMY_DATA,
			      strlen(DUMMY_DATA));
}

static int ak897x_set_interval(struct sensor_desc *d, int interval)
{

	if (interval > AKM_MAX_INTERVAL) {
		interval = AKM_MAX_INTERVAL;
	}
	if (interval < 0) {
		interval = -1;
	}

	return d->sysfs.write_int(&d->sysfs, "interval", interval);
}

static int ak897x_init(struct sensor_api_t *s_api)
{
	int fd;
	struct sensor_desc *d = container_of(s_api, struct sensor_desc, api);

	/* check for availablity */
	fd = open_input_dev_by_name(d->input_name, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		ALOGE("%s: unable to find %s input device!\n", __func__,
			d->input_name);
		return -1;
	}
	close(fd);

	sensors_sysfs_init(&d->sysfs, ak897x_sysfs_path, SYSFS_TYPE_ABS_PATH);
	sensors_select_init(&d->select_worker, ak897x_read, d, -1);

	ALOGE("%s: init OK.\n", __func__);
	return 0;
}

static void ak897x_close(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	d->select_worker.destroy(&d->select_worker);
}

static int ak897x_activate(struct sensor_api_t *s, int enable)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int ret = 0;
	int fd;

	fd = d->select_worker.get_fd(&d->select_worker);
	if (enable && (fd < 0)) {
		fd = open_input_dev_by_name(d->input_name,
						O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			ALOGE("%s: Failed to open input device %s", __func__,
				d->input_name);
			ret = -1;
			goto exit;
		}

		d->select_worker.set_fd(&d->select_worker, fd);
		d->select_worker.resume(&d->select_worker);
	} else if (!enable && (fd > 0)) {

		d->select_worker.suspend(&d->select_worker);
		d->select_worker.set_fd(&d->select_worker, -1);

		ret = ak897x_set_interval(d, -1);
		if (ret < 0) {
			ALOGE("%s: ecompass: failed to set"
			" interval - ret = %d\n", __func__, ret);
			ret = -1;
		}
	}

exit:
	return ret;
}

static int ak897x_form(void)
{
	/* TODO: implement form factor */
	return AKM_ChangeFormFactor(0);
}

static void *ak897x_read(void *arg)
{
	struct input_event evbuf[10];
	struct input_event *event;
	struct sensor_desc *d = arg;
	int fd = d->select_worker.get_fd(&d->select_worker);
	int n;
	int i;
	struct sensor_data_t sd;
	int status = SENSOR_STATUS_ACCURACY_HIGH;

	n = read(fd, evbuf, sizeof(evbuf));
	if (n < 0) {
		ALOGE("%s: read error from fd %d, errno %d", __func__, fd, errno);
		goto exit;
	}
	n = n / sizeof(evbuf[0]);
	for (i = 0; i < n; i++) {
		event = evbuf + i;
		if (event->type == EV_SYN) {
			memset(&sd, 0, sizeof(sd));
			sd.sensor = &d->sensor;
			sd.timestamp = get_current_nano_time();
			sd.data = d->data;
			sd.delay = d->applied_delay_ms;
			sd.status = status;
			sensors_wrapper_data(&sd);
		}
		if (event->type != EV_ABS)
			continue;
		switch (event->code) {
			case EVENT_CODE_ORIENT_STATUS:
				status = event->value;
				break;
			case EVENT_CODE_MAGV_X:
				d->data[0] = event->value;
				break;
			case EVENT_CODE_MAGV_Y:
				d->data[1] = event->value;
				break;
			case EVENT_CODE_MAGV_Z:
				d->data[2] = event->value;
				break;
		}
	}

exit:
	return NULL;
}

static struct sensor_desc ak897xna_magnetic = {
	.sensor = {
		name: AKM_CHIP_NAME" Magnetic Field",
		vendor: "Asahi Kasei Corp.",
		version: sizeof(sensors_event_t),
		handle: SENSOR_MAGNETIC_FIELD_HANDLE,
		type: SENSOR_TYPE_MAGNETIC_FIELD,
		maxRange: AKM_CHIP_MAXRANGE,
		resolution: AKM_CHIP_RESOLUTION,
		power: AKM_CHIP_POWER,
		minDelay: 5000,
	},
	.api = {
		init: ak897x_init,
		activate: ak897x_activate,
		set_delay: ak897x_set_delay,
		close: ak897x_close,
	},
	.map_prefix = "ak897xmagnetic",
	.input_name = "compass",
	.applied_delay_ms = 0,
};

list_constructor(ak897x_init_driver);
void ak897x_init_driver()
{
	sensors_wrapper_register(&ak897xna_magnetic.sensor, &ak897xna_magnetic.api,
				 &ak897xna_magnetic.entry);
}

