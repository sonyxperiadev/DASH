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

#ifndef SENSORS_WRAPPER_H
#define SENSORS_WRAPPER_H

#include <hardware/sensors.h>
#include "sensor_api.h"

#define MAX_SENSOR_CONNECTIONS 4
#define NO_RATE		(-1)

/* Android sensor HAL types and functions */
struct wrapper_access {
	int sensor[MAX_SENSOR_CONNECTIONS];
	int client[MAX_SENSOR_CONNECTIONS];
	int nr;
	int match[MAX_SENSOR_CONNECTIONS];
	int m_nr;
};

struct wrapper_desc {
	struct sensor_t sensor;
	struct sensor_api_t api;
	struct wrapper_access access;
};

int sensors_wrapper_init(struct sensor_api_t *s);
int sensors_wrapper_activate(struct sensor_api_t *s, int enable);
int sensors_wrapper_set_delay(struct sensor_api_t *s, int64_t ns);
void sensors_wrapper_close(struct sensor_api_t *s);

/* Linux sensor HAL types and functions */
struct wrapper_entry {
	struct sensor_api_t *api[MAX_SENSOR_CONNECTIONS];
	unsigned char status[MAX_SENSOR_CONNECTIONS];
	int64_t rate[MAX_SENSOR_CONNECTIONS];
	int nr;
};

void sensors_wrapper_register(struct sensor_t *sensor,
				struct sensor_api_t *api,
				struct wrapper_entry *entry);

void sensors_wrapper_data(struct sensor_data_t *sd);

#endif
