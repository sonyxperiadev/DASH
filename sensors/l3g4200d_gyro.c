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

static struct sensor_desc gyroscope = {
	.sensor = {
		.name       = "l3g4200d gyroscope",
		.vendor     = "ST Microelectronic",
		.version    = sizeof(sensors_event_t),
		.handle     = SENSOR_GYROSCOPE_HANDLE,
		.type       = SENSOR_TYPE_GYROSCOPE,
		.maxRange   = 2000,
		.resolution = 0.017,
		.power      = 1,
	},
	.api = {
		.init      = sensor_xyz_init,
		.activate  = sensor_xyz_activate,
		.set_delay = sensor_xyz_set_delay,
		.close     = sensor_xyz_close,
	},
	.map_prefix = "l3g4200d-gyroscope",
	.map   = {0, 1, 2},
	.sign  = {1, 1, 1},
	.scale = 0.070f, /* dps scale factor */
	.read = sensor_xyz_read,
	.input_name = "l3g4200d_gyr",
	.find_input = find_input_dev,
	.dev_name = "l3g4200d_gyr",
	.dev_attr_rate_ms = "pollrate_ms",
	.dev_attr_range_mg = "range",
	.dev_attr_mode = NULL,
	.ev_type_data = EV_ABS,
	.ev_type_sync = EV_SYN,
	.ev_code = {ABS_X, ABS_Y, ABS_Z},
};

list_constructor(lsm303dlh_gyroscope);
void lsm303dlh_gyroscope()
{
	sensors_wrapper_register(&gyroscope.sensor,
				&gyroscope.api,
				&gyroscope.entry);
}
