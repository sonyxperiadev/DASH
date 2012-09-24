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

#define LOG_TAG "DASH - lps331ap_input"

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
#include "sensors_sysfs.h"

#define LPS331AP_PRS_DEV_NAME "lps331ap_prs_sysfs"
#define NR_SAMPLES 16
#define ROW_TO_MBAR_SCALE 4096.0

static int lps331ap_input_init(struct sensor_api_t *s);
static int lps331ap_input_activate(struct sensor_api_t *s, int enable);
static int lps331ap_input_set_delay(struct sensor_api_t *s, int64_t ns);
static void lps331ap_input_close(struct sensor_api_t *s);
static void *lps331ap_input_read(void *arg);

struct sensor_desc {
	struct sensors_select_t select_worker;
	struct sensors_sysfs_t sysfs;
	struct sensor_t sensor;
	struct sensor_api_t api;
	long current_data[2];
	int64_t delay;
	long mem[NR_SAMPLES];
	int current_sample;
	int num_samples;
};

static struct sensor_desc lps331ap_pressure_input = {
	.sensor = { name: "LPS331AP Pressure",
		vendor : "STMicroelectronics",
		version : sizeof(sensors_event_t),
		handle : SENSOR_PRESSURE_HANDLE,
		type : SENSOR_TYPE_PRESSURE,
		maxRange : 1100.00, /* hecto pascal */
		resolution : 0.01, /* hecto pascal */
		power : 0.012 /* mA per sample at ultra high resolution */
	},
	.api = { init: lps331ap_input_init,
		activate : lps331ap_input_activate,
		set_delay : lps331ap_input_set_delay,
		close : lps331ap_input_close
	},
};

static int lps331ap_input_init(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd;

	fd = open_input_dev_by_name(LPS331AP_PRS_DEV_NAME, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		ALOGE("%s: failed to open input dev %s, error: %s\n",
			__func__, LPS331AP_PRS_DEV_NAME, strerror(errno));
		return -1;
	}
	close(fd);

	sensors_sysfs_init(&d->sysfs, LPS331AP_PRS_DEV_NAME, SYSFS_TYPE_INPUT_DEV);
	sensors_select_init(&d->select_worker, lps331ap_input_read, s, -1);

	return 0;
}

static int lps331ap_input_activate(struct sensor_api_t *s, int enable)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd = d->select_worker.get_fd(&d->select_worker);

	/* suspend/resume will be handled in kernel-space */
	if (enable && (fd < 0)) {
		fd = open_input_dev_by_name(LPS331AP_PRS_DEV_NAME,
			O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			ALOGE("%s: failed to open input dev %s, error: %s\n",
				__func__, LPS331AP_PRS_DEV_NAME,
				strerror(errno));
			return -1;
		}
		d->current_sample = 0;
		d->num_samples = 0;
		d->current_data[0] = 0;
		d->select_worker.set_fd(&d->select_worker, fd);
		d->select_worker.resume(&d->select_worker);
	} else if (!enable && (fd > 0)) {
		d->select_worker.set_fd(&d->select_worker, -1);
		d->select_worker.suspend(&d->select_worker);
	}
	return 0;
}

static int lps331ap_input_set_delay(struct sensor_api_t *s, int64_t ns)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd = d->select_worker.get_fd(&d->select_worker);
	unsigned int ms = ns/(1000*1000);

	d->delay = ns;

	return d->sysfs.write_int(&d->sysfs, "device/poll_period_ms", ms);
}

static void lps331ap_input_close(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	d->select_worker.destroy(&d->select_worker);
}

static void *lps331ap_input_read(void *arg)
{
	struct sensor_api_t *s = arg;
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	struct input_event event[3];
	int bytes, i, fd;
	sensors_event_t data;
	long pressure;

	fd = d->select_worker.get_fd(&d->select_worker);

	bytes = read(fd, event, sizeof(event));

	if (bytes < 0) {
		ALOGE("%s: read failed, error: %d\n", __func__, bytes);
		return NULL;
	}

	for (i = 0; i < (bytes / (int)sizeof(struct input_event)); i++) {
		switch (event[i].type) {
		case EV_ABS:
			switch (event[i].code) {
			case ABS_PRESSURE:
				/* convert to hPa (millibar) */
				if (d->num_samples == NR_SAMPLES)
					d->current_data[0] -=
						d->mem[d->current_sample];
				else
					d->num_samples++;

				d->mem[d->current_sample++] = event[i].value;
				d->current_data[0] += event[i].value;
				d->current_sample %= NR_SAMPLES;
				break;
			default:
				break;
			}
			break;

		case EV_SYN:
			/* report pressure */
			pressure = d->current_data[0] / d->num_samples;
			data.pressure = pressure / ROW_TO_MBAR_SCALE;
			ALOGD_IF(DEBUG_VERBOSE, "lps331ap: %f", data.pressure);
			data.version = lps331ap_pressure_input.sensor.version;
			data.sensor = lps331ap_pressure_input.sensor.handle;
			data.type = lps331ap_pressure_input.sensor.type;
			data.timestamp = get_current_nano_time();
			sensors_fifo_put(&data);
			break;

		default:
			ALOGE("%s: unknown event type 0x%X\n",
				__func__, event[i].type);
			break;
		}
	}

	return NULL;
}

list_constructor(lps331ap_input_init_driver);
void lps331ap_input_init_driver()
{
	(void)sensors_list_register(&lps331ap_pressure_input.sensor,
					&lps331ap_pressure_input.api);
}
