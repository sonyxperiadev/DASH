#ifndef SENSORS_INPUT_CACHE_H_
#define SENSORS_INPUT_CACHE_H_

#define INPUT_EVENT_DIR      "/dev/input/"
#define INPUT_EVENT_BASENAME "event"
#define INPUT_EVENT_PATH     INPUT_EVENT_DIR INPUT_EVENT_BASENAME
#define MAX_INT_STRING_SIZE  sizeof("4294967295")

struct sensors_input_cache_entry_t {
	int nr;
	char dev_name[32];
	char event_path[sizeof(INPUT_EVENT_PATH) + MAX_INT_STRING_SIZE];
};

const struct sensors_input_cache_entry_t *sensors_input_cache_get(
							const char *name);

#endif
