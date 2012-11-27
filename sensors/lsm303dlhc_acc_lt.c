#include "sensor_xyz.h"

static struct sensor_desc accelerometer = {
	.sensor = {
		.name       = "lsm303dlhc acceleration",
		.vendor     = "ST Microelectronic",
		.version    = sizeof(sensors_event_t),
		.handle     = SENSOR_ACCELEROMETER_HANDLE,
		.type       = SENSOR_TYPE_ACCELEROMETER,
		.maxRange   = 16,
		.resolution = 0.1,
		.power      = 1,
	},
	.api = {
		.init      = sensor_xyz_init,
		.activate  = sensor_xyz_activate,
		.set_delay = sensor_xyz_set_delay,
		.close     = sensor_xyz_close,
	},
	.map_prefix = "lsm303dlhc-acceleration",
	.map   = {0, 1, 2},
	.sign  = {1, 1, 1},
	.scale = 1.0f / 1000.f, /* GRAVITY_EARTH applied in wrapper */
	.read = sensor_xyz_read,
	.input_name = "lsm303dlhc_acc_lt",
	.find_input = find_input_dev,
	.dev_name = "lsm303dlhc_acc_lt",
	.dev_attr_rate_ms = "pollrate_ms",
	.ev_type_data = EV_ABS,
	.ev_type_sync = EV_SYN,
	.ev_code = {ABS_X, ABS_Y, ABS_Z},
};

list_constructor(lsm303dlhx_accelerometer_lt);
void lsm303dlhx_accelerometer_lt()
{
	sensors_wrapper_register(&accelerometer.sensor,
				&accelerometer.api,
				&accelerometer.entry);
}
