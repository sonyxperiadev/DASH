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

#if defined(ST_LSM303DLH) || defined(ST_LSM303DLHC)
static struct sensor_desc magnetometer = {
	.sensor = {
#ifdef ST_LSM303DLH
		.name       = "lsm303dlh magnetic",
#elif ST_LSM303DLHC
		.name       = "lsm303dlhc magnetic",
#endif
		.vendor     = "ST Microelectronic",
		.version    = sizeof(sensors_event_t),
		.handle     = SENSOR_MAGNETIC_FIELD_HANDLE,
		.type       = SENSOR_TYPE_MAGNETIC_FIELD,
		.maxRange   = 8.2,
		.resolution = 0.1,
		.power      = 1,
	},
	.api = {
		.init      = sensor_xyz_init,
		.activate  = sensor_xyz_activate,
		.set_delay = sensor_xyz_set_delay,
		.close     = sensor_xyz_close,
	},
	.map_prefix = "lsm303dlhx-magnetic",
	.map   = {0, 1, 2},
	.sign  = {1, 1, 1},
	.scale = 1.0f / 1000.0f * 100.0f, /* 1 Gauss = 100 micro-Tesla*/
	.read = sensor_xyz_read,
	.input_name = "lsm303dlh_mag",
	.find_input = find_input_dev,
	.dev_name = "lsm303dlh_mag",
	.dev_attr_rate_ms = "pollrate_ms",
	.dev_attr_range_mg = "range_mg",
	.dev_attr_mode = NULL,
	.ev_type_data = EV_ABS,
	.ev_type_sync = EV_SYN,
	.ev_code = {ABS_X, ABS_Y, ABS_Z},
};

list_constructor(lsm303dlh_magnetometer);
void lsm303dlh_magnetometer()
{
	sensors_wrapper_register(&magnetometer.sensor,
				&magnetometer.api,
				&magnetometer.entry);
}
#else
#error "ST_LSM303DLH or ST_LSM303DLHC define is required!"
#endif
