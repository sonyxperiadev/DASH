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

#define LOG_TAG "DASH"

#include <stddef.h>
#include <string.h>
#include "sensors_log.h"
#include <pthread.h>
#include "sensor_util.h"
#include "sensors_wrapper.h"

#define UNUSED		0
#define CLOSE		0x1
#define INIT		0x2
#define ACTIVE		0x4

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define LOCK(p) do { \
	ALOGD("%s(%d): %s: lock\n", __FILE__, __LINE__, __func__); \
	pthread_mutex_lock(p); \
} while (0)

#define UNLOCK(p) do { \
	ALOGD("%s(%d): %s: unlock\n", __FILE__, __LINE__, __func__); \
	pthread_mutex_unlock(p); \
} while (0)

pthread_mutex_t wrapper_mutex = PTHREAD_MUTEX_INITIALIZER;

struct wrapper_list {
	struct sensor_t *sensor;
	struct sensor_api_t *api;
	struct wrapper_entry *entry;
};
static struct wrapper_list list[16];
static int idx = 0;

/* list manipulation routines */
static int list_get_status(int sensor, unsigned char pattern)
{
	int j;
	int found = 0;

	for (j = 0; j < list[sensor].entry->nr; j++) {
		if (list[sensor].entry->status[j] & pattern)
			found++;
	}

	return found;
}

static void list_set_status(int sensor, int client,
						unsigned char pattern)
{
	list[sensor].entry->status[client] |= pattern;
}

static void list_clear_status(int sensor, int client,
						unsigned char pattern)
{
	list[sensor].entry->status[client] &= ~pattern;
}

static int64_t list_get_rate(int sensor)
{
	int j;
	int64_t rate = NO_RATE;

	for (j = 0; j < list[sensor].entry->nr; j++) {
		if ((list[sensor].entry->rate[j] >= 0) &&
			(list[sensor].entry->rate[j] < (uint64_t)rate))
			rate = list[sensor].entry->rate[j];
	}

	return rate;
}

static void list_set_rate(int sensor, int client, int64_t rate)
{
	list[sensor].entry->rate[client] = rate;
}

static void list_set_api(int sensor, int client, struct sensor_api_t *s)
{
	list[sensor].entry->api[client] = s;
}

/* perform init of entry and store pointers in the internal wrapper list */
void sensors_wrapper_register(struct sensor_t *sensor,
				struct sensor_api_t *api,
				struct wrapper_entry *entry)
{
	int i;

	if (sensor == NULL || api == NULL || entry == NULL) {
		if (sensor == NULL)
			ALOGE("%s: Error sensor is NULL pointer", __func__);
		else
			ALOGE("%s: Error %s NULL pointer", __func__,
				sensor->name);
		return;
	}

	for (i = 0; i < MAX_SENSOR_CONNECTIONS; i++) {
		entry->api[i] = NULL;
		entry->status[i] = UNUSED;
		entry->rate[i] = NO_RATE;
	}
	entry->nr = 0;

	LOCK(&wrapper_mutex);
	if( idx < (int) ARRAY_SIZE(list)) {
		list[idx].sensor = sensor;
		list[idx].api = api;
		list[idx].entry = entry;
		idx++;
	}
	UNLOCK(&wrapper_mutex);
}

/* find sensor match in list and call all the data api entry functions on it
   lock and unlock is handled by sensor select to keep the lock order */
void sensors_wrapper_data(struct sensor_data_t *sd)
{
	int i = 0;
	int j = 0;

	while (sd->sensor != list[i].sensor) {
		i++;
		if (i >= idx) {
			ALOGE("%s: Error %s not found",
				__func__, sd->sensor->name);
			return;
		}
	}

	for (j = 0; j < list[i].entry->nr; j++) {
		if (list[i].entry->status[j] & ACTIVE) {
			if (list[i].entry->api[j]->data != NULL)
				list[i].entry->api[j]->data(
					list[i].entry->api[j], sd);
		}
	}
}

/* match supplied sensor with the entries in the internal wrapper list and
   update the access information and entry information for all matches */
int sensors_wrapper_init(struct sensor_api_t *s)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
	int i;
	int err = -1;

	LOCK(&wrapper_mutex);
scan:
	for (i = 0; i < idx; i++) {
		if (list[i].sensor->type == d->access.match[d->access.nr]) {
			int init, rv = 0;
			ALOGV("%s: matched '%s' and '%s'", __func__,
				d->sensor.name, list[i].sensor->name);

			d->access.sensor[d->access.nr] = i;
			d->access.client[d->access.nr] = list[i].entry->nr;

			init = list_get_status(i, INIT);
			if (!init)
				rv = list[i].api->init(list[i].api);

			if (rv < 0) {
				ALOGE("%s: '%s' init failed, continue search",
				__func__, list[i].sensor->name);
				err = rv;
			} else {
				list_set_status(
					d->access.sensor[d->access.nr],
					d->access.client[d->access.nr],
					INIT);

				list_set_api(d->access.sensor[d->access.nr],
						d->access.client[d->access.nr],
						&d->api);

				list[i].entry->nr++;
				d->access.nr++;

				if (d->access.nr != d->access.m_nr) {
					goto scan;
				} else {
					err = 0;
					break;
				}
			}
		}
	}
	UNLOCK(&wrapper_mutex);

	return err;
}

/* perfom activate for all sensors included in the access field, but it will
   only be performed once for each sensor in the interal wrapper list */
int sensors_wrapper_activate(struct sensor_api_t *s, int enable)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
	int i;
	int sensor;
	int client;
	int active;
	int rv = 0;
	int64_t old_rate, new_rate;

	LOCK(&wrapper_mutex);
	for (i = 0; i < d->access.nr; i++) {
		sensor = d->access.sensor[i];
		client = d->access.client[i];

		if (!enable) {
			list_clear_status(sensor, client, ACTIVE);

			old_rate = list_get_rate(sensor);
			list_set_rate(sensor, client, NO_RATE);
			new_rate = list_get_rate(sensor);
			if ((new_rate != NO_RATE) &&
				(old_rate != new_rate))
				rv = list[sensor].api->set_delay(
					list[sensor].api, new_rate);
		}

		active = list_get_status(sensor, ACTIVE);

		if (enable)
			list_set_status(sensor, client, ACTIVE);

		if (!active)
			rv = list[sensor].api->activate(list[sensor].api,
								enable);
	}
	UNLOCK(&wrapper_mutex);

	return rv;
}

/* perfom set delay for all sensors included in the access field, but it will
   only be performed if the new rate is the fastest one for this sensor */
int sensors_wrapper_set_delay(struct sensor_api_t *s, int64_t ns)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
	int i;
	int sensor;
	int client;
	int rv = 0;
	int64_t old_rate, new_rate;

	LOCK(&wrapper_mutex);
	for (i = 0; i < d->access.nr; i++) {
		sensor = d->access.sensor[i];
		client = d->access.client[i];
		old_rate = list_get_rate(sensor);
		list_set_rate(sensor, client, ns);
		new_rate = list_get_rate(sensor);

		if (old_rate != new_rate)
			rv = list[sensor].api->set_delay(
				list[sensor].api, new_rate);
	}
	UNLOCK(&wrapper_mutex);
	return rv;
}

/* perfom close for all sensors included in the access field, but it will
   only be performed if all client on the specific sensor has requested it.
   rate will be invalidated if close occurs and must be re evaluated again */
void sensors_wrapper_close(struct sensor_api_t *s)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
	int i;
	int sensor;
	int client;
	int close;

	LOCK(&wrapper_mutex);
	for (i = 0; i < d->access.nr; i++) {
		sensor = d->access.sensor[i];
		client = d->access.client[i];
		list_set_status(sensor, client, CLOSE);
		list_set_rate(sensor, client, NO_RATE);
		close = list_get_status(sensor, CLOSE);
		if (close == list[sensor].entry->nr)
			list[sensor].api->close(list[sensor].api);
	}
	UNLOCK(&wrapper_mutex);
}
