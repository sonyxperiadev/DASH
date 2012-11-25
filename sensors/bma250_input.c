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

#define LOG_TAG "DASH - bma250_input"

#include <string.h>
#include "sensors_log.h"
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_select.h"
#include "sensor_util.h"
#include "sensors_id.h"
#include "sensors_config.h"
#include "sensors_sysfs.h"

#define BMA250_INPUT_NAME "bma250"

#define VALID_HANDLE(h) ((h) > CLIENT_ANDROID && (h) < MAX_CLIENTS)

enum bma250_clients {
	CLIENT_ANDROID = 0,
	MAX_CLIENTS = 2,
	CLIENT_DELAY_UNUSED = 0,
};

static int bma250_input_init(struct sensor_api_t *s);
static int bma250_input_activate(struct sensor_api_t *s, int enable);
static int bma250_input_fw_delay(struct sensor_api_t *s, int64_t ns);
static void bma250_input_close(struct sensor_api_t *s);
static void *bma250_input_read(void *arg);

struct sensor_desc {
	struct sensors_select_t select_worker;
	struct sensors_sysfs_t sysfs;
	struct sensor_t sensor;
	struct sensor_api_t api;

	int input_fd;
	float current_data[3];
	int64_t delay;

	/* config options */
	int axis_x;
	int axis_y;
	int axis_z;

	int neg_x;
	int neg_y;
	int neg_z;

	int64_t  delay_requests[MAX_CLIENTS];
};

static struct sensor_desc bma250_input = {
	.sensor = {
		name: "BMA250 accelerometer",
		vendor: "Bosch Sensortec GmbH",
		version: sizeof(sensors_event_t),
		handle: SENSOR_ACCELEROMETER_HANDLE,
		type: SENSOR_TYPE_ACCELEROMETER,
		maxRange: 156.96, /* max +/-16G */
		resolution: 20,
		power: 0.003,/* sleep 50ms */
		minDelay: 5000
	},
	.api = {
		init: bma250_input_init,
		activate: bma250_input_activate,
		set_delay: bma250_input_fw_delay,
		close: bma250_input_close
	},
	.input_fd = -1,
	.axis_x = 0,
	.axis_y = 1,
	.axis_z = 2,
	.neg_x = 1,
	.neg_y = 1,
	.neg_z = 0
};

static inline float ev2grav(int evv)
{
	/* bma250 driver sensitivity is 256 lsb/g for all g-ranges. */
	const float earth_g = 9.81;
	const float lsb_per_g = 256;
	float gravity_in_g = (float)evv / lsb_per_g;
	return gravity_in_g * earth_g;
}

static void bma250_input_read_config(struct sensor_desc *d)
{
	int conf_axis_x, conf_axis_y, conf_axis_z;
	int conf_neg_x, conf_neg_y, conf_neg_z;
	struct {
		char *key;
		int min;
		int max;
		int *store;
	} conf_values[] = {
		{ "axis_x", 0, 2, &conf_axis_x },
		{ "axis_y", 0, 2, &conf_axis_y },
		{ "axis_z", 0, 2, &conf_axis_z },

		{ "neg_x", 0, 1, &conf_neg_x },
		{ "neg_y", 0, 1, &conf_neg_y },
		{ "neg_z", 0, 1, &conf_neg_z },
	};
	unsigned int i;

	if (!sensors_have_config_file()) {
		ALOGI("%s: No config file found: using default config.",
		     __func__);
		return;
	}

	for (i = 0; i < (sizeof(conf_values)/sizeof(conf_values[0])); i++) {
		int value;
		if (sensors_config_get_key("bma250input", conf_values[i].key,
					   TYPE_INT,
					   (void*)&value,
					   sizeof(value)) < 0) {
			ALOGE("%s: failed to read %s", __func__, conf_values[i].key);
			return;
		}

		if ((value < conf_values[i].min) || (value > conf_values[i].max)) {
			ALOGE("%s: %s value out of bounds: %d\n", __func__,
			     conf_values[i].key, value);
			return;
		}

		*conf_values[i].store = value;
	}

	d->axis_x = conf_axis_x;
	d->axis_y = conf_axis_y;
	d->axis_z = conf_axis_z;

	d->neg_x = conf_neg_x;
	d->neg_y = conf_neg_y;
	d->neg_z = conf_neg_z;
}

static int bma250_input_init(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd;
	bma250_input_read_config(d);

	fd = open_input_dev_by_name(BMA250_INPUT_NAME, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		ALOGE("%s: failed to open input dev %s, error: %s\n",
			__func__, BMA250_INPUT_NAME, strerror(errno));
		return -1;
	}
	close(fd);

	sensors_sysfs_init(&d->sysfs, BMA250_INPUT_NAME, SYSFS_TYPE_INPUT_DEV);
	sensors_select_init(&d->select_worker, bma250_input_read, s, -1);

	return 0;
}

static int bma250_input_activate(struct sensor_api_t *s, int enable)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd = d->select_worker.get_fd(&d->select_worker);

	/* suspend/resume will be handled in kernel-space */
	if (enable && (fd < 0)) {
		fd = open_input_dev_by_name(BMA250_INPUT_NAME,
			O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			ALOGE("%s: failed to open input dev %s, error: %s\n",
				__func__, BMA250_INPUT_NAME, strerror(errno));
			return -1;
		}
		d->select_worker.set_fd(&d->select_worker, fd);
		d->select_worker.resume(&d->select_worker);
	} else if (!enable && (fd > 0)) {
		d->select_worker.set_fd(&d->select_worker, -1);
		d->select_worker.suspend(&d->select_worker);
	}
	return 0;
}

static int bma250_input_set_delay(struct sensor_api_t *s, int64_t ns)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd = d->select_worker.get_fd(&d->select_worker);
	unsigned int ms = ns/(1000*1000);
	int ret;

	d->delay = ns;
	d->select_worker.set_delay(&d->select_worker, ns);

	/* rate */
	ret = d->sysfs.write_int(&d->sysfs, "bma250_rate", ms);
	if (ret < 0) {
		ALOGE("%s: sysfs.write_int failed!\n", __func__);
		return ret;
	}

	/* range */
	ret = d->sysfs.write_int(&d->sysfs, "bma250_range", 2);
	if (ret < 0) {
		ALOGE("%s: sysfs.write_int failed!\n", __func__);
		return ret;
	}

	/* resolution */
	ret = d->sysfs.write_int(&d->sysfs, "bma250_resolution",
		(ms > 50) ? 0 : 1);
	if (ret < 0)
		ALOGE("%s: sysfs.write_int failed!\n", __func__);

	return ret;
}

static void bma250_input_close(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	d->select_worker.destroy(&d->select_worker);
}

static void *bma250_input_read(void *arg)
{
	struct sensor_api_t *s = arg;
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	struct input_event event;
	int fd = d->select_worker.get_fd(&d->select_worker);
	sensors_event_t data;

	while (read(fd, &event, sizeof(event)) > 0) {
		switch (event.type) {
		case EV_ABS:
			switch (event.code) {
			case ABS_X:
				d->current_data[0] = ev2grav(event.value);
				break;

			case ABS_Y:
				d->current_data[1] = ev2grav(event.value);
				break;

			case ABS_Z:
				d->current_data[2] = ev2grav(event.value);
				break;

			case ABS_MISC:
				/* temperature, 0.5C/lsb */
				break;

			default:
				ALOGE("%s: unknown event code 0x%X\n",
					__func__, event.code);
				goto exit;
			}
			break;

		case EV_SYN:
			data.acceleration.x = (d->neg_x ? -d->current_data[d->axis_x] :
						d->current_data[d->axis_x]);
			data.acceleration.y = (d->neg_y ? -d->current_data[d->axis_y] :
						d->current_data[d->axis_y]);
			data.acceleration.z = (d->neg_z ? -d->current_data[d->axis_z] :
						d->current_data[d->axis_z]);
			data.acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;

			data.sensor = bma250_input.sensor.handle;
			data.type = bma250_input.sensor.type;
			data.version = bma250_input.sensor.version;
			data.timestamp = get_current_nano_time();

			sensors_fifo_put(&data);
			goto exit;

		default:
			ALOGE("%s: unknown event type 0x%X\n",
				__func__, event.type);
			goto exit;
		}
	}

exit:
	return NULL;
}


static int bma250_input_config_delay(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int i;
	int64_t usec = d->delay_requests[0];
	int64_t x;

	for (i = 0; i < MAX_CLIENTS; i++) {
		x = d->delay_requests[i];
		if (x > CLIENT_DELAY_UNUSED && x < usec)
			usec = x;
	}

	if (usec < d->sensor.minDelay) {
		usec = d->sensor.minDelay;
	}

	bma250_input_set_delay(s, usec);

	return 0;
}

static int bma250_input_fw_delay(struct sensor_api_t *s, int64_t ns)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	d->delay_requests[CLIENT_ANDROID] = ns;
	return bma250_input_config_delay(s);
}

int bma250_input_request_delay(int *handle, int64_t ns)
{
	struct sensor_desc *d = &bma250_input;
	int err;
	int h = *handle;
	int i;

	if (!ns && VALID_HANDLE(h)) {
		/* Need to release handle */
		*handle = -1;
		goto found;
	}
	if (!ns) /* error situation */
		return -1;
	if (!VALID_HANDLE(h)) {
		/* Need to allocate new handle */
		for (i = CLIENT_ANDROID + 1; i < MAX_CLIENTS; i++) {
			if (d->delay_requests[i] == CLIENT_DELAY_UNUSED) {
				*handle = h = i;
				goto found;
			}
		}
	}

found:
	d->delay_requests[h] = ns;
	err = bma250_input_config_delay(&bma250_input.api);
	if (err) {
		/* delay not set - deallocate handle */
		d->delay_requests[h] = CLIENT_DELAY_UNUSED;
		*handle = -1;
	}
	return err;
}

list_constructor(bma250_input_init_driver);
void bma250_input_init_driver()
{
	(void)sensors_list_register(&bma250_input.sensor, &bma250_input.api);
}
