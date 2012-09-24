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

#ifndef SENSOR_UTIL_H_
#define SENSOR_UTIL_H_
#include <stdint.h>

#define container_of(ptr, type, member) ({ \
	const typeof( ((type *)0)->member ) *__mptr = (ptr); \
	(type *)( (char *)__mptr - offsetof(type,member) ); \
})

struct sensor_transform_t {
	int x[3];
	int y[3];
	int z[3];
};

void sensors_nsleep(int64_t ns);
void sensors_usleep(int us);
int64_t get_current_nano_time();
int open_input_dev_by_name(char *name, int flags);
int input_dev_path_by_name(char *name, char *path, int path_max);
int input_dev_path_by_keycode(int type, int code, char *path, int path_max);
int dev_phys_path_by_attr(const char *attr, const char *attr_val,
			const char *base, char *path, int path_max);

#endif
