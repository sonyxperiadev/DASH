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
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_select.h"
#include "sensor_util.h"
#include "sensors_id.h"

#define PROXIMITY_DEV_NAME "apds9702"
#define THRESH_DEFAULT   5
#define BURST_DEFAULT    1
#define BURST_THRESHOLD_MIN 2
#define PATH_SIZE      100

static int apds9700_init(struct sensor_api_t *s);
static int apds9700_activate(struct sensor_api_t *s, int enable);
static int apds9700_set_delay(struct sensor_api_t *s, int64_t ns);
static void apds9700_close(struct sensor_api_t *s);
static void *apds9700_read(void *arg);

struct sensor_desc {
	struct sensors_select_t select_worker;
	struct sensor_t sensor;
	struct sensor_api_t api;

	float distance;
	int64_t delay;

	int th_det;
	int th_not_det;
	char th_path[PATH_SIZE];

	int burst_det;
	int burst_not_det;
	char burst_path[PATH_SIZE];
};

static struct sensor_desc apds970x = {
	.sensor = {
		.name = PROXIMITY_SENSOR_NAME,
		.vendor = "Sony",
		.version = sizeof(sensors_event_t),
		.handle = SENSOR_PROXIMITY_HANDLE,
		.type = SENSOR_TYPE_PROXIMITY,
		.maxRange = 1.0,
		.resolution = 1.0,
		.power = 2
	},
	.api = {
		.init = apds9700_init,
		.activate = apds9700_activate,
		.set_delay = apds9700_set_delay,
		.close = apds9700_close
	}
};

static void apds9700_init_threshold_members(struct sensor_desc *d, int fd_input)
{
	char buf[PATH_SIZE];
	int fd_th;

	if (ioctl(fd_input, EVIOCGPHYS(sizeof(buf)), buf) < 0 ||
	    snprintf(d->th_path, sizeof(d->th_path), "%s/threshold", buf) < 0) {
		ALOGE("%s: getting device physical path failed (%s)\n",
		     __func__, strerror(errno));
		goto exit_with_defaults;
	}

	fd_th = open(d->th_path, O_RDONLY);
	if (fd_th < 0) {
		ALOGE("%s: opening %s for read failed (%s)\n",
		     __func__, d->th_path, strerror(errno));
		goto exit_with_defaults;
	}

	if (read(fd_th, buf, sizeof(buf)) < 0) {
		close(fd_th);
		goto exit_with_defaults;
	}

	d->th_not_det = atoi(buf);
	d->th_det = d->th_not_det - 1;
	if (d->th_det < 0) {
		d->th_not_det = 1;
		d->th_det = 0;
	}
	close(fd_th);
	return;

exit_with_defaults:
	d->th_path[0] = 0;
	d->th_not_det = THRESH_DEFAULT;
	d->th_det = THRESH_DEFAULT - 1;
}

static void apds9700_init_burst_members(struct sensor_desc *d, int fd_input)
{
	char buf[PATH_SIZE];
	int fd_burst;
	int requested_burst;

	if (ioctl(fd_input, EVIOCGPHYS(sizeof(buf)), buf) < 0 ||
	    snprintf(d->burst_path, sizeof(d->burst_path),
		"%s/nburst", buf) < 0) {
		ALOGE("%s: getting device physical path failed (%s)\n",
		     __func__, strerror(errno));
		goto exit_with_defaults;
	}

	fd_burst = open(d->burst_path, O_RDONLY);
	if (fd_burst < 0) {
		ALOGE("%s: opening %s for read failed (%s)\n",
		     __func__, d->burst_path, strerror(errno));
		ALOGE("burst path error read error\n");
		goto exit_with_defaults;
	}

	if (read(fd_burst, buf, sizeof(buf)) < 0) {
		close(fd_burst);
		goto exit_with_defaults;
	}

	requested_burst = atoi(buf);

	if (0 > requested_burst || 31 < requested_burst)
		requested_burst = BURST_DEFAULT;

	if (15 < requested_burst) {
		d->burst_not_det = 17;
		d->burst_det = requested_burst;
	} else {
		d->burst_det = requested_burst;
		d->burst_not_det = requested_burst;
	}

	close(fd_burst);
	return;

exit_with_defaults:
	ALOGE("Default burst path error\n");
	d->burst_path[0] = 0;
	d->burst_not_det = BURST_DEFAULT;
	d->burst_det = BURST_DEFAULT - 1;
}

static void apds9700_change_threshold(struct sensor_desc *d)
{
	int fd_th;

	if (d->th_path[0] == 0)
		return;

	fd_th = open(d->th_path, O_WRONLY);
	if (fd_th < 0) {
		ALOGE("%s: opening %s for write failed (%s)\n",
		     __func__, d->th_path, strerror(errno));
	} else {
		int th_new = (d->distance == 0.0) ? d->th_det : d->th_not_det;
		char th_new_c[10];
		unsigned int cnt = snprintf(th_new_c, sizeof(th_new_c), "%d",
					th_new);

		if (cnt > 0 && cnt < sizeof(th_new_c))
			write(fd_th, th_new_c, cnt + 1);
		close(fd_th);
	}
}

static void apds9700_change_burst(struct sensor_desc *d)
{
	int fd_burst;

	if (d->burst_path[0] == 0)
		return;

	fd_burst = open(d->burst_path, O_WRONLY);
	if (fd_burst < 0) {
		ALOGE("%s: opening %s for write failed (%s)\n",
		     __func__, d->burst_path, strerror(errno));
	} else {
		int burst_new = (d->distance == 0.0)
			? d->burst_det : d->burst_not_det;

		char burst_new_c[10];
		unsigned int cnt = snprintf(burst_new_c,
			sizeof(burst_new_c), "%d", burst_new);

		if (cnt > 0 && cnt < sizeof(burst_new_c))
			write(fd_burst, burst_new_c, cnt + 1);

		close(fd_burst);
	}
}

static int apds9700_init(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd;

	/* check for availablity */
	fd = open_input_dev_by_name(PROXIMITY_DEV_NAME, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		ALOGE("%s: unable to find %s input device!\n", __func__,
			PROXIMITY_DEV_NAME);
		return fd;
	}
	apds9700_init_threshold_members(d, fd);
	apds9700_init_burst_members(d, fd);
	close(fd);

	sensors_select_init(&d->select_worker, apds9700_read, s, -1);
	return 0;
}

static int apds9700_activate(struct sensor_api_t *s, int enable)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd = d->select_worker.get_fd(&d->select_worker);

	if (enable && (fd < 0)) {
		fd = open_input_dev_by_name(PROXIMITY_DEV_NAME,
			O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			ALOGE("%s: failed to open input dev %s\n", __func__,
				PROXIMITY_DEV_NAME);
			return fd;
		}
		d->select_worker.set_fd(&d->select_worker, fd);
		d->select_worker.resume(&d->select_worker);
	} else if (!enable && (fd > 0)) {
		d->select_worker.set_fd(&d->select_worker, -1);
		d->select_worker.suspend(&d->select_worker);
	}

	return 0;
}

static int apds9700_set_delay(struct sensor_api_t *s, int64_t ns)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	d->delay = ns;
	d->select_worker.set_delay(&d->select_worker, ns);

	return 0;
}

static void apds9700_close(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	d->select_worker.destroy(&d->select_worker);
}

static void *apds9700_read(void *arg)
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
			if (event.code == ABS_DISTANCE) {
				d->distance = event.value ? 1.0 : 0.0;
				if (d->burst_not_det != d->burst_det)
					apds9700_change_burst(d);
				apds9700_change_threshold(d);
			}

			goto exit;

		case EV_SYN:
			memset(&data, 0, sizeof(data));

			data.distance = d->distance;

			data.version = apds970x.sensor.version;
			data.sensor = apds970x.sensor.handle;
			data.type = apds970x.sensor.type;
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

list_constructor(apds9700_init_driver);
void apds9700_init_driver()
{
	(void)sensors_list_register(&apds970x.sensor, &apds970x.api);
}
