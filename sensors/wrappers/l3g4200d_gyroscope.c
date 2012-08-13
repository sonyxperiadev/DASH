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

#define LOG_TAG "DASH - l3g4200d_gyro - wrapper"

#include <stdlib.h>
#include <string.h>
#include "sensors_log.h"
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_id.h"
#include "sensors_wrapper.h"
#include "sensor_util.h"
#include "sensor_xyz.h"

#define DPS_2_RADPS (360/(2*3.14159265))

static void gyroscope_data(struct sensor_api_t *s, struct sensor_data_t *sd)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
	sensors_event_t data;

	data.timestamp = get_current_nano_time();
	data.sensor = d->sensor.handle;
	data.version = d->sensor.version;
	data.type = d->sensor.type;
	data.gyro.status  = sd->status;
	/* convert to dps with scale, and then to radps for android */
	data.gyro.x = (sd->data[AXIS_X] * sd->scale)/DPS_2_RADPS;
	data.gyro.y = (sd->data[AXIS_Y] * sd->scale)/DPS_2_RADPS;
	data.gyro.z = (sd->data[AXIS_Z] * sd->scale)/DPS_2_RADPS;
	sensors_fifo_put(&data);
}

static struct wrapper_desc gyroscope = {
	.sensor = {
		.name       = "ST gyroscope",
		.vendor     = "ST Microelectronic",
		.version    = sizeof(sensors_event_t),
		.handle     = SENSOR_GYROSCOPE_HANDLE,
		.type       = SENSOR_TYPE_GYROSCOPE,
		.maxRange   = 2000,
		.resolution = 0.017,
		.power      = 1,
	},
	.api = {
		.init      = sensors_wrapper_init,
		.activate  = sensors_wrapper_activate,
		.set_delay = sensors_wrapper_set_delay,
		.close     = sensors_wrapper_close,
		.data      = gyroscope_data,
	},
	.access = {
		.match = {
			SENSOR_TYPE_GYROSCOPE,
		},
		.m_nr = 1,
	},
};

list_constructor(gyroscope_register);
void gyroscope_register()
{
	sensors_list_register(&gyroscope.sensor, &gyroscope.api);
}
