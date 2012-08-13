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

#define LOG_TAG "DASH - list"

#include "sensors_log.h"
#include <string.h>
#include "sensors_list.h"

#define DASH_MAX_SENSORS 16

static struct sensor_t sensors[DASH_MAX_SENSORS];
static struct sensor_api_t* sensor_apis[DASH_MAX_SENSORS];
static int number_of_sensors = 0;

int sensors_list_get(struct sensors_module_t* module, struct sensor_t const** plist)
{
	*plist = sensors;
	return number_of_sensors;
}

int sensors_list_register(struct sensor_t* sensor, struct sensor_api_t* api)
{
	if (!sensor || !api)
		return -1;

	if (number_of_sensors > DASH_MAX_SENSORS-1)
		return -1;

	sensor_apis[number_of_sensors] = api;
	/* We have to copy due to sensor API */
	memcpy(&sensors[number_of_sensors++], sensor, sizeof(*sensor));

	return 0;
}

void sensors_list_deregister(struct sensor_api_t* api)
{
	int i;

	for (i = 0; i < number_of_sensors; i++)
		if (sensor_apis[i] == api)
			break;

	if (i == number_of_sensors)
		return;

	for ( ; i < number_of_sensors-1; i++) {
		sensor_apis[i] = sensor_apis[i+1];
		sensors[i] = sensors[i+1];
	}

	--number_of_sensors;
}

void sensors_list_destroy()
{
	int i;
	for (i = 0; i < number_of_sensors; i++)
		sensor_apis[i]->close(sensor_apis[i]);
}

struct sensor_api_t* sensors_list_get_api_from_handle(int handle)
{
	int i;
	for (i = 0; i < number_of_sensors; i++)
		if (sensors[i].handle == handle)
			return sensor_apis[i];
	return NULL;
}

void sensors_list_foreach_api(int (*f)(struct sensor_api_t* api, void* arg),
			      void *arg)
{
	int i;
	for (i = 0; i < number_of_sensors; i++)
		if (f(sensor_apis[i], arg) != SENSOR_OK) {
			sensors_list_deregister(sensor_apis[i]);
			/* need to revisit this position after deregister */
			--i;
		}
}
