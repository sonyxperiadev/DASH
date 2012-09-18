#ifndef SENSORS_SYSFS_H_
#define SENSORS_SYSFS_H_

enum sensors_sysfs_type {
	SYSFS_TYPE_ABS_PATH,
	SYSFS_TYPE_INPUT_DEV
};

#define SYSFS_PATH_MAX 64
struct sysfs_data_t {
	char path[SYSFS_PATH_MAX];
};

struct sensors_sysfs_t {
	int (*write)(struct sensors_sysfs_t* s, const char* attribute,
		     const char *value, const int length);
	int (*write_int)(struct sensors_sysfs_t* s, const char* attribute,
			 const long long value);

	struct sysfs_data_t data;
};

int sensors_sysfs_init(struct sensors_sysfs_t* s, const char *str,
		       enum sensors_sysfs_type type);
#endif

