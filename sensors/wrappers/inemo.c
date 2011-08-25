#define LOG_TAG "DASH iNemo - wrapper"

#include <stdlib.h>
#include <string.h>
#include "sensors_log.h"
#include <errno.h>
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_id.h"
#include "sensors_wrapper.h"
#include "sensor_util.h"
#include "iNemoEngineAPI.h"
#include "sensor_xyz.h"

/* Define to switch on/off iNemo orientation sensor*/
#define CONFIG_INEMO_ORIENTATION
#define SCALAR_COMPONENT_W 3

/* functions used by the internal sensor inemo */
static int inemo_init(struct sensor_api_t *s);
static int inemo_activate(struct sensor_api_t *s, int enable);
static int inemo_delay(struct sensor_api_t *s, int64_t ns);
static void inemo_close(struct sensor_api_t *s);
static void inemo_data(struct sensor_api_t *s, struct sensor_data_t *sd);

/* function sends android data after inemo has executed */
static void android_event(struct wrapper_desc *d, float *p, int64_t t);

#define NUMAXES 3
#define QNUMAXES 4
#define LOCALEARTHMAGFIELD 50
#define MAGFULLSCALE 0xE0
#define FORMFACTORNUMBER 0
#define DELTATIME 100
#define UTESLA_TO_MGAUSS 10

typedef struct Output
{
	float quaternion[QNUMAXES];           /* four number hpr system      */
	float gravity[NUMAXES];               /* device frame gravity        */
	float linear_acceleration[NUMAXES];   /* device frame lin accel      */
	float rotation[NUMAXES];              /* heading, pitch and roll     */
} Output;

enum {
	GRAVITY = 0,
	LINEAR_ACCELERATION = 1,
	ORIENTATION = 2,
	ROTATION_VECTOR = 3,
	MAGNETIC = 4,
	Numsensors
};

struct inemoengine_t {
	/* Inemo specific */
	RawCounts    data;
	Output       output;
	/* local control */
	int magnetic_status;
	int enable_mask;
	int init;/* true, if has initialized*/
	int init_ret;
	unsigned char acc_data_exist;
	unsigned char mag_data_exist;
	unsigned char gyr_data_exist;
	/* inemo internal sensor */
	int64_t rate_ns;
	struct wrapper_desc inemo;
	/* inemo android sensors */
	struct wrapper_desc gravity;
	struct wrapper_desc linear_acceleration;
	struct wrapper_desc rotation_vector;
	struct wrapper_desc orientation;
	struct wrapper_desc magnetic;
};

struct inemoengine_t inemoengine = {
	.enable_mask = 0x00, /* Start with all Android sensors deactivated */
	.init = 0,
	.init_ret = SENSOR_OK,
	.rate_ns = 10000000, /* always run all releated sensors in 100Hz */
	.inemo = {
		.sensor = {
			.name       = "iNemo",
			.vendor     = "ST Microelectronic",
			.version    = sizeof(sensors_event_t),
			.handle     = SENSOR_INTERNAL_HANDLE_MIN,
		},
		.api = {
			.init      = inemo_init,
			.activate  = inemo_activate,
			.set_delay = inemo_delay,
			.close     = inemo_close,
			.data      = inemo_data,
		},
		.access = {
			.match = {
				SENSOR_TYPE_ACCELEROMETER,
				SENSOR_TYPE_MAGNETIC_FIELD,
				SENSOR_TYPE_GYROSCOPE,
			},
			.m_nr = 3,
		},
	},
	.gravity = {
		.sensor = {
			.name       = "iNemo Gravity",
			.vendor     = "ST Microelectronic",
			.version    = sizeof(sensors_event_t),
			.handle     = SENSOR_GRAVITY_HANDLE,
			.type       = SENSOR_TYPE_GRAVITY,
			.maxRange   = 9.81,
			.resolution = 0.000001,
			.power      = 1,
		},
		.api = {
			.init      = inemo_init,
			.activate  = inemo_activate,
			.set_delay = inemo_delay,
			.close     = inemo_close,
		},
	},
	.linear_acceleration = {
		.sensor = {
			.name       = "iNemo Linear acceleration",
			.vendor     = "ST Microelectronic",
			.version    = sizeof(sensors_event_t),
			.handle     = SENSOR_LINEAR_ACCELERATION_HANDLE,
			.type       = SENSOR_TYPE_LINEAR_ACCELERATION,
			.maxRange   = 9.81,
			.resolution = 0.000001,
			.power      = 6,
		},
		.api = {
			.init      = inemo_init,
			.activate  = inemo_activate,
			.set_delay = inemo_delay,
			.close     = inemo_close,
		},
	},
	.rotation_vector = {
			.sensor = {
				.name       = "iNemo Rotation vector",
				.vendor     = "ST Microelectronic",
				.version    = sizeof(sensors_event_t),
				.handle     = SENSOR_ROTATION_VECTOR_HANDLE,
				.type       = SENSOR_TYPE_ROTATION_VECTOR,
				.maxRange   = 1,
				.resolution = 0.000001,
				.power      = 6,
			},
			.api = {
				.init      = inemo_init,
				.activate  = inemo_activate,
				.set_delay = inemo_delay,
				.close     = inemo_close,
			},
		},
	.orientation = {
			.sensor = {
				.name       = "iNemo Orientation",
				.vendor     = "ST Microelectronic",
				.version    = sizeof(sensors_event_t),
				.handle     = SENSOR_ORIENTATION_2_HANDLE,
				.type       = SENSOR_TYPE_ORIENTATION,
				.maxRange   = 360,
				.resolution = 1,
				.power      = 6,
			},
			.api = {
				.init      = inemo_init,
				.activate  = inemo_activate,
				.set_delay = inemo_delay,
				.close     = inemo_close,
			},
		},
	.magnetic = {
			.sensor = {
				.name       = "iNemo Magnetometer",
				.vendor     = "ST Microelectronic",
				.version    = sizeof(sensors_event_t),
				.handle     = SENSOR_MAGNETIC_FIELD_HANDLE,
				.type       = SENSOR_TYPE_MAGNETIC_FIELD,
				.maxRange   = 8.2,
				.resolution = 0.1,
				.power      = 1,
			},
			.api = {
				.init      = inemo_init,
				.activate  = inemo_activate,
				.set_delay = inemo_delay,
				.close     = inemo_close,
			},
		},
};

list_constructor(st_inemo_register);
void st_inemo_register()
{
	sensors_list_register(&inemoengine.gravity.sensor,
				&inemoengine.gravity.api);
	sensors_list_register(&inemoengine.linear_acceleration.sensor,
				&inemoengine.linear_acceleration.api);
	sensors_list_register(&inemoengine.rotation_vector.sensor,
				&inemoengine.rotation_vector.api);
	sensors_list_register(&inemoengine.orientation.sensor,
				&inemoengine.orientation.api);
	sensors_list_register(&inemoengine.magnetic.sensor,
				&inemoengine.magnetic.api);
}

static int inemo_init(struct sensor_api_t *s)
{
	if (!inemoengine.init) {
		inemoengine.init = 1;

		inemoengine.init_ret = iNemoEngineAPI_Initialization(
			LOCALEARTHMAGFIELD, MAGFULLSCALE, FORMFACTORNUMBER);

		if (inemoengine.init_ret < 0) {
			ALOGE("%s: iNemoEngineAPI failed", __func__);
			return inemoengine.init_ret;
		}

		inemoengine.init_ret = sensors_wrapper_init(
					&inemoengine.inemo.api);
		if (inemoengine.init_ret < 0)
			ALOGE("%s: init failed", __func__);
	}

	return inemoengine.init_ret;
}

static int inemo_activate(struct sensor_api_t *s, int enable)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
	int sensor = -1;

	switch(d->sensor.type) {
		case SENSOR_TYPE_ORIENTATION: sensor = ORIENTATION; break;
		case SENSOR_TYPE_ROTATION_VECTOR: sensor = ROTATION_VECTOR; break;
		case SENSOR_TYPE_LINEAR_ACCELERATION: sensor = LINEAR_ACCELERATION; break;
		case SENSOR_TYPE_GRAVITY: sensor = GRAVITY; break;
		case SENSOR_TYPE_MAGNETIC_FIELD: sensor = MAGNETIC; break;
	}

	if (enable) {
		if(inemoengine.enable_mask > 0) {
			inemoengine.enable_mask |= (1 << sensor);
			return 0;
		}
		inemoengine.enable_mask |= (1 << sensor);
	} else {
		if ((inemoengine.enable_mask & ~(1 << sensor)) != 0 ) {
			inemoengine.enable_mask &= ~(1 << sensor);
			return 0;
		}
		inemoengine.enable_mask &= ~(1 << sensor);
	}

	return sensors_wrapper_activate(&inemoengine.inemo.api, enable);
}

static int inemo_delay(struct sensor_api_t *s, int64_t ns)
{
	/* always set the default rate required by inemo lib */
	return sensors_wrapper_set_delay(&inemoengine.inemo.api, inemoengine.rate_ns);
}

static void inemo_close(struct sensor_api_t *s)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);

	if (inemoengine.enable_mask == 0)
		sensors_wrapper_close(&inemoengine.inemo.api);
}

static void inemo_data(struct sensor_api_t *s, struct sensor_data_t *sd)
{
	int64_t t;

	if (sd->sensor->type == SENSOR_TYPE_ACCELEROMETER) {
		/* store data in G and in NED format (scale = 0.001) */
		inemoengine.data.acc[AXIS_X] = sd->data[AXIS_X] * sd->scale;
		inemoengine.data.acc[AXIS_Y] = sd->data[AXIS_Y] * sd->scale;
		inemoengine.data.acc[AXIS_Z] = sd->data[AXIS_Z] * sd->scale;
		inemoengine.acc_data_exist = 1;
	} else if (sd->sensor->type == SENSOR_TYPE_MAGNETIC_FIELD) {
		inemoengine.data.mag[AXIS_X] = sd->data[AXIS_X] * sd->scale;
		inemoengine.data.mag[AXIS_Y] = sd->data[AXIS_Y] * sd->scale;
		inemoengine.data.mag[AXIS_Z] = sd->data[AXIS_Z] * sd->scale;
		inemoengine.magnetic_status = sd->status;
		inemoengine.mag_data_exist = 1;
	} else if (sd->sensor->type == SENSOR_TYPE_GYROSCOPE) {
		/* store data in DPS and in NED format (scale = 0.070) */
		inemoengine.data.gyro[AXIS_X] = sd->data[AXIS_X] * sd->scale;
		inemoengine.data.gyro[AXIS_Y] = sd->data[AXIS_Y] * sd->scale;
		inemoengine.data.gyro[AXIS_Z] = sd->data[AXIS_Z] * sd->scale;
		inemoengine.gyr_data_exist = 1;
	} else {
		ALOGE("%s: Error, %s is unknown", __func__, sd->sensor->name);
		return ;
	}

	/* run the lib if we have all data we need */
	if (inemoengine.acc_data_exist && inemoengine.mag_data_exist && inemoengine.gyr_data_exist) {
		(void)iNemoEngineAPI_Run(DELTATIME, &inemoengine.data);
		t = get_current_nano_time();

		if (inemoengine.enable_mask & (1 << GRAVITY)) {
			(void) iNemoEngineAPI_Return_Gravity(inemoengine.output.gravity);
			(void)android_event(&inemoengine.gravity, inemoengine.output.gravity, t);
		}
		if (inemoengine.enable_mask & (1 << LINEAR_ACCELERATION)) {
			(void) iNemoEngineAPI_Return_Linear_acceleration(
					inemoengine.output.linear_acceleration);
			(void)android_event(&inemoengine.linear_acceleration,
					inemoengine.output.linear_acceleration, t);
		}
		if(inemoengine.enable_mask & (1 << ROTATION_VECTOR)) {
			(void) iNemoEngineAPI_Return_Quaternion(inemoengine.output.quaternion);
			(void)android_event(&inemoengine.rotation_vector,
					inemoengine.output.quaternion, t);
		}
		if (inemoengine.enable_mask & (1 << ORIENTATION)) {
			(void) iNemoEngineAPI_Return_Rotation(inemoengine.output.rotation);
			android_event(&inemoengine.orientation,
					inemoengine.output.rotation, t);
		}
		if (inemoengine.enable_mask & (1 << MAGNETIC)) {
			CalibFactor Calibration;
			sensors_event_t se;

			iNemoEngineAPI_getCalibrationData(&Calibration);
			se.sensor = inemoengine.magnetic.sensor.handle;
			se.version = inemoengine.magnetic.sensor.version;
			se.type = inemoengine.magnetic.sensor.type;
			se.timestamp = t;
			se.magnetic.status = inemoengine.magnetic_status;
			se.magnetic.x = inemoengine.data.mag[AXIS_X] - Calibration.magOffX/UTESLA_TO_MGAUSS;
			se.magnetic.y = inemoengine.data.mag[AXIS_Y] - Calibration.magOffY/UTESLA_TO_MGAUSS;
			se.magnetic.z = inemoengine.data.mag[AXIS_Z] - Calibration.magOffZ/UTESLA_TO_MGAUSS;
			sensors_fifo_put(&se);
		}

		/* reset */
		inemoengine.acc_data_exist = 0;
		inemoengine.mag_data_exist = 0;
		inemoengine.gyr_data_exist = 0;
	}
}

void android_event(struct wrapper_desc *d, float *p, int64_t t)
{
	sensors_event_t se;
	se.timestamp = t;
	se.sensor = d->sensor.handle;
	se.version = d->sensor.version;
	se.type = d->sensor.type;

	if (se.type == SENSOR_TYPE_GRAVITY) {
		/* change from NED formatted with gravity range -1.0 to 1.0,
		to android formatted with gravity in m/s^2 */
		se.acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;
		se.acceleration.x = p[AXIS_X] * GRAVITY_EARTH;
		se.acceleration.y = p[AXIS_Y] * GRAVITY_EARTH;
		se.acceleration.z = p[AXIS_Z] * GRAVITY_EARTH;
	} else if (se.type == SENSOR_TYPE_LINEAR_ACCELERATION) {
		/* change from NED formatted with gravity range -1.0 to 1.0,
		to android formatted with gravity in m/s^2 */
		se.acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;
		se.acceleration.x = p[AXIS_X] * GRAVITY_EARTH;
		se.acceleration.y = p[AXIS_Y] * GRAVITY_EARTH;
		se.acceleration.z = p[AXIS_Z] * GRAVITY_EARTH;
	} else if (se.type == SENSOR_TYPE_ROTATION_VECTOR) {
		se.data[AXIS_X] = p[AXIS_X];
		se.data[AXIS_Y] = p[AXIS_Y];
		se.data[AXIS_Z] = p[AXIS_Z];
	} else if (se.type == SENSOR_TYPE_ORIENTATION) {
		se.orientation.status = SENSOR_STATUS_ACCURACY_HIGH;
		se.orientation.azimuth = p[AXIS_X];
		se.orientation.pitch =  p[AXIS_Y];
		se.orientation.roll = p[AXIS_Z];
	}
	sensors_fifo_put(&se);
}
