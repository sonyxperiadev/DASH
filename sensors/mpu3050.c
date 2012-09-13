#define LOG_TAG "DASH - mpu3050"

#include <stdlib.h>
#include <string.h>
#include <linux/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <pthread.h>
#include <errno.h>
#include "sensors_log.h"
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_select.h"
#include "sensor_util.h"
#include "sensors_id.h"
#include "sensors_config.h"

#include "sensors.h"
#include "MPLWrapper.h"

#define SENSORS_ROTATION_VECTOR  (1<<ID_RV)
#define SENSORS_LINEAR_ACCEL     (1<<ID_LA)
#define SENSORS_GRAVITY          (1<<ID_GR)
#define SENSORS_GYROSCOPE        (1<<ID_GY)
#define SENSORS_ACCELERATION     (1<<ID_A)
#define SENSORS_MAGNETIC_FIELD   (1<<ID_M)
#define SENSORS_ORIENTATION      (1<<ID_O)

#define SENSORS_ROTATION_VECTOR_HANDLE  ID_RV
#define SENSORS_LINEAR_ACCEL_HANDLE     ID_LA
#define SENSORS_GRAVITY_HANDLE          ID_GR
#define SENSORS_GYROSCOPE_HANDLE        ID_GY
#define SENSORS_ACCELERATION_HANDLE     ID_A
#define SENSORS_MAGNETIC_FIELD_HANDLE   ID_M
#define SENSORS_ORIENTATION_HANDLE      ID_O

#define NUMBER_OF_SENSORTYPE 16

static void *the_object;
static int numSensors;

struct sensor_desc {
	struct sensor_t sensor;
	struct sensor_api_t api;

	void *handle;
	int active;
};

struct mpu3050_sensor_composition {
	struct sensor_desc rotation_v;
	struct sensor_desc linear_accl;
	struct sensor_desc gravity;
	struct sensor_desc gyro;
	struct sensor_desc orientation;
	struct sensor_desc accelerometer;
	struct sensor_desc magnetic;
	struct sensors_worker_t sensor_worker;

	int64_t delay;
	pthread_mutex_t lock;
	int mpu_initialized;
	int mpu_activated;
	int fd;
};

static int mpu3050_activate(struct sensor_api_t *s, int enable)
{
	int32_t handle;
	int fd;

	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	struct mpu3050_sensor_composition *sc = d->handle;
	handle = d->sensor.handle;

	call_enable(the_object, handle, enable);

	d->active = enable;
	if (enable && !sc->mpu_activated) {
		fd = call_open_fd(the_object);
		sc->mpu_activated = 1;
		sc->fd = fd;
		sc->sensor_worker.resume(&sc->sensor_worker);
	} else if (!enable && sc->mpu_activated) {
		if (!sc->rotation_v.active &&
		    !sc->linear_accl.active &&
		    !sc->gravity.active &&
		    !sc->accelerometer.active &&
		    !sc->orientation.active &&
		    !sc->magnetic.active &&
		    !sc->gyro.active ) {
			sc->mpu_activated = 0;
			sc->sensor_worker.suspend(&sc->sensor_worker);
			sc->fd = -1;
		}
	}

	return 0;
}

static int mpu3050_set_delay(struct sensor_api_t *s, int64_t ns)
{
	int32_t handle;

	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	struct mpu3050_sensor_composition *sc = d->handle;
	handle = d->sensor.handle;

	sc->delay = ns;
	sc->sensor_worker.set_delay(&sc->sensor_worker, ns);

	return call_setdelay(the_object, handle, ns);
}

static void *mpu3050_read(void *arg)
{
	int ret;
	int i;

	struct mpu3050_sensor_composition *sc = arg;
	sensors_event_t data[numSensors];

	ret = call_readEvents(the_object, data, NUMBER_OF_SENSORTYPE);
	for (i = 0; i < ret; i++) {
		sensors_fifo_put(&data[i]);
	}

	return NULL;
}

static int mpu3050_init(struct sensor_api_t *s_api)
{
	int32_t handle;
	int ret;
	struct stat file_stat;

	struct sensor_desc *d = container_of(s_api, struct sensor_desc, api);
	struct mpu3050_sensor_composition *sc = d->handle;

	handle = d->sensor.handle;

	if (!sc->mpu_initialized) {
		sc->mpu_initialized = 1;
		sensors_worker_init(&sc->sensor_worker, mpu3050_read, sc);
		the_object = new_object();
		numSensors = get_numSensors(the_object);
	}

	return 0;
}

static void mpu3050_close(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	struct mpu3050_sensor_composition *sc = d->handle;

	if (sc->mpu_initialized) {
		sc->mpu_initialized = 0;
		call_close(the_object);
	}
}

static struct mpu3050_sensor_composition mpu3050_Gyro = {
	.rotation_v = {
		.sensor = {
			name: "DASH MPL Rotation vector",
			vendor: "Invensense",
			version: 1,
			handle: SENSORS_ROTATION_VECTOR_HANDLE,
			type: SENSOR_TYPE_ROTATION_VECTOR,
			maxRange: 10240.0f,
			resolution: 1.0f,
			power: 0.5f,
			minDelay: 5000,
			reserved: { },
		},
		.api = {
			init: mpu3050_init,
			activate: mpu3050_activate,
			set_delay: mpu3050_set_delay,
			close: mpu3050_close
		},
	},
	.accelerometer = {
		.sensor = {
			name: "DASH MPL Accelerometer",
			vendor: "Invensense",
			version: 1,
			handle: SENSORS_ACCELERATION_HANDLE,
			type: SENSOR_TYPE_ACCELEROMETER,
			maxRange: 10240.0f,
			resolution: 1.0f,
			power: 0.5f,
			minDelay: 5000,
			reserved: { },
		},
		.api = {
			init: mpu3050_init,
			activate: mpu3050_activate,
			set_delay: mpu3050_set_delay,
			close: mpu3050_close
		},
	},
	.linear_accl = {
		.sensor = {
			name: "DASH MPL Linear acceleration",
			vendor: "Invensense",
			version: 1,
			handle: SENSORS_LINEAR_ACCEL_HANDLE,
			type: SENSOR_TYPE_LINEAR_ACCELERATION,
			maxRange: 10240.0f,
			resolution: 1.0f,
			power: 0.5f,
			minDelay: 5000,
			reserved: { },
		},
		.api = {
			init: mpu3050_init,
			activate: mpu3050_activate,
			set_delay: mpu3050_set_delay,
			close: mpu3050_close
		},
	},
	.orientation = {
		.sensor = {
			name: "DASH MPL Orientation",
			vendor: "Invensense",
			version: 1,
			handle: SENSORS_ORIENTATION_HANDLE,
			type: SENSOR_TYPE_ORIENTATION,
			maxRange: 360.0f,
			resolution: 1.0f,
			power: 9.7f,
			minDelay: 5000,
			reserved: { },
		},
		.api = {
			init: mpu3050_init,
			activate: mpu3050_activate,
			set_delay: mpu3050_set_delay,
			close: mpu3050_close
		},
	},
	.magnetic = {
		.sensor = {
			name: "DASH MPL Magnetic Field",
			vendor: "Invensense",
			version: 1,
			handle: SENSORS_MAGNETIC_FIELD_HANDLE,
			type: SENSOR_TYPE_MAGNETIC_FIELD,
			maxRange: 10240.0f,
			resolution: 1.0f,
			power: 0.5f,
			minDelay: 5000,
			reserved: { },
		},
		.api = {
			init: mpu3050_init,
			activate: mpu3050_activate,
			set_delay: mpu3050_set_delay,
			close: mpu3050_close
		},
	},
	.gravity = {
		.sensor = {
			name: "DASH MPL Gravity",
			vendor: "Invensense",
			version: 1,
			handle: SENSORS_GRAVITY_HANDLE,
			type: SENSOR_TYPE_GRAVITY,
			maxRange: 10240.0f,
			resolution: 1.0f,
			power: 0.5f,
			minDelay: 5000,
			reserved: { },
		},
		.api = {
			init: mpu3050_init,
			activate: mpu3050_activate,
			set_delay: mpu3050_set_delay,
			close: mpu3050_close
		},
	},
	.gyro = {
		.sensor = {
			name: "DASH MPL Gyro",
			vendor: "Invensense",
			version: 1,
			handle: SENSORS_GYROSCOPE_HANDLE,
			type: SENSOR_TYPE_GYROSCOPE,
			maxRange: 10240.0f,
			resolution: 1.0f,
			power: 0.5f,
			minDelay: 5000,
			reserved: { },
		},
		.api = {
			init: mpu3050_init,
			activate: mpu3050_activate,
			set_delay: mpu3050_set_delay,
			close: mpu3050_close
		},
	},
	.mpu_initialized = 0,
	.mpu_activated = 0,
};

static void mpu3050_gyro_register(struct mpu3050_sensor_composition *sc)
{
	pthread_mutex_init(&sc->lock, NULL);

	sc->rotation_v.handle = sc->linear_accl.handle =
		sc->gravity.handle = sc->gyro.handle = sc->orientation.handle =
		sc->accelerometer.handle = sc->magnetic.handle = sc;

	sensors_list_register(&sc->rotation_v.sensor, &sc->rotation_v.api);
	sensors_list_register(&sc->linear_accl.sensor, &sc->linear_accl.api);
	sensors_list_register(&sc->gravity.sensor, &sc->gravity.api);
	sensors_list_register(&sc->gyro.sensor, &sc->gyro.api);
	sensors_list_register(&sc->orientation.sensor, &sc->orientation.api);
	sensors_list_register(&sc->accelerometer.sensor, &sc->accelerometer.api);
	sensors_list_register(&sc->magnetic.sensor, &sc->magnetic.api);
}

list_constructor(mpu3050_gyro_init_driver);
void mpu3050_gyro_init_driver()
{
	mpu3050_gyro_register(&mpu3050_Gyro);
}
