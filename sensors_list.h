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

#ifndef SENSORS_LIST_H_
#define SENSORS_LIST_H_
#include <hardware/sensors.h>
#include "sensor_api.h"

#define list_constructor(fn) extern void fn(void) \
                     __attribute__ ((constructor));

enum sensor_init {
	SENSOR_ERROR      = -1,
	SENSOR_OK         = 0,
	SENSOR_UNREGISTER = 1,
};

void sensors_list_destroy();
int sensors_list_get(struct sensors_module_t* module,
		     struct sensor_t const** plist);
int sensors_list_register(struct sensor_t* sensor, struct sensor_api_t* api);
void sensors_list_deregister(struct sensor_api_t* api);
struct sensor_api_t* sensors_list_get_api_from_handle(int handle);
void sensors_list_foreach_api(int (*f)(struct sensor_api_t* api, void* arg),
			      void *arg);

#endif
