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

#ifndef SENSOR_API_H_
#define SENSOR_API_H_
#include <stdint.h>

struct sensor_data_t {
	struct sensor_t *sensor;
	int *data;
	int size;
	float scale;
	int status;
	int64_t timestamp;
	int delay;
};

struct sensor_api_t {
	int (*init)(struct sensor_api_t *s);
	int (*activate)(struct sensor_api_t *s, int enable);
	int (*set_delay)(struct sensor_api_t *s, int64_t ns);
	void (*close)(struct sensor_api_t *s);
	void (*data)(struct sensor_api_t *s, struct sensor_data_t *sd);
};

#endif
