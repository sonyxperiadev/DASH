#define LOG_TAG "DASH - light"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <linux/input.h>
#include <errno.h>
#include "sensors_log.h"
#include "sensors_list.h"
#include "sensors_fifo.h"
#include "sensors_worker.h"
#include "sensor_util.h"
#include "sensors_id.h"

static struct sensor_desc light_sensor;

struct sensor_desc {
	struct sensors_worker_t worker;
	struct sensor_t sensor;
	struct sensor_api_t api;
	int fd;
};


static void *light_poll(void *arg)
{
	struct sensor_desc *d = container_of(arg, struct sensor_desc, worker);
	sensors_event_t data;
	char buf[20];
	ssize_t n;
	int lux;

	memset(&data, 0, sizeof(data));

	n = pread(d->fd, buf, sizeof(buf), 0);

	/*convert to lux value*/
	lux = atof(buf)*12;

	data.light = lux;
	data.version = light_sensor.sensor.version;
	data.sensor = light_sensor.sensor.handle;
	data.type = light_sensor.sensor.type;
	data.timestamp = get_current_nano_time();
	sensors_fifo_put(&data);

	return NULL;
}

static int light_init(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	sensors_worker_init(&d->worker, light_poll, &d->worker);

	return 0;
}

static int light_activate(struct sensor_api_t *s, int enable)
{
	int fd;
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	if (enable) {
		fd = open(ALS_PATH, O_RDONLY);
		if (fd <0) {
			ALOGE("%s: failed to open as3676: %s\n",__func__, ALS_PATH);
			return -1;
		}
		d->fd = fd;
		d->worker.resume(&d->worker);
	} else {
		d->worker.suspend(&d->worker);
		close(d->fd);
		d->fd = -1;
	}

	return 0;
}

static int light_set_delay(struct sensor_api_t *s, int64_t ns)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	d->worker.set_delay(&d->worker, ns);

	return 0;
}

static void light_close(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	d->worker.destroy(&d->worker);
}

static struct sensor_desc light_sensor = {
	.sensor = {
		.name = "AS3676 based light sensor",
		.vendor = "Austria Micro Systems",
		.version = sizeof(sensors_event_t),
		.handle = SENSOR_LIGHTSENSOR_HANDLE,
		.type = SENSOR_TYPE_LIGHT,
		.maxRange = ALS_CHIP_MAXRANGE,
		.resolution = 1.0,
		.power = 1
	},
	.api = {
		.init = light_init,
		.activate = light_activate,
		.set_delay = light_set_delay,
		.close = light_close
	},
	.fd = -1,
};

list_constructor(light_init_driver);
void light_init_driver()
{
	(void)sensors_list_register(&light_sensor.sensor, &light_sensor.api);
}
