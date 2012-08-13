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

#define LOG_TAG "DASH - lsm303dlh"

#include <stdlib.h>
#include <string.h>
#include "sensors_log.h"
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
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
#include "lsm303dlh.h"
#include "sensors_compass_API.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define DELAY_LOWEST_MS 1000
#define DEBUG_VERBOSE 0
#define PHYS_PATH_BASE "/sys/bus/i2c/devices"
#define PHYS_PATH_LEN  (sizeof(PHYS_PATH_BASE) + sizeof("/0-0000/"))
#define ATTR_NAME_LEN  32
#define DEV_PATH_LEN  sizeof("/dev/input/event/4294967295")


enum android_rates {
	RATE_GAME   =  20,
	RATE_UI     =  60,
	RATE_NORMAL = 200,
};

enum dev_mode {
	MODE_FASTEST,
	MODE_GAME,
	MODE_UI,
	MODE_NORMAL,
	MODES_NUM
};

enum user_type {
	USER_INTERNAL = 0x01,
	USER_EXTERNAL = 0x02,
};

enum {
	AXIS_X,
	AXIS_Y,
	AXIS_Z,
	NUM_AXIS
};

struct config_record {
	int max;
	int min;
	int size;
	int *store;
};

struct sensor_desc {
	struct sensor_t sensor;
	struct sensor_api_t api;
	struct sensors_select_t select_worker;
	int status;
	int data[3];
	char *map_prefix;
	int map[3];
	int sign[3];
	enum user_type users;
	int64_t internal_delay;
	int64_t external_delay;
	int applied_delay_ms;
	float scale;
	void * (*read)(void *);
	int (*find_input)(struct sensor_desc *d);
	char *input_name;
	char dev_path[DEV_PATH_LEN];
	char phys_path[PHYS_PATH_LEN + ATTR_NAME_LEN];
	char *dev_name;
	char *dev_attr_rate_ms;
	char *dev_attr_range_mg;
	char *dev_attr_mode;
	char *dev_modes[MODES_NUM];
};

static struct sensor_desc *required_sensors[2];
static struct sensor_desc *sensor_acc;
static struct sensor_desc *sensor_mag;
static struct sensor_desc *sensor_compass;
static int compass_api_activated;
static int num_formations;
static pthread_mutex_t lock;

static int config_read_axis(char *prefix, char *key, struct config_record *cr)
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

static void config_read_sensor_map(struct sensor_desc *d)
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
	config_read_axis(d->map_prefix, "axis_map", &rec);

	rec.min = -1;
	rec.max = 1;
	rec.store = d->sign;
	config_read_axis(d->map_prefix, "axis_sign", &rec);

	ALOGD("%s: %s: axis map [%2d %2d %2d]", __func__, d->sensor.name,
	     d->map[AXIS_X], d->map[AXIS_Y], d->map[AXIS_Z]);
	ALOGD("%s: %s: axis sign [%2d %2d %2d]", __func__, d->sensor.name,
	     d->sign[AXIS_X], d->sign[AXIS_Y], d->sign[AXIS_Z]);
}

static int store_str_attr(struct sensor_desc *d, const char *attr,
			const char *val)
{
	int rc;
	int fd;
	unsigned l = strlen(d->phys_path);

	if (!*d->phys_path)
		return 0;

	rc = snprintf(d->phys_path + l, sizeof(d->phys_path) - l, "%s", attr);
	if ((unsigned)rc >= sizeof(d->phys_path) - l) {
		ALOGE("%s: Attr path truncated to '%s'\n", __func__,
				d->phys_path);
		rc = -1;
		goto exit;
	}
	fd = open(d->phys_path, O_WRONLY);
	ALOGD("%s: '%s' = %s", __func__, d->phys_path, val);
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

	(void)snprintf(b, sizeof(b), "%d\n", value);
	return store_str_attr(d, attr, b);
}

static void scale_data(struct sensors_event_t *s, struct sensor_desc *d)
{
	unsigned i;
	float scale = d->scale;

	for (i = 0; i < ARRAY_SIZE(s->data); i++)
		s->data[i] = d->data[i] * scale;
}

static int compass_status(int accuracy)
{
	if (accuracy <= 110)
		return SENSOR_STATUS_ACCURACY_HIGH;
	else if (accuracy < 130)
		return SENSOR_STATUS_ACCURACY_MEDIUM;
	else if (accuracy < 150)
		return SENSOR_STATUS_ACCURACY_LOW;
	else
		return SENSOR_STATUS_UNRELIABLE;
}

static int compass_run(sensors_event_t *s)
{
	int rc;
	int accuracy = -1;

	rc = compass_API_SaveAcc(sensor_acc->data[0],
				sensor_acc->data[1],
				sensor_acc->data[2]);
	if (rc) {
		ALOGE("%s: compass_API_SaveAcc, error %d\n", __func__, rc);
		goto exit;
	}
	rc = compass_API_SaveMag(sensor_mag->data[0],
				sensor_mag->data[1],
				sensor_mag->data[2]);
	if (rc) {
		ALOGE("%s: compass_API_SaveMag, error %d\n", __func__, rc);
		goto exit;
	}
	rc = compass_API_Run();
	if (!rc) {
		rc = compass_API_OrientationValues(s);
		if (!rc)
			accuracy = compass_API_GetCalibrationGodness();
		else
			ALOGE("%s GetCalibrationGodness, error %d\n",
				__func__, rc);
	} else {
		ALOGE("%s: compass_API_Run, error %d\n", __func__, rc);
	}
	rc = compass_API_GetNewFullScale();
	if (rc) {
		int range = to_lsm303dlh_mag_range(rc);

		ALOGD("%s: new range %d requested", __func__, rc);
		if (sensor_mag->dev_attr_range_mg)
			store_int_attr(sensor_mag,
					sensor_mag->dev_attr_range_mg, range);
		else
			ALOGI("%s: magnetic range not supported", __func__);
	}
exit:
	ALOGD("%s: accuracy %d\n", __func__, accuracy);
	return accuracy;
}

static void compass_formation(int value)
{
	ALOGD("%s: formation %d", __func__, value);
	compass_API_ChangeFormFactor(!!value);
}

static int find_input_dev(struct sensor_desc *d)
{
	return input_dev_path_by_name(d->input_name, d->dev_path,
			sizeof(d->dev_path));
}

static int compass_find_input_dev(struct sensor_desc *d)
{
	return input_dev_path_by_keycode(EV_SW, SW_LID, d->dev_path,
			sizeof(d->dev_path));
}

static int open_input_device(struct sensor_desc *d)
{
	int rc;

	if (!*d->dev_path) {
		ALOGD("%s: No input device for %s", __func__, d->sensor.name);
		return -1;
	}
	rc = open(d->dev_path, O_RDONLY | O_NONBLOCK);
	if (rc < 0) {
		ALOGE("%s: Failed to open '%s' but access (R_OK) got",
					__func__, d->dev_path);
	}

	return rc;
}

static int sensor_init(struct sensor_api_t *s_api)
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
	sensors_select_init(&d->select_worker, d->read, d, -1);

	return 0;
}

static int set_current_formation(int fd, int formation)
{
	if (fd >= 0) {
		/*
		* TODO: perform formation check (ioctl)
		*/
		formation = 0;
	}
	compass_API_ChangeFormFactor(formation);

	return formation;
}

static int compass_api_init(struct sensor_desc *d, int fd)
{
	int rc;
	int f;

	rc = compass_API_Init(LSM303DLH_H_8_1G, num_formations);
	if (rc) {
		ALOGE("%s: Failed in API_Init, status %d", __func__, rc);
		return rc;
	}
	f = set_current_formation(fd, 0);

	return 0;
}

static int compass_sensor_init(struct sensor_api_t *s_api)
{
	int fd;
	struct sensor_desc *d = container_of(s_api, struct sensor_desc, api);

	fd = open_input_device(d);
	compass_api_init(d, fd);
	if (fd >= 0)
		close(fd);
	sensors_select_init(&d->select_worker, d->read, d, -1);

	return 0;
}

static void sensor_close(struct sensor_api_t *s)
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

static int set_pollrate(struct sensor_desc *d, int ms)
{
	if (ms != d->applied_delay_ms) {
		int rc = store_int_attr(d, d->dev_attr_rate_ms, ms);
		if (rc)
			return rc;
		if (d->dev_attr_mode) {
			const char *old, *new;
			enum dev_mode n = rate2mode(ms);
			enum dev_mode o = rate2mode(d->applied_delay_ms);

			old = d->dev_modes[o];
			new = d->dev_modes[n];

			if (strcmp(old, new))
				rc = store_str_attr(d, d->dev_attr_mode, new);
		}
		d->applied_delay_ms = ms;
	}

	return 0;
}

static int update_delay(struct sensor_desc *d)
{
	int ms = DELAY_LOWEST_MS;
	int d_int = d->internal_delay / 1000000;
	int d_ext = d->external_delay / 1000000;
	int rc = 0;

	if (!d->dev_attr_rate_ms)
		return 0;

	if (d_int && d_ext)
		ms = d_int < d_ext ? d_int : d_ext;
	else if (d_int)
		ms = d_int;
	else if (d_ext)
		ms = d_ext;

	if (ms != d->applied_delay_ms) {
		rc = set_pollrate(d, ms);
		if (!rc)
			d->applied_delay_ms = ms;
	}

	return rc;
}

static int sensor_set_delay(struct sensor_api_t *s, int64_t ns)
{
	int rc = 0;
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	pthread_mutex_lock(&lock);
	d->external_delay = ns;
	update_delay(d);
	pthread_mutex_unlock(&lock);

	return rc;
}

static int compass_sensor_set_delay(struct sensor_api_t *s, int64_t ns)
{
	int rc = 0;
	unsigned i;
	struct sensor_desc *d;

	pthread_mutex_lock(&lock);
	for (i = 0; i < ARRAY_SIZE(required_sensors); i++) {
		d = required_sensors[i];
		d->internal_delay = ns;
		rc += update_delay(d);
	}
	pthread_mutex_unlock(&lock);

	return rc ? -1 : 0;
}

static int sensor_activate_local(struct sensor_desc *d, int enable,
				enum user_type user)
{
	int fd = d->select_worker.get_fd(&d->select_worker);

	if (enable)
		d->users |= user;
	else
		d->users &= ~user;

	if (enable && fd < 0) {
		fd = open_input_device(d);
		if (fd < 0)
			return -1;
		d->select_worker.set_fd(&d->select_worker, fd);
		d->select_worker.resume(&d->select_worker);
	} else if (!enable && fd > 0 && !d->users) {
		d->select_worker.suspend(&d->select_worker);
		d->select_worker.set_fd(&d->select_worker, -1);
	}
	return 0;
}

static int sensor_activate(struct sensor_api_t *s, int enable)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int rc;

	pthread_mutex_lock(&lock);
	rc = sensor_activate_local(d, enable, USER_EXTERNAL);
	pthread_mutex_unlock(&lock);

	return rc;

}

static int activate_required_sensors(int enable)
{
	unsigned i;
	struct sensor_desc *d;

	for (i = 0; i < ARRAY_SIZE(required_sensors); i++) {
		d = required_sensors[i];
		/*
		* If any of required sensors can't be ativated (enable == 1)
		* then comapss is also not operational
		*/
		if (sensor_activate_local(d, enable, USER_INTERNAL) && enable)
			goto err_exit;
		if (!enable) {
			d->internal_delay = 0;
			update_delay(d);
		}
	}
	return 0;

err_exit:
	ALOGE("%s: failed on sensor '%s'\n", __func__, d->sensor.name);
	while(i > 0) {
		d = required_sensors[--i];
		sensor_activate_local(d, 0, USER_INTERNAL);
	}
	return -1;
}

static int compass_sensor_activate(struct sensor_api_t *s, int enable)
{
	int rc = 0;
	unsigned i;
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd = d->select_worker.get_fd(&d->select_worker);

	pthread_mutex_lock(&lock);

	if (enable)
		d->users |= USER_EXTERNAL;
	else
		d->users &= ~USER_EXTERNAL;

	if (enable) {
		if (fd < 0)
			fd = open_input_device(d);
		rc = activate_required_sensors(1);
		if (!rc) {
			d->select_worker.set_fd(&d->select_worker, fd);
			d->select_worker.resume(&d->select_worker);
			goto exit;
		}
		if (fd >= 0)
			close(fd);
		ALOGE("%s: '%s' can't be activated\n", __func__, d->sensor.name);
	} else {
		d->select_worker.suspend(&d->select_worker);
		if (fd > 0) {
			ALOGD("%s: closing down fd %d\n", __func__, fd);
			d->select_worker.set_fd(&d->select_worker, -1);
		}
		activate_required_sensors(0);
	}
exit:
	pthread_mutex_unlock(&lock);
	return rc;
}

#define MAX_EVENTS 4 /* X, Y, Z, SYN */
static void *sensor_read(void *arg)
{
	struct input_event events[MAX_EVENTS];
	struct input_event *e;
	int i;
	int n;
	int64_t t;
	sensors_event_t sdata;
	struct sensor_desc *p = arg;
	int fd = p->select_worker.get_fd(&p->select_worker);

	t = get_current_nano_time();

	if (fd < 0)
		return 0;

	n = read(fd, events, sizeof(events)) / sizeof(events[0]);
	if (n < 0) {
		ALOGE("%s: read error from fd %d, sensor '%s'", __func__,
			fd, p->sensor.name);
		return 0;
	}

	pthread_mutex_lock(&lock);
	for (i = 0; i < n; i++) {
		e = events + i;
		if (e->type == EV_SW && e->code == SW_LID) {
			compass_formation(e->value);
			continue;
		}

		if (e->type == EV_SYN &&
				p->sensor.type != SENSOR_TYPE_ORIENTATION) {

			ALOGD_IF(DEBUG_VERBOSE, "%s(%s):%9lld %6d %6d %6d",
					__func__,
					p->sensor.name,
					t,
					p->data[0],
					p->data[1],
					p->data[2]);

			if (p->users & USER_EXTERNAL) {
				memset(&sdata, 0, sizeof(sdata));
				scale_data(&sdata, p);
				sdata.sensor = p->sensor.handle;
				sdata.orientation.status = SENSOR_STATUS_ACCURACY_MEDIUM;
				sdata.version = p->sensor.version;
				sdata.timestamp = t;
				sensors_fifo_put(&sdata);
			}

			if (sensor_compass &&
				sensor_compass->users & USER_EXTERNAL &&
				p->sensor.type == SENSOR_TYPE_MAGNETIC_FIELD) {

				int accuracy;

				memset(&sdata, 0, sizeof(sdata));
				accuracy = compass_run(&sdata);
				if (accuracy < 0)
					continue;
				sdata.orientation.status = compass_status(accuracy);
				sdata.timestamp = t;
				sdata.sensor = sensor_compass->sensor.handle;
				sdata.version = sensor_compass->sensor.version;
				sdata.type = sensor_compass->sensor.type;
				sensors_fifo_put(&sdata);
				ALOGD_IF(DEBUG_VERBOSE,
					"%s(%s):%9lld a=%3.0f, p=%4.1f, r=%4.1f, "
					"status=%d (accuracy=%d)",
					__func__,
					sensor_compass->sensor.name,
					t,
					sdata.orientation.azimuth,
					sdata.orientation.pitch,
					sdata.orientation.roll,
					sdata.orientation.status,
					accuracy);
			}
			continue;
		}

		if (e->type != EV_ABS)
			continue;

		switch (e->code) {
		case ABS_X:
			p->data[p->map[0]] = e->value * p->sign[0];
			break;
		case ABS_Y:
			p->data[p->map[1]] = e->value * p->sign[1];
			break;
		case ABS_Z:
			p->data[p->map[2]] = e->value * p->sign[2];
			break;
		}
	}
	pthread_mutex_unlock(&lock);

	return NULL;
}

static struct sensor_desc magnetometer = {
	.sensor = {
		.name       = "lsm303dlh magnetic",
		.vendor     = "ST Microelectronic",
		.version    = sizeof(sensors_event_t),
		.handle     = SENSOR_MAGNETIC_FIELD_HANDLE,
		.type       = SENSOR_TYPE_MAGNETIC_FIELD,
		.maxRange   = 8.2,
		.resolution = 0.1,
		.power      = 1,
	},
	.api = {
		.init      = sensor_init,
		.activate  = sensor_activate,
		.set_delay = sensor_set_delay,
		.close     = sensor_close,
	},
	.map_prefix = "lsm303dlh-magnetic",
	.map   = {0, 1, 2},
	.sign  = {1, 1, 1},
	.scale = 1.0f / 1000.0f * 100.0f, /* 1 Gauss = 100 micro-Tesla*/
	.read = sensor_read,
	.input_name = "lsm303dlh_mag",
	.find_input = find_input_dev,
	.dev_name = "lsm303dlh_mag",
	.dev_attr_rate_ms ="pollrate_ms",
	.dev_attr_range_mg = "range_mg",
	.dev_attr_mode = NULL,
};

static struct sensor_desc ls303dlh_acc = {
	.sensor = {
		.name       = "lsm303dlh acceleration",
		.vendor     = "ST Microelectronic",
		.version    = sizeof(sensors_event_t),
		.handle     = SENSOR_ACCELEROMETER_HANDLE,
		.type       = SENSOR_TYPE_ACCELEROMETER,
		.maxRange   = 8,
		.resolution = 0.1,
		.power      = 1,
	},
	.api = {
		.init      = sensor_init,
		.activate  = sensor_activate,
		.set_delay = sensor_set_delay,
		.close     = sensor_close,
	},
	.map_prefix = "lsm303dlh-acceleration",
	.map   = {0, 1, 2},
	.sign  = {1, 1, 1},
	.scale = GRAVITY_EARTH / 1000.f,
	.read = sensor_read,
	.input_name = "lsm303dlh_acc",
	.find_input = find_input_dev,
	.dev_name = "lsm303dlh_acc",
	.dev_attr_rate_ms = "pollrate_ms",
	.dev_attr_range_mg = "range",
	.dev_attr_mode = NULL,
};

static struct sensor_desc ls303dlhc_acc = {
	.sensor = {
		.name       = "lsm303dlhc acceleration",
		.vendor     = "ST Microelectronic",
		.version    = sizeof(sensors_event_t),
		.handle     = SENSOR_ACCELEROMETER_HANDLE,
		.type       = SENSOR_TYPE_ACCELEROMETER,
		.maxRange   = 16,
		.resolution = 0.1,
		.power      = 1,
	},
	.api = {
		.init      = sensor_init,
		.activate  = sensor_activate,
		.set_delay = sensor_set_delay,
		.close     = sensor_close,
	},
	.map_prefix = "lsm303dlhc-acceleration",
	.map   = {0, 1, 2},
	.sign  = {1, 1, 1},
	.scale = GRAVITY_EARTH / 1000.f,
	.read = sensor_read,
	.input_name = "lsm303dlhc_acc",
	.find_input = find_input_dev,
	.dev_name = "lsm303dlhc_acc",
	.dev_attr_rate_ms = "pollrate_ms",
	.dev_attr_range_mg = "range",
	.dev_attr_mode = "mode",
	.dev_modes = {
		[MODE_FASTEST] = "poll",
		[MODE_GAME]    = "poll",
		[MODE_UI]      = "irq",
		[MODE_NORMAL]  = "irq",
	},
};

static struct sensor_desc compass = {
	.sensor = {
		.name       = "lsm303dlh compass",
		.vendor     = "ST Microelectronic",
		.version    = sizeof(sensors_event_t),
		.handle     = SENSOR_ORIENTATION_HANDLE,
		.type       = SENSOR_TYPE_ORIENTATION,
		.maxRange   = 360,
		.resolution = 1,
		.power      = 2,
	},
	.api = {
		.init      = compass_sensor_init,
		.activate  = compass_sensor_activate,
		.set_delay = compass_sensor_set_delay,
		.close     = sensor_close,
	},
	.map_prefix = "n/a",
	.map   = {0, 1, 2},
	.sign  = {1, 1, 1},
	.scale = 1.0f,
	.read = sensor_read,
	.input_name = "slider",
	.find_input = compass_find_input_dev,
};

list_constructor(lsm303dlh_init_driver);
void lsm303dlh_init_driver()
{
	pthread_mutex_init(&lock, NULL);

	if (!ls303dlhc_acc.find_input(&ls303dlhc_acc))
		sensor_acc = &ls303dlhc_acc;
	else if (!ls303dlh_acc.find_input(&ls303dlh_acc))
		sensor_acc = &ls303dlh_acc;
	if (sensor_acc) {
		sensors_list_register(&sensor_acc->sensor, &sensor_acc->api);
		ALOGI("Accelerometer '%s' in use.", sensor_acc->sensor.name);
	} else {
		ALOGE("No accelerometer found.");
	}

	if (!magnetometer.find_input(&magnetometer)) {
		sensor_mag = &magnetometer;
		sensors_list_register(&sensor_mag->sensor, &sensor_mag->api);
		ALOGI("Magnetometer '%s' in use.", sensor_mag->sensor.name);
	} else {
		ALOGE("No magnetometer found.");
	}

	if (sensor_acc && sensor_mag) {
		if (!compass.find_input(&compass)) {
			ALOGI("e-Compass: device with slider detected.");
			num_formations = 1;
		} else {
			ALOGI("e-Compass: no slider found.");
			*compass.dev_path = 0;
		}
		sensors_list_register(&compass.sensor, &compass.api);
		required_sensors[0] = sensor_acc;
		required_sensors[1] = sensor_mag;
		sensor_compass = &compass;
		ALOGI("e-Compass '%s' in use.", compass.sensor.name);
	}
}
