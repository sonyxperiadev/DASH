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

#define LOG_TAG "DASH - proximity"

#include <stdlib.h>
#include <string.h>
#include "sensors_log.h"
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_select.h"
#include "sensor_util.h"
#include "sensors_id.h"

#define PROXIMITY_DEV_NAME "gp2ap002a00f"

static int sharp_init(struct sensor_api_t *s);
static int sharp_activate(struct sensor_api_t *s, int enable);
static int sharp_set_delay(struct sensor_api_t *s, int64_t ns);
static void sharp_close(struct sensor_api_t *s);
static void *sharp_read(void *arg);

struct sensor_desc {
	struct sensors_select_t select_worker;
	struct sensor_t sensor;
	struct sensor_api_t api;

	float distance;
	uint64_t delay;
};

static struct sensor_desc sharp_gp2 = {
	.sensor = {
		name: "GP2 Proximity",
		vendor: "Sharp",
		version: sizeof(sensors_event_t),
		handle: SENSOR_PROXIMITY_HANDLE,
		type: SENSOR_TYPE_PROXIMITY,
		maxRange: 1.0,
		resolution: 1,
		power: 20
	},
	.api = {
		init: sharp_init,
		activate: sharp_activate,
		set_delay: sharp_set_delay,
		close: sharp_close
	},
};

static int sharp_init(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd;

	fd = open_input_dev_by_name(PROXIMITY_DEV_NAME, O_RDONLY);
	if (fd < 0) {
		ALOGE(LOG_TAG" %s: open device failed!", __func__);
		return -1;
	}
	close(fd);

	sensors_select_init(&d->select_worker, sharp_read, s, -1);
	return 0;
}

static int sharp_activate(struct sensor_api_t *s, int enable)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd = d->select_worker.get_fd(&d->select_worker);

	if (enable && (fd < 0)) {
		fd = open_input_dev_by_name(PROXIMITY_DEV_NAME,
			O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			ALOGE("%s: failed to open input dev %s\n", __func__,
				PROXIMITY_DEV_NAME);
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

static int sharp_set_delay(struct sensor_api_t *s, int64_t ns)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	d->delay = ns;
	d->select_worker.set_delay(&d->select_worker, ns);

	return 0;
}

static void sharp_close(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	d->select_worker.destroy(&d->select_worker);
}

static void *sharp_read(void *arg)
{
	struct sensor_api_t *s = arg;
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	struct input_event event;
	sensors_event_t data;
	int ret;
	int fd = d->select_worker.get_fd(&d->select_worker);

	while ((ret = read(fd, &event, sizeof(event))) > 0) {
		switch (event.type) {
		case EV_ABS:
			switch (event.code) {
			case ABS_DISTANCE:
				d->distance = event.value ? 1.0 : 0.0;
				break;
			default:
				goto exit;
			}
			break;

		case EV_SYN:
			memset(&data, 0, sizeof(data));

			data.distance = d->distance;

			data.version = sharp_gp2.sensor.version;
			data.sensor = sharp_gp2.sensor.handle;
			data.type = sharp_gp2.sensor.type;
			data.timestamp = get_current_nano_time();

			sensors_fifo_put(&data);

			/* sleep for delay time */
			sensors_nsleep(d->delay);

			goto exit;

		default:
			goto exit;
		}
	}

exit:
	return NULL;
}

list_constructor(sharp_init_driver);
void sharp_init_driver()
{
	(void)sensors_list_register(&sharp_gp2.sensor, &sharp_gp2.api);
}

