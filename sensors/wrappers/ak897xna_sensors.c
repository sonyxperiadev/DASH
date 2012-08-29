#define LOG_TAG "DASH - ak897x_sensors - wrapper"

#include <stdlib.h>
#include <string.h>
#include "sensors_log.h"
#include <linux/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <pthread.h>
#include <errno.h>
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_select.h"
#include "sensor_util.h"
#include "sensors_id.h"
#include "sensors_config.h"
#include "sensors_wrapper.h"
#include "sensor_xyz.h"
#include "libs/akm8972/SEMC_APIs.h"

#define SETTING_FILE_NAME "/data/misc/akm_set.txt"

#if defined(AK8975)
#define CLASS_DEVICE_NAME akm8975
#elif defined(AK8972)
#define CLASS_DEVICE_NAME akm8972
#else
#error No AKM chip is defined
#endif

#define TO_STRING_(x) #x
#define TO_STRING(x) TO_STRING_(x)

char const * const SYSFS_REGISTERS_FILE_NAME = TO_STRING( /sys/class/compass/CLASS_DEVICE_NAME/registers );

#define ACC_SENSITIVITY 256
#define AK897X_MAXFORM 2
#define MAG_LAYOUT_ROW 3
#define MAG_LAYOUT_CLM 3
#define AKM_CALIBRATION_THRESHOLD 300
#define FRC_TRIGGER 4096
#define CLIENT_DELAY_UNUSED NO_RATE

static int threshold_counter = 0;

enum {
	LAYOUT_11,
	LAYOUT_12,
	LAYOUT_13,
	LAYOUT_21,
	LAYOUT_22,
	LAYOUT_23,
	LAYOUT_31,
	LAYOUT_32,
	LAYOUT_33,
	NUM_LAYOUT
};

enum {
	MAGNETIC,
	ORIENTATION,
	NUMSENSORS
};

struct config_record {
	int max;
	int min;
	int size;
	int *store;
};

struct akm_t {
	/* ak897x internal sensor */
	char *map_prefix;
	int layout[NUM_LAYOUT];
	int enable_mask;
	int init;/* true, if has initialized*/
	int init_ret;
	struct wrapper_desc ak897x;
	/* android sensors */
	struct wrapper_desc magnetic;
	struct wrapper_desc compass;
	int64_t delay_requests[NUMSENSORS];
};

static int ak897x_init(struct sensor_api_t *s);
static int ak897x_activate(struct sensor_api_t *s, int enable);
static int ak897x_delay(struct sensor_api_t *s, int64_t ns);
static void ak897x_close(struct sensor_api_t *s);
static void ak897xna_compass_data(struct sensor_api_t *s, struct sensor_data_t *sd);

struct akm_t akm = {
	.map_prefix = "ak897xmagnetic",
	.layout = {1, 0, 0, 0, 1, 0, 0, 0, 1},
	.enable_mask = 0x00, /* Start with all Android sensors deactivated */
	.init = 0,
	.init_ret = SENSOR_OK,
	.ak897x = {
		.sensor = {
			name: AKM_CHIP_NAME,
			vendor: "Asahi Kasei Corp.",
			version: sizeof(sensors_event_t),
			handle: SENSOR_INTERNAL_HANDLE_MIN,
		},
		.api = {
			.init = ak897x_init,
			.activate = ak897x_activate,
			.set_delay = ak897x_delay,
			.close = ak897x_close,
			.data = ak897xna_compass_data,
		},
		.access = {
			.match = {
				SENSOR_TYPE_ACCELEROMETER,
				SENSOR_TYPE_MAGNETIC_FIELD,
			},
			.m_nr = 2,
		},
	},
	.magnetic = {
		.sensor = {
			name: AKM_CHIP_NAME" Magnetic Field",
			vendor: "Asahi Kasei Corp.",
			version: sizeof(sensors_event_t),
			handle: SENSOR_MAGNETIC_FIELD_HANDLE,
			type: SENSOR_TYPE_MAGNETIC_FIELD,
			maxRange: AKM_CHIP_MAXRANGE,
			resolution: AKM_CHIP_RESOLUTION,
			power: AKM_CHIP_POWER,
			minDelay: 5000,
		},
		.api = {
			.init = ak897x_init,
			.activate = ak897x_activate,
			.set_delay = ak897x_delay,
			.close = ak897x_close,
		},
	},
	.compass = {
		.sensor = {
			name: AKM_CHIP_NAME" Compass",
			vendor: "Asahi Kasei Corp.",
			version: sizeof(sensors_event_t),
			handle: SENSOR_ORIENTATION_HANDLE,
			type: SENSOR_TYPE_ORIENTATION,
			maxRange: 360,
			resolution: 100,
			power: 0.8,
			minDelay: 5000,
		},
		.api = {
			.init = ak897x_init,
			.activate = ak897x_activate,
			.set_delay = ak897x_delay,
			.close = ak897x_close,
		},
	},
	.delay_requests = {
		CLIENT_DELAY_UNUSED,
		CLIENT_DELAY_UNUSED,
	},
};

static int ak897x_read_layout(char *prefix, char *key, struct config_record *cr)
{
	int rc = -1;
	int tmp[NUM_LAYOUT];

	if (!sensors_config_get_key(prefix, key, TYPE_ARRAY_INT, tmp, cr->size)) {
		int i;
		for (i = 0; i < cr->size; i++) {
			if (tmp[i] < cr->min || tmp[i] > cr->max)
				break;
		}
		if (i == cr->size) {
			memcpy(cr->store, tmp, sizeof(tmp));
			rc = 0;
		} else {
			ALOGE("%s: bad config (%s_%s) [%2d %2d %2d],[%2d %2d %2d],[%2d %2d %2d]",
				 __func__, prefix, key,
				 tmp[LAYOUT_11], tmp[LAYOUT_12], tmp[LAYOUT_13],
				 tmp[LAYOUT_21], tmp[LAYOUT_22], tmp[LAYOUT_23],
				 tmp[LAYOUT_31], tmp[LAYOUT_32], tmp[LAYOUT_33]);
		}
	} else {
		ALOGE("%s: failed to read %s_%s", __func__, prefix, key);
	}
	return rc;
}

static void ak897x_read_sensor_layout(struct akm_t *d)
{
	struct config_record rec;

	if (!sensors_have_config_file()) {
		ALOGI("%s: No config file found. Using default config.",
			 __func__);
		return;
	}
	rec.size = NUM_LAYOUT;
	rec.min = -1;
	rec.max = 1;
	rec.store = d->layout;
	ak897x_read_layout(d->map_prefix, "layout", &rec);

	ALOGD("%s: %s: layout [%2d %2d %2d],[%2d %2d %2d],[%2d %2d %2d]",
		 __func__, d->ak897x.sensor.name,
		 d->layout[LAYOUT_11], d->layout[LAYOUT_12], d->layout[LAYOUT_13],
		 d->layout[LAYOUT_21], d->layout[LAYOUT_22], d->layout[LAYOUT_23],
		 d->layout[LAYOUT_31], d->layout[LAYOUT_32], d->layout[LAYOUT_33]);
}

static int ak897x_read_regs(register_map_ak897x *regs)
{
	int fd;
	int err;
	unsigned char reg_buf[13];

	fd = open(SYSFS_REGISTERS_FILE_NAME, O_RDONLY);

	if (fd < 0) {
		err = -errno;
		ALOGE("%s: ecompass: failed to open registers"
				" errno:%d %s\n", __func__, err, strerror(err));
		goto err_exit;
	}
	err = read(fd, &reg_buf, sizeof(reg_buf));
	close(fd);
	if (err < 0) {
		err = -errno;
		ALOGE("%s: ecompass: failed to perform "
				"read_registers errno:%d %s\n",
				__func__, err, strerror(err));
		goto err_exit;
	}

	regs->WIA = reg_buf[0];
	regs->INFO = reg_buf[1];
	regs->ST1 = reg_buf[2];
	regs->HXL = reg_buf[3];
	regs->HXH = reg_buf[4];
	regs->HYL = reg_buf[5];
	regs->HYH = reg_buf[6];
	regs->HZL = reg_buf[7];
	regs->HZH = reg_buf[8];
	regs->ST2 = reg_buf[9];
	regs->ASAX = reg_buf[10];
	regs->ASAY = reg_buf[11];
	regs->ASAZ = reg_buf[12];

err_exit:
	return err;
}

static int ak897x_init(struct sensor_api_t *s)
{
	register_map_ak897x regs;
	int16_t mag_layout[MAG_LAYOUT_ROW][MAG_LAYOUT_CLM] = {{1,0,0},{0,1,0},{0,0,1}};
	int i;
	int j;
	int k = 0;

	if (!akm.init) {
		akm.init = 1;
		akm.init_ret = sensors_wrapper_init(&akm.ak897x.api);
		if (akm.init_ret < 0) {
			ALOGE("%s: init failed", __func__);
			return akm.init_ret;
		}
		ak897x_read_regs(&regs);
		ak897x_read_sensor_layout(&akm);

		for (i = 0; i < MAG_LAYOUT_ROW; i++) {
			for (j = 0; j < MAG_LAYOUT_CLM; j++) {
				mag_layout[i][j] = akm.layout[k];
				k++;
			}
		}

		akm.init_ret = AKM_Init(AK897X_MAXFORM, &regs,
				(const int16_t (*)[MAG_LAYOUT_CLM])mag_layout);
		if (akm.init_ret)
			ALOGE("%s: AKM_Init Error !\n", __func__);
	}

	return akm.init_ret;
}

static int ak897x_activate(struct sensor_api_t *s, int enable)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
	int ret;
	int sensor = -1;

	switch(d->sensor.handle) {
		case SENSOR_MAGNETIC_FIELD_HANDLE: sensor = MAGNETIC; break;
		case SENSOR_ORIENTATION_HANDLE: sensor = ORIENTATION; break;
	}

	if (enable) {
		if (akm.enable_mask > 0) {
			akm.enable_mask |= (1 << sensor);
			return 0;
		}
		akm.enable_mask |= (1 << sensor);
		AKM_Start(SETTING_FILE_NAME);
	} else {
		ret = ak897x_delay(s, CLIENT_DELAY_UNUSED);
		if (ret)
			ALOGE("%s: Error %s delay failed", __func__, d->sensor.name);
		if ((akm.enable_mask & ~(1 << sensor)) != 0 ) {
			akm.enable_mask &= ~(1 << sensor);
			return 0;
		}
		akm.enable_mask &= ~(1 << sensor);
		ret = AKM_Stop(SETTING_FILE_NAME);
		if (ret)
			ALOGE("%s: AKM_Stop Error !\n", __func__);
	}
	ALOGV("%s: %s '%s' by '%s'", __func__, enable ? "enable" : "disable",
		akm.ak897x.sensor.name, d->sensor.name);
	return sensors_wrapper_activate(&akm.ak897x.api, enable);
}

static int ak897x_delay(struct sensor_api_t *s, int64_t ns)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int i, sensor = -1;
	int64_t delay_tmp = CLIENT_DELAY_UNUSED;

	switch(d->sensor.handle) {
		case SENSOR_MAGNETIC_FIELD_HANDLE: sensor = MAGNETIC; break;
		case SENSOR_ORIENTATION_HANDLE: sensor = ORIENTATION; break;
		default: return sensor;
	}

	akm.delay_requests[sensor] = ns;

	for (i = 0; i < NUMSENSORS; i++) {
		if ((akm.enable_mask & (1 << i)) &&
			(akm.delay_requests[i] != CLIENT_DELAY_UNUSED)) {
			if ((delay_tmp == CLIENT_DELAY_UNUSED) ||
			    (delay_tmp > akm.delay_requests[i]))
				delay_tmp = akm.delay_requests[i];
		}
	}
	return sensors_wrapper_set_delay(&akm.ak897x.api, delay_tmp);
}

static void ak897x_close(struct sensor_api_t *s)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);

	if (akm.enable_mask == 0) {
		ALOGV("%s: '%s' by '%s'", __func__, akm.ak897x.sensor.name,
			d->sensor.name);
		AKM_Release();
		sensors_wrapper_close(&akm.ak897x.api);
	}
}

static int ak897x_form(void)
{
	/* TODO: implement form factor */
	return AKM_ChangeFormFactor(0);
}

static void ak897xna_compass_data(struct sensor_api_t *s, struct sensor_data_t *sd)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
	sensors_event_t data;
	int err;
	unsigned int cal;

	memset(&data, 0, sizeof(data));

	if (sd->sensor->type == SENSOR_TYPE_ACCELEROMETER) {
		err = AKM_SaveAcc(sd->data[AXIS_X], sd->data[AXIS_Y], sd->data[AXIS_Z],
					ACC_SENSITIVITY);
		if (err)
			ALOGE("%s: AKM_SaveAcc Error !\n", __func__);
	}

	if (sd->sensor->type == SENSOR_TYPE_MAGNETIC_FIELD) {
		cal = AKM_GetCalibrationGoodness();
		if (AKM_CALIBRATION_THRESHOLD < cal) {
			threshold_counter++;
			if (threshold_counter > FRC_TRIGGER) {
				AKM_ForceReCalibration();
				ALOGD("%s: calibration: rating = %d,"
					"force recalibration.\n", __func__, cal);
				threshold_counter = 0;
			}
		}

		err = AKM_SaveMag(sd->data[AXIS_X],
			  sd->data[AXIS_Y],
			  sd->data[AXIS_Z],
			  sd->status,
			  sd->delay);
		if (err)
			ALOGE("%s: AKM_Save_Mag Error !\n", __func__);

		data.timestamp = sd->timestamp;

		if (akm.enable_mask & (1 << MAGNETIC)) {
			err = AKM_GetMagneticValues(&data);
			if (err)
				ALOGE("%s: AKM_AKM_GetMagneticValues Error !\n", __func__);

			data.version = akm.magnetic.sensor.version;
			data.sensor = akm.magnetic.sensor.handle;
			data.type = akm.magnetic.sensor.type;
			sensors_fifo_put(&data);
			ALOGV("%s:mag x=%f, y=%f, z=%f", __func__,
			     data.magnetic.x, data.magnetic.y, data.magnetic.z);
		}
		if (akm.enable_mask & (1 << ORIENTATION)) {
			err = AKM_GetOrientationValues(&data);
			if (err)
				ALOGE("%s: AKM_GetOrientationValues Error !\n", __func__);
			data.version = akm.compass.sensor.version;
			data.sensor = akm.compass.sensor.handle;
			data.type = akm.compass.sensor.type;
			sensors_fifo_put(&data);
			ALOGV("%s: x=%f, y=%f, z=%f", __func__, data.orientation.azimuth,
			     data.orientation.pitch, data.orientation.roll);
		}
	}
}

list_constructor(ak897xna_register);
void ak897xna_register()
{
	(void)sensors_list_register(&akm.magnetic.sensor, &akm.magnetic.api);
	(void)sensors_list_register(&akm.compass.sensor, &akm.compass.api);
}

