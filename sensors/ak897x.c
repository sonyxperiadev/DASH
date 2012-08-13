/*
 * Copyright (C) 2012 Sony Mobile Communications AB.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define LOG_TAG "DASH - ak897x"

#include <stdlib.h>
#include <string.h>
#include <linux/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <pthread.h>
#include <errno.h>
#include "sensors_log.h"
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_select.h"
#include "sensor_util.h"
#include "sensors_id.h"
#include "sensors_config.h"

#if defined(AK8973)
#include <linux/akm8973.h>
#elif defined(AK8975)
#include <linux/i2c/akm8975.h>
#else
#error No AKM chip is defined
#endif

#ifdef ACC_BMA150_INPUT
int bma150_input_request_delay(int *handle, int64_t ns);
#endif

#ifdef ACC_BMA250_INPUT
int bma250_input_request_delay(int *handle, int64_t ns);
#endif

/* Orientation */
#define EVENT_CODE_YAW   ABS_RX
#define EVENT_CODE_PITCH ABS_RY
#define EVENT_CODE_ROLL  ABS_RZ
#define EVENT_CODE_ORIENT_STATUS ABS_RUDDER
#define EVENT_CODE_MAGV_X ABS_HAT0X
#define EVENT_CODE_MAGV_Y ABS_HAT0Y
#define EVENT_CODE_MAGV_Z ABS_BRAKE
#define SENSOR_STATE_MASK (0x7FFF)

enum {
	AXIS_X,
	AXIS_Y,
	AXIS_Z,
	NUM_AXIS
};

static void *ak897x_read(void *arg);

struct sensor_desc {
	struct sensor_t sensor;
	struct sensor_api_t api;

	float scale;
	int data[NUM_AXIS];
	int status;
	int map[NUM_AXIS];
	int sign[NUM_AXIS];
	char *map_prefix;
	void *handle;
	int active;
	int64_t delay;
	int init;
};

struct ak897x_sensor_composition {
	struct sensor_desc orientation;
	struct sensor_desc orientation_raw;
	struct sensor_desc magnetic;
	char *input_name;
	struct sensors_select_t select_worker;
	pthread_mutex_t lock;
	int acc_handle;
	int (*request_acc_delay)(int *handle, int64_t ns);
};

struct config_record {
	int max;
	int min;
	int size;
	int *store;
};

static int ak897x_read_axis(char *prefix, char *key, struct config_record *cr)
{
	int rc = -1;
	int tmp[NUM_AXIS];

	if (!sensors_config_get_key(prefix, key, TYPE_ARRAY_INT, tmp, cr->size)) {
		int i;
		for (i = 0; i < cr->size; i++) {
			if (tmp[i] < cr->min || tmp[i] > cr->max)
				break;
		}
		if (i == cr->size) {
			memcpy(cr->store, tmp, sizeof(tmp));
			rc = 0;
		} else {
			ALOGE("%s: bad config (%s_%s) [%2d %2d %2d]", __func__,
					prefix, key, tmp[AXIS_X], tmp[AXIS_Y],
					tmp[AXIS_Z]);
		}
	} else {
		ALOGE("%s: failed to read %s_%s", __func__, prefix, key);
	}
	return rc;
}

static void ak897x_read_sensor_map(struct sensor_desc *d)
{
	struct config_record rec;

	if (!sensors_have_config_file()) {
		ALOGI("%s: No config file found. Using default config.",
		     __func__);
		return;
	}
	rec.size = NUM_AXIS;
	rec.min = AXIS_X;
	rec.max = AXIS_Z;
	rec.store = d->map;
	ak897x_read_axis(d->map_prefix, "axis_map", &rec);

	rec.min = -1;
	rec.max = 1;
	rec.store = d->sign;
	ak897x_read_axis(d->map_prefix, "axis_sign", &rec);

	ALOGD("%s: %s: axis map [%2d %2d %2d]", __func__, d->sensor.name,
	     d->map[AXIS_X], d->map[AXIS_Y], d->map[AXIS_Z]);
	ALOGD("%s: %s: axis sign [%2d %2d %2d]", __func__, d->sensor.name,
	     d->sign[AXIS_X], d->sign[AXIS_Y], d->sign[AXIS_Z]);
}

static int ak897x_set_delay(struct sensor_api_t *s, int64_t ns)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	struct ak897x_sensor_composition *sc = d->handle;

	d->delay = ns;

	if (d->sensor.type == SENSOR_TYPE_ORIENTATION) {
		int err;
		err = sc->request_acc_delay(&sc->acc_handle, ns);
		if (err)
			ALOGE("%s: unable to set accelerometer delay!\n", __func__);
	}
	sc->select_worker.set_delay(&sc->select_worker, ns);

	return 0;
}

static void ak897x_compass_register(struct ak897x_sensor_composition *sc)
{
	pthread_mutex_init(&sc->lock, NULL);
	sc->orientation.handle = sc->orientation_raw.handle =
				sc->magnetic.handle = sc;
	sensors_list_register(&sc->orientation.sensor, &sc->orientation.api);
	sensors_list_register(&sc->orientation_raw.sensor,
			      &sc->orientation_raw.api);
	sensors_list_register(&sc->magnetic.sensor, &sc->magnetic.api);
}

static int ak897x_init(struct sensor_api_t *s_api)
{
	int fd;
	struct sensor_desc *d = container_of(s_api, struct sensor_desc, api);
	struct ak897x_sensor_composition *sc = d->handle;
	int init = sc->orientation.init || sc->orientation_raw.init ||
		sc->magnetic.init;

	d->init = 1;

	/* check for availablity */
	fd = open_input_dev_by_name(sc->input_name, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		ALOGE("%s: unable to find %s input device!\n", __func__,
			sc->input_name);
		return -1;
	}
	close(fd);

	if (init)
		return 0;

	ak897x_read_sensor_map(&sc->orientation);
	ak897x_read_sensor_map(&sc->orientation_raw);
	ak897x_read_sensor_map(&sc->magnetic);
	sensors_select_init(&sc->select_worker, ak897x_read, sc, -1);
	return 0;
}

static void ak897x_close(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	struct ak897x_sensor_composition *sc = d->handle;

	d->init = 0;
	if (!(sc->orientation.init || sc->orientation_raw.init ||
				sc->magnetic.init))
		sc->select_worker.destroy(&sc->select_worker);
}

static int ak897x_activate(struct sensor_api_t *s, int enable)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	struct ak897x_sensor_composition *sc = d->handle;
	int ret = 0;
	int fd = sc->select_worker.get_fd(&sc->select_worker);

	d->active = enable;
	if (enable && (fd < 0)) {
		fd = open_input_dev_by_name(sc->input_name,
						O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			ALOGE("%s: Failed to open input device %s", __func__,
				sc->input_name);
			ret = -1;
			goto exit;
		}
		sc->select_worker.set_fd(&sc->select_worker, fd);
		sc->select_worker.resume(&sc->select_worker);
	} else if (!enable && (fd > 0)) {
		if (!sc->magnetic.active &&
		    !sc->orientation.active &&
		    !sc->orientation_raw.active) {
			sc->select_worker.suspend(&sc->select_worker);
			sc->select_worker.set_fd(&sc->select_worker, -1);
		}
	}
	if (!enable && !sc->orientation.active && !sc->orientation_raw.active) {
		int err;
		err = sc->request_acc_delay(&sc->acc_handle, 0);
		if (err)
			ALOGE("%s: unable to set accelerometer delay!\n", __func__);
	}

exit:
	return ret;
}

static void scale_and_map(sensors_event_t *d, struct sensor_desc *s)
{
	int i;
	for (i = 0; i < 3; i++)
		d->orientation.v[i] = s->data[s->map[i]] * s->sign[i] * s->scale;
}

static void *ak897x_read(void *arg)
{
	struct input_event evbuf[10];
	struct input_event *event;
	struct ak897x_sensor_composition *sc = arg;
	int fd = sc->select_worker.get_fd(&sc->select_worker);
	sensors_event_t sdata;
	int n;
	int i;

	memset(&sdata, 0, sizeof(sdata));

	pthread_mutex_lock(&sc->lock);
	n = read(fd, evbuf, sizeof(evbuf)) / sizeof(evbuf[0]);
	for (i = 0; i < n; i++) {
		event = evbuf + i;
		if (event->type == EV_SYN) {
			if (sc->magnetic.active) {
				sdata.version = sc->magnetic.sensor.version;
				sdata.sensor = sc->magnetic.sensor.handle;
				sdata.type = sc->magnetic.sensor.type;
				sdata.timestamp = get_current_nano_time();
				scale_and_map(&sdata, &sc->magnetic);

				sensors_fifo_put(&sdata);
			}
			if (sc->orientation_raw.active) {
				sdata.version = sc->orientation_raw.sensor.version;
				sdata.sensor = sc->orientation_raw.sensor.handle;
				sdata.type = sc->orientation_raw.sensor.type;
				sdata.timestamp = get_current_nano_time();
				scale_and_map(&sdata, &sc->orientation_raw);

				sensors_fifo_put(&sdata);
			}
			if (sc->orientation.active) {
				sdata.version = sc->orientation.sensor.version;
				sdata.sensor = sc->orientation.sensor.handle;
				sdata.type = sc->orientation.sensor.type;
				sdata.timestamp = get_current_nano_time();
				sdata.orientation.status = sc->orientation_raw.status;

				memcpy(&sc->orientation.data,
				       &sc->orientation_raw.data,
				       sizeof(sc->orientation.data));
				scale_and_map(&sdata, &sc->orientation);

				sensors_fifo_put(&sdata);
			}
			continue;
		}
		if (event->type != EV_ABS)
			continue;
		switch (event->code) {
			case EVENT_CODE_YAW:
				sc->orientation_raw.data[0] = event->value;
				break;
			case EVENT_CODE_PITCH:
				sc->orientation_raw.data[1] = event->value;
				break;
			case EVENT_CODE_ROLL:
				sc->orientation_raw.data[2] = event->value;
				break;
			case EVENT_CODE_ORIENT_STATUS:
				sc->orientation_raw.status =
						event->value & SENSOR_STATE_MASK;
				break;
			case EVENT_CODE_MAGV_X:
				sc->magnetic.data[0] = event->value;
				break;
			case EVENT_CODE_MAGV_Y:
				sc->magnetic.data[1] = event->value;
				break;
			case EVENT_CODE_MAGV_Z:
				sc->magnetic.data[2] = event->value;
				break;
		}
	}
	pthread_mutex_unlock(&sc->lock);

	return NULL;
}

int dummy_acc_delay(int *handle, int64_t ns)
{
	return 0;
}

static struct ak897x_sensor_composition ak897x_compass = {
	.orientation = {
		.sensor = {
			name: AKM_CHIP_NAME" Compass",
			vendor: "Asahi Kasei Corp.",
			version: sizeof(sensors_event_t),
			handle: SENSOR_ORIENTATION_HANDLE,
			type: SENSOR_TYPE_ORIENTATION,
			maxRange: 360,
			resolution: 100,
			power: 0.8,
		},
		.api = {
			init: ak897x_init,
			activate: ak897x_activate,
			set_delay: ak897x_set_delay,
			close: ak897x_close,
		},
		.map_prefix = "ak897xorientation",
		.map = {0, 1, 2},
		.sign = {1, 1, -1},
		.scale = 1.0f/64.0f,
	},
	.orientation_raw = {
		.sensor = {
			name: AKM_CHIP_NAME" Compass Raw",
			vendor: "Asahi Kasei Corp.",
			version: sizeof(sensors_event_t),
			handle: SENSOR_ORIENTATION_RAW_HANDLE,
			type: SENSOR_TYPE_ORIENTATION,
			maxRange: 23040,
			resolution: 100,
			power: 0.8,
		},
		.api = {
			init: ak897x_init,
			activate: ak897x_activate,
			set_delay: ak897x_set_delay,
			close: ak897x_close,
		},
		.map_prefix = "ak897xorientation",
		.map = {0, 1, 2},
		.sign = {1, 1, -1},
		.scale = 1.0f,
	},
	.magnetic = {
		.sensor = {
			name: AKM_CHIP_NAME" Magnetic Field",
			vendor: "Asahi Kasei Corp.",
			version: sizeof(sensors_event_t),
			handle: SENSOR_MAGNETIC_FIELD_HANDLE,
			type: SENSOR_TYPE_MAGNETIC_FIELD,
			maxRange: 2000,
			resolution: 100,
			power: 0.8,
		},
		.api = {
			init: ak897x_init,
			activate: ak897x_activate,
			set_delay: ak897x_set_delay,
			close: ak897x_close,
		},
		.map_prefix = "ak897xmagnetic",
		.map = {0, 1, 2},
		.sign = {1, -1, -1},
		.scale = 1.0f/16.0f,
	},
	.input_name = "compass",
	.acc_handle = -1,

#ifdef ACC_BMA250_INPUT
	.request_acc_delay = bma250_input_request_delay,
#else

#ifdef ACC_BMA150_INPUT
	.request_acc_delay = bma150_input_request_delay,
#else
	.request_acc_delay = dummy_acc_delay,
#endif

#endif
};

list_constructor(ak897x_init_driver);
void ak897x_init_driver()
{
	ak897x_compass_register(&ak897x_compass);
}

