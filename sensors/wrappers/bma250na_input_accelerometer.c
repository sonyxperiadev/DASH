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

#define LOG_TAG "DASH - bma250_input_acc - wrapper"

#include <string.h>
#include "sensors_log.h"
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_select.h"
#include "sensor_util.h"
#include "sensors_id.h"
#include "sensors_config.h"
#include "sensors_wrapper.h"
#include "sensor_xyz.h"

static void bma250_data(struct sensor_api_t *s, struct sensor_data_t *sd);

static struct wrapper_desc bma250_input = {
	.sensor = {
		name: "BMA250 accelerometer",
		vendor: "Bosch Sensortec GmbH",
		version: sizeof(sensors_event_t),
		handle: SENSOR_ACCELEROMETER_HANDLE,
		type: SENSOR_TYPE_ACCELEROMETER,
		maxRange: 156.96, /* max +/-16G */
		resolution: 20,
		power: 0.003,/* sleep 50ms */
		minDelay: 5000
	},
	.api = {
		.init = sensors_wrapper_init,
		.activate = sensors_wrapper_activate,
		.set_delay = sensors_wrapper_set_delay,
		.close = sensors_wrapper_close,
		.data = bma250_data,
	},
	.access = {
		.match = {
			SENSOR_TYPE_ACCELEROMETER,
		},
		.m_nr = 1,
	},
};

static void bma250_data(struct sensor_api_t *s, struct sensor_data_t *sd)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
	sensors_event_t data;

	data.sensor = d->sensor.handle;
	data.type = d->sensor.type;
	data.version = d->sensor.version;
	data.timestamp = sd->timestamp;
	data.acceleration.status = sd->status;
	data.acceleration.x = sd->data[AXIS_X] * sd->scale * GRAVITY_EARTH;
	data.acceleration.y = sd->data[AXIS_Y] * sd->scale * GRAVITY_EARTH;
	data.acceleration.z = sd->data[AXIS_Z] * sd->scale * GRAVITY_EARTH;
	sensors_fifo_put(&data);
}

list_constructor(bma250_register);
void bma250_register()
{
	(void)sensors_list_register(&bma250_input.sensor, &bma250_input.api);
}
