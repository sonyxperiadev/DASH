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

#define LOG_TAG "DASH - lsm303dlh_compass - wrapper"

#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_id.h"
#include "sensors_wrapper.h"
#include "sensor_util.h"
#include "sensors_compass_API.h"
#include "lsm303dlh.h"
#include "sensor_xyz.h"

#define ACCURACY_HIGH_TH 110
#define ACCURACY_MEDIUM_TH 130
#define ACCURACY_LOW_TH 150

static int num_formations = 1;
static int compass_init(struct sensor_api_t *s)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
	int formation = 0;
	int rc;

	rc = compass_API_Init(LSM303DLH_H_8_1G, num_formations);
	if (rc) {
		LOGE("%s: Failed in API_Init, status %d", __func__, rc);
		return rc;
	}
	compass_API_ChangeFormFactor(formation);

	rc = sensors_wrapper_init(s);
	return rc;
}

static int compass_status(int accuracy)
{
	if (accuracy <= ACCURACY_HIGH_TH)
		return SENSOR_STATUS_ACCURACY_HIGH;
	else if (accuracy < ACCURACY_MEDIUM_TH)
		return SENSOR_STATUS_ACCURACY_MEDIUM;
	else if (accuracy < ACCURACY_LOW_TH)
		return SENSOR_STATUS_ACCURACY_LOW;
	else
		return SENSOR_STATUS_UNRELIABLE;
}

static void compass_data(struct sensor_api_t *s, struct sensor_data_t *sd)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
	sensors_event_t data;
	int accuracy = -1;
	int rc;

	if (sd->sensor->type == SENSOR_TYPE_ACCELEROMETER) {
		rc = compass_API_SaveAcc(sd->data[AXIS_X], sd->data[AXIS_Y],
						sd->data[AXIS_Z]);
		if (rc) {
			LOGE("%s: compass_API_SaveAcc, error %d\n",
				__func__, rc);
			return;
		}
	} else if (sd->sensor->type == SENSOR_TYPE_MAGNETIC_FIELD) {
		rc = compass_API_SaveMag(sd->data[AXIS_X], sd->data[AXIS_Y],
						sd->data[AXIS_Z]);
		if (rc) {
			LOGE("%s: compass_API_SaveMag, error %d\n",
				__func__, rc);
			return;
		}
	} else {
		LOGE("%s: unknown sensor %s\n", __func__, sd->sensor->name);
		return;
	}

	/* only run compass lib on new mag valeus */
	if (sd->sensor->type == SENSOR_TYPE_MAGNETIC_FIELD) {

		rc = compass_API_Run();
		if (!rc) {
			rc = compass_API_OrientationValues(&data);
			if (!rc)
				accuracy = compass_API_GetCalibrationGodness();
			else
				LOGE("%s GetCalibrationGoodness, error %d\n",
					__func__, rc);
		} else {
			LOGE("%s: compass_API_Run, error %d\n", __func__, rc);
		}
		if (accuracy < 0)
			return;

		data.orientation.status = compass_status(accuracy);
		data.timestamp = get_current_nano_time();
		data.sensor = d->sensor.handle;
		data.version = d->sensor.version;
		data.type = d->sensor.type;
		sensors_fifo_put(&data);
	}
}

static struct wrapper_desc compass = {
	.sensor = {
		.name       = "ST compass",
		.vendor     = "ST Microelectronic",
		.version    = sizeof(sensors_event_t),
		.handle     = SENSOR_ORIENTATION_HANDLE,
		.type       = SENSOR_TYPE_ORIENTATION,
		.maxRange   = 360,
		.resolution = 1,
		.power      = 2,
	},
	.api = {
		.init      = compass_init,
		.activate  = sensors_wrapper_activate,
		.set_delay = sensors_wrapper_set_delay,
		.close     = sensors_wrapper_close,
		.data      = compass_data,
	},
	.access = {
		.match = {
			SENSOR_TYPE_ACCELEROMETER,
			SENSOR_TYPE_MAGNETIC_FIELD,
		},
		.m_nr = 2,
	},
};

list_constructor(compass_register);
void compass_register()
{
	sensors_list_register(&compass.sensor, &compass.api);
}
