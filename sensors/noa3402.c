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

#define LOG_TAG "DASH - proximity_noa3402"

#include <stdlib.h>
#include <string.h>
#include "sensors_log.h"
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_select.h"
#include "sensor_util.h"
#include "sensors_id.h"

#define NOA3402_NAME "noa3402"

static int noa3402_init(struct sensor_api_t *s);
static int noa3402_activate(struct sensor_api_t *s, int enable);
static int noa3402_set_delay(struct sensor_api_t *s, int64_t ns);
static void noa3402_close(struct sensor_api_t *s);
static void *noa3402_read(void *arg);

struct sensor_desc {
	struct sensors_select_t select_worker;
	struct sensor_t sensor;
	struct sensor_api_t api;
	float distance;
};

static struct sensor_desc noa3402 = {
	.sensor = {
		.name = "NOA3402 Proximity",
		.vendor = "Sony",
		.version = sizeof(sensors_event_t),
		.handle = SENSOR_PROXIMITY_HANDLE,
		.type = SENSOR_TYPE_PROXIMITY,
		.maxRange = 1.0,
		.resolution = 1.0,
		.power = 20
	},
	.api = {
		.init = noa3402_init,
		.activate = noa3402_activate,
		.set_delay = noa3402_set_delay,
		.close = noa3402_close
	}
};

static void noa3402_report_distance(float distance)
{
	sensors_event_t data;

	memset(&data, 0, sizeof(data));
	data.distance = distance;
	data.version = noa3402.sensor.version;
	data.sensor = noa3402.sensor.handle;
	data.type = noa3402.sensor.type;
	data.timestamp = get_current_nano_time();
	sensors_fifo_put(&data);
}

static int noa3402_get_current_distance(float *current_distance)
{
	int ret = 0;
	int fd;
	char result;

	fd = open(PROXIMITY_PATH, O_RDONLY);
	if (fd < 0) {
		ALOGE("%s: Failed to open %s\n", __func__, PROXIMITY_PATH);
		ret = -ENODEV;
		goto exit;
	}
	if (read(fd, &result, sizeof(result)) == sizeof(result)) {
		*current_distance = strncmp(&result, "0", 1) ? 1.0 : 0.0;
	} else {
		ALOGE("%s: Failed to read initial proximity state\n", __func__);
		ret = -ENODEV;
	}
	close(fd);
exit:
	return ret;
}

static int noa3402_init(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd;

	/* check for availablity */
	fd = open_input_dev_by_name(NOA3402_NAME, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		ALOGE("%s: unable to find %s input device!\n", __func__,
			NOA3402_NAME);
		return fd;
	}
	close(fd);

	sensors_select_init(&d->select_worker, noa3402_read, s, -1);
	return 0;
}

static int noa3402_activate(struct sensor_api_t *s, int enable)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd = d->select_worker.get_fd(&d->select_worker);
	float current_distance;

	if (enable && (fd < 0)) {
		fd = open_input_dev_by_name(NOA3402_NAME,
			O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			ALOGE("%s: failed to open input dev %s\n", __func__,
				NOA3402_NAME);
			return fd;
		}
		d->select_worker.set_fd(&d->select_worker, fd);
		d->select_worker.resume(&d->select_worker);
		if (!noa3402_get_current_distance(&current_distance))
			noa3402_report_distance(current_distance);
	} else if (!enable && (fd > 0)) {
		d->select_worker.set_fd(&d->select_worker, -1);
		d->select_worker.suspend(&d->select_worker);
	}

	return 0;
}

static int noa3402_set_delay(struct sensor_api_t *s, int64_t ns)
{
	/* N/A for this chip, but required by sensor HAL */
	return 0;
}

static void noa3402_close(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	d->select_worker.destroy(&d->select_worker);
}

static void *noa3402_read(void *arg)
{
	struct sensor_api_t *s = arg;
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int bytes, i;
	struct input_event event[2];
	int fd = d->select_worker.get_fd(&d->select_worker);

	bytes = read(fd, event, sizeof(event));
	if (bytes < 0) {
		ALOGE("%s: read failed, error: %d\n", __func__, bytes);
		return NULL;
	}

	for (i = 0; i < (bytes / (int)sizeof(struct input_event)); i++) {
		switch (event[i].type) {
		case EV_ABS:
			if (event[i].code == ABS_DISTANCE)
				d->distance = event[i].value ? 1.0 : 0.0;
			else
				ALOGE("%s: unknown event code 0x%X\n",
						__func__, event[i].code);
			break;
		case EV_SYN:
			noa3402_report_distance(d->distance);
			break;
		default:
			ALOGE("%s: unknown event type 0x%X\n",
						__func__, event[i].type);
			break;
		}
	}
	return NULL;
}

list_constructor(noa3402_init_driver);
void noa3402_init_driver()
{
	(void)sensors_list_register(&noa3402.sensor, &noa3402.api);
}
