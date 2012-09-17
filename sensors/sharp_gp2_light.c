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

#define LOG_TAG "DASH - gp2_light"

#include <stdlib.h>
#include <string.h>
#include "sensors_log.h"
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <linux/input.h>
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_select.h"
#include "sensor_util.h"
#include "sensors_sysfs.h"
#include "sensors_id.h"

#define LIGHT_DEV_NAME "lightsensor-level"

static int gp2_light_init(struct sensor_api_t *s);
static int gp2_light_activate(struct sensor_api_t *s, int enable);
static int gp2_light_set_delay(struct sensor_api_t *s, int64_t ns);
static void gp2_light_close(struct sensor_api_t *s);
static void *gp2_light_read(void *arg);

struct sensor_desc {
	struct sensors_select_t select_worker;
	struct sensors_sysfs_t sysfs;
	struct sensor_t sensor;
	struct sensor_api_t api;

	float light;
	uint64_t delay;
};

static struct sensor_desc gp2_light_gp2 = {
	.sensor = {
		name: "DASH GP2 Light",
		vendor: "Sharp",
		version: sizeof(sensors_event_t),
		handle: SENSOR_LIGHT_HANDLE,
		type: SENSOR_TYPE_LIGHT,
		maxRange: 646239.5,
		resolution: 1.0f,
		power: 0.75f,
		minDelay: 0
	},
	.api = {
		init: gp2_light_init,
		activate: gp2_light_activate,
		set_delay: gp2_light_set_delay,
		close: gp2_light_close
	},
};

static int gp2_light_init(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd;

	fd = open_input_dev_by_name(LIGHT_DEV_NAME, O_RDONLY);
	if (fd < 0) {
		ALOGE(LOG_TAG" %s: open device failed!", __func__);
		return -1;
	}
	close(fd);

	sensors_sysfs_init(&d->sysfs, LIGHT_DEV_NAME, SYSFS_TYPE_INPUT_DEV);
	sensors_select_init(&d->select_worker, gp2_light_read, s, -1);
	return 0;
}

static int gp2_light_activate(struct sensor_api_t *s, int enable)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd = d->select_worker.get_fd(&d->select_worker);

	if (enable && (fd < 0)) {
		fd = open_input_dev_by_name(LIGHT_DEV_NAME,
			O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			ALOGE("%s: failed to open input dev %s\n", __func__,
				LIGHT_DEV_NAME);
			return -1;
		}
		d->select_worker.set_fd(&d->select_worker, fd);
		d->select_worker.resume(&d->select_worker);
	} else if (!enable && (fd > 0)) {
		d->select_worker.set_fd(&d->select_worker, -1);
		d->select_worker.suspend(&d->select_worker);
	}

	return d->sysfs.write_int(&d->sysfs, "enable", enable);
}

static int gp2_light_set_delay(struct sensor_api_t *s, int64_t ns)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	d->delay = ns;
	d->select_worker.set_delay(&d->select_worker, ns);

	return 0;
}

static void gp2_light_close(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	d->select_worker.destroy(&d->select_worker);
}

static void *gp2_light_read(void *arg)
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
			case ABS_MISC:
				d->light = powf(10, event.value * (125.0f / 1023.0f / 24.0f)) * 4;
				break;
			default:
				goto exit;
			}
			break;

		case EV_SYN:
			memset(&data, 0, sizeof(data));

			data.light = d->light;
			data.version = gp2_light_gp2.sensor.version;
			data.sensor = gp2_light_gp2.sensor.handle;
			data.type = gp2_light_gp2.sensor.type;
			data.timestamp = get_current_nano_time();

			sensors_fifo_put(&data);
			goto exit;

		default:
			goto exit;
		}
	}

exit:
	return NULL;
}

list_constructor(gp2_light_init_driver);
void gp2_light_init_driver()
{
	(void)sensors_list_register(&gp2_light_gp2.sensor, &gp2_light_gp2.api);
}

