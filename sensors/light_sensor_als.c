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
#include "als_input.h"

static int light_init(struct sensor_api_t *s);
static int light_activate(struct sensor_api_t *s, int enable);
static int light_set_delay(struct sensor_api_t *s, int64_t ns);
static void light_close(struct sensor_api_t *s);

struct sensor_desc {
	struct sensors_worker_t worker;
	struct sensor_t sensor;
	struct sensor_api_t api;
	int64_t delay;

};

static struct sensor_desc light_sensor = {
	.sensor = {
		.name = "Light sensor",
		.vendor = "Sony Mobile",
		.version = sizeof(sensors_event_t),
		.handle = SENSOR_LIGHTSENSOR_HANDLE,
		.type = SENSOR_TYPE_LIGHT,
		.maxRange = LIGHT_RANGE,
		.resolution = 1.0,
		.power = 1
	},
	.api = {
		.init = light_init,
		.activate = light_activate,
		.set_delay = light_set_delay,
		.close = light_close
	}
};

static void *light_poll(void *arg)
{
	struct sensor_desc *d = arg;
	float val;
	sensors_event_t data;

	if (als_getlux_filtered(&val))
		return NULL;

	memset(&data, 0, sizeof(data));

	data.light = val;
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

	sensors_worker_init(&d->worker, light_poll, d);
	return 0;
}

static int light_activate(struct sensor_api_t *s, int enable)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	if (enable) {
		als_enable();
		d->worker.resume(&d->worker);
	} else {
		d->worker.suspend(&d->worker);
		als_disable();
	}

	return 0;
}

static int light_set_delay(struct sensor_api_t *s, int64_t ns)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	d->delay = ns;
	d->worker.set_delay(&d->worker, ns);

	return 0;
}

static void light_close(struct sensor_api_t *s)
{
	struct sensor_desc *d = container_of(s, struct sensor_desc, api);

	d->worker.destroy(&d->worker);
}

list_constructor(light_init_driver);
void light_init_driver()
{
	(void)sensors_list_register(&light_sensor.sensor, &light_sensor.api);
}
