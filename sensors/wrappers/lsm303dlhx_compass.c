#define LOG_TAG "DASH ecompass - wrapper"

#include <stdlib.h>
#include <string.h>
#include "sensors_log.h"
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_id.h"
#include "sensors_wrapper.h"
#include "sensor_util.h"
#include "sensors_compass_API.h"
#include "sensor_xyz.h"

#define SENSOR_TYPE_ORIENTATION_BIT    (0)
#define SENSOR_TYPE_MAGNETIC_FIELD_BIT (1)
#define COMPASS_DELAY 25000000
#define ACCURACY_HIGH_TH 110
#define ACCURACY_MEDIUM_TH 130
#define ACCURACY_LOW_TH 150

static int ecompass_init(struct sensor_api_t *s);
static int ecompass_activate(struct sensor_api_t *s, int enable);
static int ecompass_delay(struct sensor_api_t *s, int64_t ns);
static void ecompass_close(struct sensor_api_t *s);
static void ecompass_data(struct sensor_api_t *s, struct sensor_data_t *sd);

struct engine_t {
	int enable_mask;
	int init; /* true, if has initialized*/
	int init_ret;
	int num_formations;
	struct wrapper_desc ecompass;
	struct wrapper_desc compass;
	struct wrapper_desc magnetometer;
};

static struct engine_t engine = {
	.init = 0,
	.init_ret = SENSOR_OK,
	.num_formations = 1,
	.ecompass = {
		.sensor = {
			.name       = "ST ecompass internal",
			.vendor     = "ST Microelectronic",
			.version    = sizeof(sensors_event_t),
			.handle     = SENSOR_INTERNAL_HANDLE_MIN,
		},
		.api = {
			.init      = ecompass_init,
			.activate  = ecompass_activate,
			.set_delay = ecompass_delay,
			.close     = ecompass_close,
			.data      = ecompass_data,
		},
		.access = {
			.match = {
				SENSOR_TYPE_ACCELEROMETER,
				SENSOR_TYPE_MAGNETIC_FIELD,
			},
			.m_nr = 2,
		},
	},
	.compass = {
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
			.init      = ecompass_init,
			.activate  = ecompass_activate,
			.set_delay = ecompass_delay,
			.close     = ecompass_close,
		},
	},
	.magnetometer = {
		.sensor = {
			.name       = "ST magnetometer",
			.vendor     = "ST Microelectronic",
			.version    = sizeof(sensors_event_t),
			.handle     = SENSOR_MAGNETIC_FIELD_HANDLE,
			.type       = SENSOR_TYPE_MAGNETIC_FIELD,
			.maxRange   = 8.2,
			.resolution = 0.1,
			.power      = 1,
		},
		.api = {
			.init      = ecompass_init,
			.activate  = ecompass_activate,
			.set_delay = ecompass_delay,
			.close     = ecompass_close,
		},
	},
};

list_constructor(st_ecompass_register);
void st_ecompass_register()
{
	sensors_list_register(&engine.compass.sensor,
				&engine.compass.api);
	sensors_list_register(&engine.magnetometer.sensor,
				&engine.magnetometer.api);
}

static int ecompass_init(struct sensor_api_t *s)
{
	int formation = 0;

	if (!engine.init) {
		engine.init = 1;

		engine.init_ret = compass_API_Init(LSM303DLH_H_8_1G,
					engine.num_formations);
		if (engine.init_ret) {
			ALOGE("%s: Failed in API_Init, status %d", __func__,
					engine.init_ret);
			return engine.init_ret;
		}
		compass_API_ChangeFormFactor(formation);

		engine.init_ret = sensors_wrapper_init(&engine.ecompass.api);
		if (engine.init_ret < 0)
			ALOGE("%s: init failed", __func__);
	}

	return engine.init_ret;
}

static int ecompass_activate(struct sensor_api_t *s, int enable)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
	int sensor;

	if (d->sensor.type == SENSOR_TYPE_ORIENTATION)
		sensor = SENSOR_TYPE_ORIENTATION_BIT;
	else
		sensor = SENSOR_TYPE_MAGNETIC_FIELD_BIT;

	if (enable) {
		if(engine.enable_mask > 0) {
			engine.enable_mask |= (1 << sensor);
			return 0;
		}
		engine.enable_mask |= (1 << sensor);
	} else {
		if ((engine.enable_mask & ~(1 << sensor)) != 0 ) {
			engine.enable_mask &= ~(1 << sensor);
			return 0;
		}
		engine.enable_mask &= ~(1 << sensor);
	}

	return sensors_wrapper_activate(&engine.ecompass.api, enable);
}

static int ecompass_delay(struct sensor_api_t *s, int64_t ns)
{
	/* compass should run on at least 25ms according to STM */
	if (ns > COMPASS_DELAY)
		ns = COMPASS_DELAY;

	return sensors_wrapper_set_delay(&engine.ecompass.api, ns);
}

static void ecompass_close(struct sensor_api_t *s)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);

	if (engine.enable_mask == 0)
		sensors_wrapper_close(&engine.ecompass.api);
}

inline static int compass_status(int accuracy)
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

static void ecompass_data(struct sensor_api_t *s, struct sensor_data_t *sd)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
	sensors_event_t data;
	int accuracy = -1;
	int rc;

	if (sd->sensor->type == SENSOR_TYPE_ACCELEROMETER) {
		rc = compass_API_SaveAcc(sd->data[AXIS_X], sd->data[AXIS_Y],
						sd->data[AXIS_Z]);
		if (rc) {
			ALOGE("%s: compass_API_SaveAcc, error %d\n",
				__func__, rc);
			return ;
		}
	} else if (sd->sensor->type == SENSOR_TYPE_MAGNETIC_FIELD) {
		rc = compass_API_SaveMag(sd->data[AXIS_X], sd->data[AXIS_Y],
						sd->data[AXIS_Z]);
		if (rc) {
			ALOGE("%s: compass_API_SaveMag, error %d\n",
				__func__, rc);
			return ;
		}
	} else {
		ALOGE("%s: unknown sensor %s\n", __func__, sd->sensor->name);
		return ;
	}

	/* only run compass lib on new mag valeus */
	if (sd->sensor->type != SENSOR_TYPE_MAGNETIC_FIELD)
		return ;

	rc = compass_API_Run();
	if (rc) {
		ALOGE("%s: compass_API_Run, error %d\n", __func__, rc);
		return ;
	}
	memset(&data, 0, sizeof(data));
	rc = compass_API_OrientationValues(&data);
	if (rc) {
		ALOGE("%s: compass_API_OrientationValues, error %d\n",
			__func__, rc);
		return ;
	}
	accuracy = compass_API_GetCalibrationGodness();

	if (engine.enable_mask & (1 << SENSOR_TYPE_ORIENTATION_BIT)) {
		data.timestamp = get_current_nano_time();
		data.sensor = engine.compass.sensor.handle;
		data.version = engine.compass.sensor.version;
		data.type = engine.compass.sensor.type;
		data.orientation.status = compass_status(accuracy);
		sensors_fifo_put(&data);
	}
	if (engine.enable_mask & (1 << SENSOR_TYPE_MAGNETIC_FIELD_BIT)) {
		CalibFactor CalibrationData;
		data.timestamp = get_current_nano_time();
		data.sensor = engine.magnetometer.sensor.handle;
		data.version = engine.magnetometer.sensor.version;
		data.type = engine.magnetometer.sensor.type;
		data.magnetic.status = sd->status;
		getCalibrationData(&CalibrationData);
		data.magnetic.x = (sd->data[AXIS_X] -
			CalibrationData.magOffX) * sd->scale;
		data.magnetic.y = (sd->data[AXIS_Y] -
			CalibrationData.magOffY) * sd->scale;
		data.magnetic.z = (sd->data[AXIS_Z] -
			CalibrationData.magOffZ) * sd->scale;
		sensors_fifo_put(&data);
	}
}
