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

#define LOG_TAG "DASH - bmp180_input"

#include <string.h>
#include <cutils/log.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_select.h"
#include "sensor_util.h"
#include "sensors_id.h"

#define BMP180_INPUT_NAME "bmp180"
#define NR_MAX_SIZE 4

static int bmp180_input_init(struct sensor_api_t *s);
static int bmp180_input_activate(struct sensor_api_t *s, int enable);
static int bmp180_input_set_delay(struct sensor_api_t *s, int64_t ns);
static void bmp180_input_close(struct sensor_api_t *s);
static void *bmp180_input_read(void *arg);

struct sensor_desc {
	struct sensors_select_t select_worker;
	struct sensor_t sensor;
	struct sensor_api_t api;
	float current_data[2];
	int64_t delay;
	char nr[NR_MAX_SIZE];
};

static struct sensor_desc bmp180_pressure_input = {
	.sensor = {
		name: "BMP180 Pressure",
		vendor: "Bosch Sensortec GmbH",
		version: sizeof(sensors_event_t),
		handle: SENSOR_PRESSURE_HANDLE,
		type: SENSOR_TYPE_PRESSURE,
		maxRange: 1100.00, /* hecto pascal */
		resolution: 0.01, /* hecto pascal */
		power: 0.012 /* mA per sample at ultra high resolution */
	},
	.api = {
		init: bmp180_input_init,
		activate: bmp180_input_activate,
		set_delay: bmp180_input_set_delay,
		close: bmp180_input_close
	},
};

static int bmp180_input_init(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd;

	fd = open_input_dev_by_name_store_nr(BMP180_INPUT_NAME,
		O_RDONLY | O_NONBLOCK, bmp180_pressure_input.nr, NR_MAX_SIZE);
	if (fd < 0) {
		LOGE("%s: failed to open input dev %s, error: %s\n",
			__func__, BMP180_INPUT_NAME, strerror(errno));
		return -1;
	}
	close(fd);
	sensors_select_init(&d->select_worker, bmp180_input_read, s, -1);
	return 0;
}

static int bmp180_input_activate(struct sensor_api_t *s, int enable)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd = d->select_worker.get_fd(&d->select_worker);

	/* suspend/resume will be handled in kernel-space */
	if (enable && (fd < 0)) {
		fd = open_input_dev_by_name_store_nr(BMP180_INPUT_NAME,
			O_RDONLY | O_NONBLOCK, bmp180_pressure_input.nr,
			NR_MAX_SIZE);
		if (fd < 0) {
			LOGE("%s: failed to open input dev %s, error: %s\n",
				__func__, BMP180_INPUT_NAME, strerror(errno));
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

static int bmp180_input_set_delay(struct sensor_api_t *s, int64_t ns)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd = d->select_worker.get_fd(&d->select_worker);
	char sysfs_path[64];
	const char *path = "sys/class/input/input";
	char buf[16];
	int sysfs_fd, count, len;
	unsigned int ms = ns/(1000*1000);

	d->delay = ns;
	d->select_worker.set_delay(&d->select_worker, ns);

	/* rate */
	count = snprintf(sysfs_path, sizeof(sysfs_path), "%s%s/bmp180_rate",
					path, bmp180_pressure_input.nr);
	if ((count < 0) || (count >= (int)sizeof(sysfs_path)))
		goto snprintf_error;

	sysfs_fd = open(sysfs_path, O_RDWR);
	if (sysfs_fd < 0)
		goto open_error;

	count = snprintf(buf, sizeof(buf), "%d\n", ms);
	if ((count < 0) || (count >= (int)sizeof(buf)))
		goto snprintf_error;

	len = write(sysfs_fd, buf, count + 1);
	close(sysfs_fd);
	if (len < 0)
		goto write_error;

	return 0;

snprintf_error:
	LOGE("%s: snprintf failed, invalid count %d\n", __func__, count);
	return -1;

open_error:
	LOGE("%s: open %s failed, error: %s\n", __func__, sysfs_path,
		strerror(errno));
	return sysfs_fd;

write_error:
	LOGE("%s: write %s failed, error: %d\n", __func__, sysfs_path, len);
	return len;
}

static void bmp180_input_close(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	d->select_worker.destroy(&d->select_worker);
}

static void *bmp180_input_read(void *arg)
{
	struct sensor_api_t *s = arg;
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	struct input_event event[3];
	int bytes, i, fd;
	sensors_event_t data;

	fd = d->select_worker.get_fd(&d->select_worker);

	bytes = read(fd, event, sizeof(event));

	if (bytes < 0) {
		LOGE("%s: read failed, error: %d\n", __func__, bytes);
		goto exit;
	}

	for (i = 0; i < (bytes / (int)sizeof(struct input_event)); i++) {
		switch (event[i].type) {
		case EV_ABS:
			switch (event[i].code) {
			case ABS_PRESSURE:
				/* convert to hPa (millibar) */
				d->current_data[0] = (float)event[i].value/100;
				break;

			case ABS_MISC:
				/* convert to degree celsius */
				d->current_data[1] = (float)event[i].value/10;
				break;

			default:
				LOGE("%s: unknown event code 0x%X\n",
					__func__, event[i].code);
				break;
			}
			break;

		case EV_SYN:
			/* report pressure */
			data.pressure = d->current_data[0];
			data.version = bmp180_pressure_input.sensor.version;
			data.sensor = bmp180_pressure_input.sensor.handle;
			data.type = bmp180_pressure_input.sensor.type;
			data.timestamp = get_current_nano_time();
			sensors_fifo_put(&data);
			break;

		default:
			LOGE("%s: unknown event type 0x%X\n",
				__func__, event[i].type);
			break;
		}
	}
exit:
	return NULL;
}

list_constructor(bmp180_input_init_driver);
void bmp180_input_init_driver()
{
	(void)sensors_list_register(&bmp180_pressure_input.sensor,
					&bmp180_pressure_input.api);
}
