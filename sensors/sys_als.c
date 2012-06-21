#define LOG_TAG "DASH - sysals"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <lights/illumination_api.h>
#include "sensors_log.h"
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_select.h"
#include "sensor_util.h"
#include "sensors_id.h"

static int als_init(struct sensor_api_t *s);
static int als_activate(struct sensor_api_t *s, int enable);
static int als_set_delay(struct sensor_api_t *s, int64_t ns);
static void als_close(struct sensor_api_t *s);
static void *als_read(void *arg);

struct sensor_desc {
	struct sensors_select_t select_worker;
	struct sensor_t sensor;
	struct sensor_api_t api;
	char *name;
};

static struct sensor_desc light_sensor = {
	.sensor = {
		.name = "Light sensor input",
		.vendor = "Sony Mobile",
		.version = sizeof(sensors_event_t),
		.handle = SENSOR_LIGHTSENSOR_HANDLE,
		.type = SENSOR_TYPE_LIGHT,
		.maxRange = LIGHT_RANGE,
		.resolution = 1.0,
		.power = 1
	},
	.api = {
		.init = als_init,
		.activate = als_activate,
		.set_delay = als_set_delay,
		.close = als_close
	},
	.name = SYS_ALS_DEV_NAME,
};

static int als_init(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	sensors_select_init(&d->select_worker, als_read, s, -1);
	return 0;
}

static int als_activate(struct sensor_api_t *s, int enable)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int fd = d->select_worker.get_fd(&d->select_worker);

	if (enable && (fd < 0)) {
		fd = open_input_dev_by_name(d->name, O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			ALOGE("%s: failed to open input dev %s, error: %s\n",
				__func__, d->name, strerror(errno));
			return -1;
		}
		if (!sysals_activate()) {
			d->select_worker.set_fd(&d->select_worker, fd);
			d->select_worker.resume(&d->select_worker);
		} else {
			close(fd);
		}
	} else if (!enable && (fd > 0)) {
		d->select_worker.set_fd(&d->select_worker, -1);
		d->select_worker.suspend(&d->select_worker);
		sysals_deactivate();
	}
	return 0;
}

static int als_set_delay(struct sensor_api_t *s, int64_t ns)
{
	return 0;
}

static void als_close(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	d->select_worker.destroy(&d->select_worker);
}

static void *als_read(void *arg)
{
	struct sensor_api_t *s = arg;
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);
	int bytes, i;
	struct input_event event[2];
	int fd = d->select_worker.get_fd(&d->select_worker);
	sensors_event_t data;

	bytes = read(fd, event, sizeof(event));
	if (bytes < 0) {
		ALOGE("%s: read failed, error: %d\n", __func__, bytes);
		return NULL;
	}
	for (i = 0; i < (bytes / (int)sizeof(struct input_event)); i++) {
		switch (event[i].type) {
		case EV_MSC:
			if (event[i].code != MSC_RAW)
				break;
			memset(&data, 0, sizeof(data));
			data.light = event[i].value;
			data.version = light_sensor.sensor.version;
			data.sensor = light_sensor.sensor.handle;
			data.type = light_sensor.sensor.type;
			data.timestamp = get_current_nano_time();
			sensors_fifo_put(&data);
			break;
		default:
			break;
		}
	}

	return NULL;
}

list_constructor(als_init_driver);
void als_init_driver()
{
	(void)sensors_list_register(&light_sensor.sensor, &light_sensor.api);
}
