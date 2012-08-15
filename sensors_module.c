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

#define LOG_TAG "DASH - module"

#include <stdio.h>
#include "sensors_log.h"
#include <stdlib.h>
#include <string.h>
#include "sensors_list.h"
#include "sensors_config.h"
#include "sensors_fifo.h"

static int sensors_module_set_delay(struct sensors_poll_device_t *dev,
				    int handle, int64_t ns)
{
	int ret;
	struct sensor_api_t* api = sensors_list_get_api_from_handle(handle);

	if (!api) {
		ALOGE("%s: unable to find handle!", __func__);
                return -1;
        }

	ret = api->set_delay(api, ns);

	return ret;
}

static int sensors_module_activate(struct sensors_poll_device_t *dev,
				   int handle, int enabled)
{
	struct sensor_api_t* api = sensors_list_get_api_from_handle(handle);

	if (!api) {
		ALOGE("%s: unable to find handle!", __func__);
                return -1;
        }

	if (api->activate(api, enabled) < 0)
		return -1;

	return 0;
}

static int sensors_module_poll(struct sensors_poll_device_t *dev,
			       sensors_event_t* data, int count)
{
	int ret;

	while ((ret = sensors_fifo_get_all(data, count)) == 0)
		;

	return ret;
}

static int sensors_module_close(struct hw_device_t* device)
{
	sensors_fifo_deinit();
	sensors_config_destroy();
	free(device);
	return 0;
}

static int sensors_init_iterator(struct sensor_api_t* api, void *arg)
{
	return api->init(api);
}

static int sensors_module_open(const struct hw_module_t* module, const char* id, struct hw_device_t** device)
{
	struct sensors_poll_device_t *dev;

	if (strcmp(id, SENSORS_HARDWARE_POLL))
		return 0;

	dev = malloc(sizeof(*dev));
	if (!dev)
		return -1;

	memset(dev, 0, sizeof(*dev));
	dev->common.tag = HARDWARE_DEVICE_TAG;
	dev->common.version = 0;
	dev->common.module = (struct hw_module_t*)module;
	dev->common.close = sensors_module_close;
	dev->activate = sensors_module_activate;
	dev->setDelay = sensors_module_set_delay;
	dev->poll = sensors_module_poll;

	*device = (struct hw_device_t*) dev;

	sensors_config_read(NULL);
	sensors_fifo_init();
	sensors_list_foreach_api(sensors_init_iterator, NULL);

	return 0;
}

struct hw_module_methods_t sensors_module_methods = {
	open: sensors_module_open
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
	common: {
		tag: HARDWARE_MODULE_TAG,
		version_major: 1,
		version_minor: 0,
		id: SENSORS_HARDWARE_MODULE_ID,
		name : "dash",
		author : "oskar.andero@sonymobile.com",
		methods: &sensors_module_methods,
	},
	get_sensors_list: sensors_list_get
};

