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

#include "sensor_xyz.h"

#ifdef ST_LSM303DLH
static struct sensor_desc accelerometer = {
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
		.init      = sensor_xyz_init,
		.activate  = sensor_xyz_activate,
		.set_delay = sensor_xyz_set_delay,
		.close     = sensor_xyz_close,
	},
	.map_prefix = "lsm303dlh-acceleration",
	.map   = {0, 1, 2},
	.sign  = {1, 1, 1},
	.scale = 1.0f / 1000.f, /* GRAVITY_EARTH applied in wrapper */
	.read = sensor_xyz_read,
	.input_name = "lsm303dlh_acc",
	.find_input = find_input_dev,
	.dev_name = "lsm303dlh_acc",
	.dev_attr_rate_ms = "pollrate_ms",
	.dev_attr_range_mg = "range",
	.dev_attr_mode = NULL,
	.ev_type_data = EV_ABS,
	.ev_type_sync = EV_SYN,
	.ev_code = {ABS_X, ABS_Y, ABS_Z},
};

#elif ST_LSM303DLHC
static struct sensor_desc accelerometer = {
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
		.init      = sensor_xyz_init,
		.activate  = sensor_xyz_activate,
		.set_delay = sensor_xyz_set_delay,
		.close     = sensor_xyz_close,
	},
	.map_prefix = "lsm303dlhc-acceleration",
	.map   = {0, 1, 2},
	.sign  = {1, 1, 1},
	.scale = 1.0f / 1000.f, /* GRAVITY_EARTH applied in wrapper */
	.read = sensor_xyz_read,
	.input_name = "lsm303dlhc_acc",
	.find_input = find_input_dev,
	.dev_name = "lsm303dlhc_acc",
	.dev_attr_rate_ms = "pollrate_ms",
	.dev_attr_range_mg = "range",
	.dev_attr_mode = "mode",
	.dev_modes = {
		[MODE_FASTEST] = "poll",
		[MODE_GAME]    = "poll",
		[MODE_UI]      = "poll",
		[MODE_NORMAL]  = "poll",
	},
	.ev_type_data = EV_ABS,
	.ev_type_sync = EV_SYN,
	.ev_code = {ABS_X, ABS_Y, ABS_Z},
};
#else
#error "ST_LSM303DLH or ST_LSM303DLHC define is required!"
#endif

#if defined(ST_LSM303DLH) || defined(ST_LSM303DLHC)
list_constructor(lsm303dlhx_accelerometer);
void lsm303dlhx_accelerometer()
{
	sensors_wrapper_register(&accelerometer.sensor,
				&accelerometer.api,
				&accelerometer.entry);
}
#endif
