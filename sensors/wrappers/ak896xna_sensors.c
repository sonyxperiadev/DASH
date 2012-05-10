#define LOG_TAG "DASH - ak896xna - wrapper"

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
#include "sensors_wrapper.h"
#include "sensor_xyz.h"

#define PATH_SIZE 44
#define DEV_NAME "compass"
#define SETTING_FILE_NAME "/data/misc/akm_set.txt"
#define PHYS_PATH_BASE "/sys/devices/virtual/input"

#if defined(AK8963)
#include "libs/akm8963/SEMC_APIs.h"
#define register_map_ak896x register_map_ak8963
#else
#error No AKM chip is defined
#endif

#define TO_STRING_(x) #x
#define TO_STRING(x) TO_STRING_(x)

static char sysfs_registers_file_name[PATH_SIZE];

#define ACC_SENSITIVITY 256
#define AK896X_MAXFORM 2
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
	/* ak896x internal sensor */
	char *map_prefix;
	int layout[NUM_LAYOUT];
	int enable_mask;
	int init;/* true, if has initialized*/
	int init_ret;
	struct wrapper_desc ak896x;
	/* android sensors */
	struct wrapper_desc magnetic;
	struct wrapper_desc compass;
	int64_t delay_requests[NUMSENSORS];
};

static int ak896x_init(struct sensor_api_t *s);
static int ak896x_activate(struct sensor_api_t *s, int enable);
static int ak896x_delay(struct sensor_api_t *s, int64_t ns);
static void ak896x_close(struct sensor_api_t *s);
static void ak896xna_compass_data(struct sensor_api_t *s, struct sensor_data_t *sd);

struct akm_t akm = {
	.map_prefix = "ak896xmagnetic",
	.layout = {1, 0, 0, 0, 1, 0, 0, 0, 1},
	.enable_mask = 0x00, /* Start with all Android sensors deactivated */
	.init = 0,
	.init_ret = SENSOR_OK,
	.ak896x = {
		.sensor = {
			name: AKM_CHIP_NAME,
			vendor: "Asahi Kasei Corp.",
			version: sizeof(sensors_event_t),
			handle: SENSOR_INTERNAL_HANDLE_MIN,
		},
		.api = {
			.init = ak896x_init,
			.activate = ak896x_activate,
			.set_delay = ak896x_delay,
			.close = ak896x_close,
			.data = ak896xna_compass_data,
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
			.init = ak896x_init,
			.activate = ak896x_activate,
			.set_delay = ak896x_delay,
			.close = ak896x_close,
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
			.init = ak896x_init,
			.activate = ak896x_activate,
			.set_delay = ak896x_delay,
			.close = ak896x_close,
		},
	},
	.delay_requests = {
		CLIENT_DELAY_UNUSED,
		CLIENT_DELAY_UNUSED,
	},
};

static int ak896x_read_layout(char *prefix, char *key, struct config_record *cr)
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

static void ak896x_read_sensor_layout(struct akm_t *d)
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
	ak896x_read_layout(d->map_prefix, "layout", &rec);

	ALOGD("%s: %s: layout [%2d %2d %2d],[%2d %2d %2d],[%2d %2d %2d]",
		 __func__, d->ak896x.sensor.name,
		 d->layout[LAYOUT_11], d->layout[LAYOUT_12], d->layout[LAYOUT_13],
		 d->layout[LAYOUT_21], d->layout[LAYOUT_22], d->layout[LAYOUT_23],
		 d->layout[LAYOUT_31], d->layout[LAYOUT_32], d->layout[LAYOUT_33]);
}

static int ak896x_read_regs(register_map_ak896x *regs)
{
	int fd;
	int err;
	unsigned char reg_buf[13];
	char buf[PATH_SIZE];

	err = dev_phys_path_by_attr("name", DEV_NAME,
		PHYS_PATH_BASE, buf, sizeof(buf));
	if (err)
		ALOGE("%s: device not found\n", __func__);

	if (snprintf(sysfs_registers_file_name,
		sizeof(sysfs_registers_file_name), "%sregisters", buf) < 0)
		ALOGE("%s: getting device physical path failed (%s)\n",
		     __func__, strerror(errno));

	fd = open(sysfs_registers_file_name, O_RDONLY);

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

static int ak896x_init(struct sensor_api_t *s)
{
	register_map_ak896x regs;
	int16_t mag_layout[MAG_LAYOUT_ROW][MAG_LAYOUT_CLM] = {{1,0,0},{0,1,0},{0,0,1}};
	int i;
	int j;
	int k = 0;

	if (!akm.init) {
		akm.init = 1;
		akm.init_ret = sensors_wrapper_init(&akm.ak896x.api);
		if (akm.init_ret < 0) {
			ALOGE("%s: init failed", __func__);
			return akm.init_ret;
		}
		ak896x_read_regs(&regs);
		ak896x_read_sensor_layout(&akm);

		for (i = 0; i < MAG_LAYOUT_ROW; i++) {
			for (j = 0; j < MAG_LAYOUT_CLM; j++) {
				mag_layout[i][j] = akm.layout[k];
				k++;
			}
		}

		akm.init_ret = AKM_Init(AK896X_MAXFORM, &regs,
				(const int16_t (*)[MAG_LAYOUT_CLM])mag_layout);
		if (akm.init_ret)
			ALOGE("%s: AKM_Init Error !\n", __func__);
	}

	return akm.init_ret;
}

static int ak896x_activate(struct sensor_api_t *s, int enable)
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
		ret = ak896x_delay(s, CLIENT_DELAY_UNUSED);
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

	return sensors_wrapper_activate(&akm.ak896x.api, enable);
}

static int ak896x_delay(struct sensor_api_t *s, int64_t ns)
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

	return sensors_wrapper_set_delay(&akm.ak896x.api, delay_tmp);
}

static void ak896x_close(struct sensor_api_t *s)
{
	struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);

	if (akm.enable_mask == 0) {
		ALOGV("%s: '%s' by '%s'", __func__, akm.ak896x.sensor.name,
			d->sensor.name);
		AKM_Release();
		sensors_wrapper_close(&akm.ak896x.api);
	}
}

static int ak896x_form(void)
{
	/* TODO: implement form factor */
	return AKM_ChangeFormFactor(0);
}

static void ak896xna_compass_data(struct sensor_api_t *s, struct sensor_data_t *sd)
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
		}
		if (akm.enable_mask & (1 << ORIENTATION)) {
			err = AKM_GetOrientationValues(&data);
			if (err)
				ALOGE("%s: AKM_GetOrientationValues Error !\n", __func__);

			data.version = akm.compass.sensor.version;
			data.sensor = akm.compass.sensor.handle;
			data.type = akm.compass.sensor.type;
			sensors_fifo_put(&data);
		}
	}
}

list_constructor(ak896xna_register);
void ak896xna_register()
{
	(void)sensors_list_register(&akm.magnetic.sensor, &akm.magnetic.api);
	(void)sensors_list_register(&akm.compass.sensor, &akm.compass.api);
}

