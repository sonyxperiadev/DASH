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
	int len;

	count = snprintf(sysfs_path, sizeof(sysfs_path), "%s/%s",
			 s->data.path, attribute);
	if ((count < 0) || (count >= (int)sizeof(sysfs_path))) {
		ALOGE("%s: snprintf failed! %d\n", __func__, count);
		return -1;
	}

	sysfs_fd = open(sysfs_path, O_RDWR);
	if (sysfs_fd < 0) {
		ALOGE("%s: open failed! %d\n", __func__, sysfs_fd);
		return sysfs_fd;
	}

	len = write(sysfs_fd, value, length);
	close(sysfs_fd);
	if (len < 0)
		ALOGE("%s: write failed! %d\n", __func__, len);

	return len;
}

static int sensors_sysfs_write_int(struct sensors_sysfs_t* s, const char* attribute,
			       const long long value) {
	char buf[32];
	int count;

	count = snprintf(buf, sizeof(buf), "%lld", value);
	if ((count < 0) || (count >= (int)sizeof(buf))) {
		ALOGE("%s: snprintf failed! %d\n", __func__, count);
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
			ALOGE("%s: sensors_input_cache_get failed!\n",
			      __func__);
			return -1;
		}
		count = snprintf(s->data.path, sizeof(s->data.path), "%s%d",
				 input_class_path, input->nr);
		if ((count < 0) || (count >= (int)sizeof(s->data.path))) {
			ALOGE("%s: snprintf failed! %d\n", __func__, count);
			return -1;
		}
		break;

	case SYSFS_TYPE_ABS_PATH:
		strlcpy(s->data.path, str, sizeof(s->data.path));
		break;

	default:
		ALOGE("%s: unknown type %d\n", __func__, type);
		return -1;
	}

	s->write = sensors_sysfs_write;
	s->write_int = sensors_sysfs_write_int;

	return 0;
}
