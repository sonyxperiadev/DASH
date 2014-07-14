/*
 * Copyright (C) 2012-2014 Sony Mobile Communications AB.
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

#define LOG_TAG "DASH"

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include "sensors_log.h"
#include "sensors_input_cache.h"
#include "sensors_sysfs.h"

static const char *input_class_path = "/sys/class/input/input";

static int sensors_sysfs_write(struct sensors_sysfs_t* s, const char* attribute,
			       const char *value, const int length)
{
	char sysfs_path[SYSFS_PATH_MAX];
	int sysfs_fd;
	int count;
	int ret;

	count = snprintf(sysfs_path, sizeof(sysfs_path), "%s/%s",
			 s->data.path, attribute);
	if ((count < 0) || (count >= (int)sizeof(sysfs_path))) {
		ALOGE("%s: snprintf failed!\n", __func__);
		return -1;
	}

	sysfs_fd = open(sysfs_path, O_RDWR);
	if (sysfs_fd < 0)
		return -errno;

	ret = write(sysfs_fd, value, length);
	if (ret < 0)
		ret = -errno;
	close(sysfs_fd);

	return ret;
}

static int sensors_sysfs_write_int(struct sensors_sysfs_t* s, const char* attribute,
			       const long long value) {
	char buf[32];
	int count;

	count = snprintf(buf, sizeof(buf), "%lld", value);
	if ((count < 0) || (count >= (int)sizeof(buf))) {
		ALOGE("%s: snprintf failed!\n", __func__);
		return -1;
	}

	return sensors_sysfs_write(s, attribute, buf, count);
}

int sensors_sysfs_init(struct sensors_sysfs_t* s, const char *str,
		       enum sensors_sysfs_type type)
{
	const struct sensors_input_cache_entry_t *input;
	int count;

	switch (type) {
	case SYSFS_TYPE_INPUT_DEV:
		input = sensors_input_cache_get(str);
		if (!input) {
			ALOGE("sensors_input_cache_get failed!\n");
			return -1;
		}
		count = snprintf(s->data.path, sizeof(s->data.path), "%s%d",
				 input_class_path, input->nr);
		if ((count < 0) || (count >= (int)sizeof(s->data.path))) {
			ALOGE("%s: snprintf failed!\n", __func__);
			return -1;
		}
		break;

	case SYSFS_TYPE_ABS_PATH:
		strlcpy(s->data.path, str, sizeof(s->data.path));
		break;

	default:
		ALOGE("unknown sysfs type %d for %s\n", type, str);
		return -1;
	}

	s->write = sensors_sysfs_write;
	s->write_int = sensors_sysfs_write_int;

	return 0;
}
