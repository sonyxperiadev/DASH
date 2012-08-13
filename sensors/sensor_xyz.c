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

#define LOG_TAG "DASH - xyz"

#include "sensor_xyz.h"

#define NS_TO_MS 1000000
#define MAX_EVENTS 4 /* X, Y, Z, SYN */

struct config_record {
	int max;
	int min;
	int size;
	int *store;
};

static int config_read_axis(char *prefix, char *key, struct config_record *cr)
{
	int rc = -1;
	int tmp[NUM_AXIS];

	if (!sensors_config_get_key(prefix, key, TYPE_ARRAY_INT, tmp,
					cr->size)) {
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

static void config_read_sensor_map(struct sensor_desc *d)
{
	struct config_record rec;

	if (!sensors_have_config_file()) {
		ALOGV("%s: No config file found. Using default config.",
		     __func__);
		return;
	}
	rec.size = NUM_AXIS;
	rec.min = AXIS_X;
	rec.max = AXIS_Z;
	rec.store = d->map;
	config_read_axis(d->map_prefix, "axis_map", &rec);

	rec.min = -1;
	rec.max = 1;
	rec.store = d->sign;
	config_read_axis(d->map_prefix, "axis_sign", &rec);
}

static int store_str_attr(struct sensor_desc *d, const char *attr,
			const char *val)
{
	int rc;
	int fd;
	unsigned l;

	if (!*d->phys_path)
		return 0;

	l = strlen(d->phys_path);

	rc = snprintf(d->phys_path + l, sizeof(d->phys_path) - l, "%s", attr);
	if (((unsigned)rc >= sizeof(d->phys_path) - l) || rc < 0) {
		ALOGE("%s: Attr path truncated to '%s'\n", __func__,
				d->phys_path);
		rc = -1;
		goto exit;
	}
	fd = open(d->phys_path, O_WRONLY);
	if (fd < 0) {
		ALOGE("%s: unable to open %s, err %d\n", __func__,
			d->phys_path, errno);
		rc = -1;
		goto exit;
	}
	rc = write(fd, val, strlen(val));
	if (rc < 0)
		ALOGE("%s: unable to write %s (fd %d), err %d\n", __func__,
			d->phys_path, fd, errno);
	close(fd);

exit:
	d->phys_path[l] = 0;
	return rc > 0 ? 0 : rc;
}

static int store_int_attr(struct sensor_desc *d, const char *attr, int value)
{
	char b[16];
	int rc;

	rc = snprintf(b, sizeof(b), "%d\n", value);
	if ((unsigned)rc >= sizeof(b) || rc < 0)
		return -1;
	else
		return store_str_attr(d, attr, b);
}

int find_input_dev(struct sensor_desc *d)
{
	return input_dev_path_by_name(d->input_name, d->dev_path,
			sizeof(d->dev_path));
}

static int open_input_device(struct sensor_desc *d)
{
	int rc = 0;

	if (d->dev_path[0] && !access(d->dev_path, R_OK))
		goto open_device;

	if (d->find_input(d)) {
		ALOGE("%s: No input device for '%s'", __func__, d->sensor.name);
		/*
		* Not created yet? Try to find it next time.
		*/
		d->dev_path[0] = 0;
		return -1;
	}
open_device:
	rc = open(d->dev_path, O_RDONLY | O_NONBLOCK);
	if (rc < 0)
		ALOGE("%s: Failed to open '%s' but got access R_OK",
					__func__, d->dev_path);
	return rc;
}

int sensor_xyz_init(struct sensor_api_t *s_api)
{
	struct sensor_desc *d = container_of(s_api, struct sensor_desc, api);
	int rc;

	rc = dev_phys_path_by_attr("name", d->dev_name, PHYS_PATH_BASE,
		d->phys_path, sizeof(d->phys_path));
	if (rc) {
		ALOGE("%s: no phys dev path for dev name '%s'", __func__,
			d->dev_name);
		*d->phys_path = 0;
	}
	config_read_sensor_map(d);

	if (d->dev_attr_mode) {
		rc = store_str_attr(d, d->dev_attr_mode,
					d->dev_modes[MODE_NORMAL]);

		if (rc) {
			ALOGE("%s: failed to set init mode for dev name '%s'",
				__func__, d->dev_name);
		}
	}

	sensors_select_init(&d->select_worker, d->read, d, -1);

	return 0;
}

void sensor_xyz_close(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	d->select_worker.destroy(&d->select_worker);
}

static enum dev_mode rate2mode(int ms)
{
	if (ms < RATE_GAME)
		return MODE_FASTEST;
	if (ms < RATE_UI)
		return MODE_GAME;
	if (ms < RATE_NORMAL)
		return MODE_UI;

	return MODE_NORMAL;
}

int sensor_xyz_set_delay(struct sensor_api_t *s, int64_t ns)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int rc = 0;
	int ms = ns/NS_TO_MS;

	if (ms != d->applied_delay_ms) {
		rc = store_int_attr(d, d->dev_attr_rate_ms, ms);
		if (rc)
			goto error;
		if (d->dev_attr_mode) {
			const char *old, *new;
			enum dev_mode n = rate2mode(ms);
			enum dev_mode o = rate2mode(d->applied_delay_ms);

			old = d->dev_modes[o];
			new = d->dev_modes[n];

			if (strcmp(old, new)) {
				rc = store_str_attr(d, d->dev_attr_mode, new);
				if (rc)
					goto error;
			}		}
		d->applied_delay_ms = ms;
	}

error:
	return rc;
}

int sensor_xyz_activate(struct sensor_api_t *s, int enable)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd = d->select_worker.get_fd(&d->select_worker);

	/* suspend/resume will be handled in kernel-space */
	if (enable && fd < 0) {
		fd = open_input_device(d);
		if (fd < 0) {
			ALOGE("%s: Failed to enable '%s'",
				__func__, d->sensor.name);
			return fd;
		}
		d->select_worker.set_fd(&d->select_worker, fd);
		d->select_worker.resume(&d->select_worker);
	} else if (!enable && fd >= 0) {
		d->select_worker.set_fd(&d->select_worker, -1);
		d->select_worker.suspend(&d->select_worker);
	}

	return 0;
}

void *sensor_xyz_read(void *arg)
{
	struct input_event events[MAX_EVENTS];
	struct input_event *e;
	struct sensor_desc *p = arg;
	struct sensor_data_t sd;
	int fd = p->select_worker.get_fd(&p->select_worker);
	int i, n;

	if (fd < 0)
		return NULL;

	n = read(fd, events, sizeof(events));
	if (n < 0) {
		ALOGE("%s: read error '%s' from fd %d sensor '%s' built %s @ %s",
			__func__, strerror(errno), fd, p->sensor.name,
			__DATE__, __TIME__);
		return NULL;
	} else if (n == 0) {
		ALOGE("%s: read error end of file from fd %d, sensor '%s'",
			__func__, fd, p->sensor.name);
		return NULL;
	}

	n = n / sizeof(events[0]);
	for (i = 0; i < n; i++) {
		e = events + i;
		if (e->type == EV_ABS) {
			switch (e->code) {
			case ABS_X:
				p->data[p->map[AXIS_X]] =
					e->value * p->sign[AXIS_X];
				break;
			case ABS_Y:
				p->data[p->map[AXIS_Y]] =
					e->value * p->sign[AXIS_Y];
				break;
			case ABS_Z:
				p->data[p->map[AXIS_Z]] =
					e->value * p->sign[AXIS_Z];
				break;
			default:
				break;
			}
		} else if (e->type == EV_SYN) {
			sd.sensor = &p->sensor;
			sd.data = p->data;
			sd.size = NUM_AXIS;
			sd.scale = p->scale;
			sd.status = SENSOR_STATUS_ACCURACY_HIGH;
			sensors_wrapper_data(&sd);
		}
	}

	return NULL;
}
