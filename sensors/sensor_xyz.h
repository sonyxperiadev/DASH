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

#ifndef SENSOR_XYZ_H
#define SENSOR_XYZ_H

#include <stdlib.h>
#include <string.h>
#include "sensors_log.h"
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_select.h"
#include "sensor_util.h"
#include "sensors_id.h"
#include "sensors_config.h"
#include "sensors_wrapper.h"
#include "sensors_sysfs.h"
#include "sensor_api.h"

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
	NUM_MODES
};

enum {
	AXIS_X,
	AXIS_Y,
	AXIS_Z,
	NUM_AXIS
};

struct sensor_desc {
	struct sensor_t sensor;
	struct sensor_api_t api;
	struct wrapper_entry entry;
	struct sensors_select_t select_worker;
	struct sensors_sysfs_t sysfs;
	int data[NUM_AXIS];
	char *map_prefix;
	int map[NUM_AXIS];
	int sign[NUM_AXIS];
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
	char *dev_modes[NUM_MODES];
	unsigned short ev_type_data;
	unsigned short ev_type_sync;
	unsigned short ev_code[NUM_AXIS];
};

int sensor_xyz_init(struct sensor_api_t *s_api);
void sensor_xyz_close(struct sensor_api_t *s);
int sensor_xyz_set_delay(struct sensor_api_t *s, int64_t ns);
int sensor_xyz_activate(struct sensor_api_t *s, int enable);
void *sensor_xyz_read(void *arg);
int find_input_dev(struct sensor_desc *d);

#endif
