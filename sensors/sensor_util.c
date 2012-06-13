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

#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include "sensor_util.h"
#include <dirent.h>
#include <ctype.h>
#include <cutils/log.h>
#include <pthread.h>
#include "sensor_util_list.h"

#define NSEC_PER_SEC 1000000000L
static int64_t timespec_to_ns(const struct timespec *ts)
{
	return ((int64_t) ts->tv_sec * NSEC_PER_SEC) + ts->tv_nsec;
}

int64_t get_current_nano_time()
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return timespec_to_ns(&t);
}

void sensors_nsleep(int64_t ns)
{
	struct timespec t;
	t.tv_sec = 0;
	t.tv_nsec = ns;
	nanosleep(&t, NULL);
}

void sensors_usleep(int us)
{
	struct timespec t;
	t.tv_sec = 0;
	t.tv_nsec = us*1000000;
	nanosleep(&t, NULL);
}

#define INPUT_EVENT_DIR      "/dev/input/"
#define INPUT_EVENT_BASENAME "event"
#define INPUT_EVENT_PATH     INPUT_EVENT_DIR INPUT_EVENT_BASENAME
#define MAX_INT_STRING_SIZE  sizeof("4294967295")
static pthread_mutex_t util_mutex = PTHREAD_MUTEX_INITIALIZER;
struct input_dev_list {
	struct list_node node;
	char event_path[sizeof(INPUT_EVENT_PATH) + MAX_INT_STRING_SIZE];
	char dev_name[32];
};

static struct list_node head;

int input_dev_path_by_name(char *name, char *path, int path_max)
{
	int rc;
	int fd;
	DIR * dir;
	struct dirent * item;
	struct list_node *member;
	struct input_dev_list *temp;
	static int init;

	pthread_mutex_lock(&util_mutex);
	if (!init) {
		init = 1;
		dir = opendir(INPUT_EVENT_DIR);
		if (!dir)
			goto opendir_err;
		node_init(&head);
		while (NULL != (item = readdir(dir))) {
			if (0 != strncmp(item->d_name, INPUT_EVENT_BASENAME,
				sizeof(INPUT_EVENT_BASENAME) - 1)) {
				continue;
			}
			temp = malloc(sizeof(*temp));
			if (temp == NULL) {
				LOGE("%s: malloc error!\n", __func__);
				goto malloc_err;
			}

			node_add(&head, &temp->node);

			snprintf(temp->event_path, sizeof(temp->event_path),
						"%s%s",
						INPUT_EVENT_DIR, item->d_name);
			fd = open(temp->event_path, O_RDONLY);
			if (fd < 0)
				goto open_err;

			rc = ioctl(fd,
				EVIOCGNAME(sizeof(temp->dev_name)),
				temp->dev_name);
			close(fd);
			if (rc < 0)
				goto open_err;
		}
		closedir(dir);
	}
	pthread_mutex_unlock(&util_mutex);

	for (member = head.n; member != &head; member = member->n) {
		temp = container_of(member, struct input_dev_list, node);
		if (!strcmp(temp->dev_name, name)) {
			strncpy(path, temp->event_path, path_max);
			return 0;
		}
	}

	return -1;

malloc_err:
open_err:
	for (member = head.n; member != &head; member = head.n) {
		temp = container_of(member, struct input_dev_list, node);
		node_del(member);
		free(temp);
	}
	closedir(dir);
opendir_err:
	pthread_mutex_unlock(&util_mutex);
	return -1;
}

int open_input_dev_by_name(char *name, int flags)
{
	char fname[sizeof(INPUT_EVENT_PATH) + MAX_INT_STRING_SIZE];

	if (!input_dev_path_by_name(name, fname, sizeof(fname)))
		return open(fname, flags);

	return -1;
}

int open_input_dev_by_name_store_nr(char *name, int flags, char *nr,
							size_t nr_size)
{
	char fname[sizeof(INPUT_EVENT_PATH) + MAX_INT_STRING_SIZE];

	if (!input_dev_path_by_name(name, fname, sizeof(fname))) {
		size_t len = strnlen(fname, sizeof(fname));
		while(iscntrl(fname[len]))
			len--;
		while(isdigit(fname[len]))
			len--;
		nr = strncpy(nr, &fname[len+1], nr_size);
		return open(fname, flags);
	}

	return -1;
}

#define test_bit(bit, array)    (array[(bit) / 8] & (1 << ((bit) % 8)))
#define bit_array_size(bit)     (((bit) + 7) / 8)
int input_dev_path_by_keycode(int type, int code, char *path, int path_max)
{
	uint8_t bits[bit_array_size(KEY_MAX + 1)];
	int rc;
	int fd;
	DIR * dir;
	struct dirent * item;

	dir = opendir(INPUT_EVENT_DIR);
	while (NULL != dir && NULL != (item = readdir(dir))) {
		if (0 != strncmp(item->d_name, INPUT_EVENT_BASENAME,
				sizeof(INPUT_EVENT_BASENAME) - 1)) {
			continue;
		}

		snprintf(path, path_max, "%s%s", INPUT_EVENT_DIR, item->d_name);
		fd = open(path, O_RDONLY);
		if (fd < 0) {
			continue;
		}

		memset(bits, 0, sizeof(bits));
		rc = ioctl(fd, EVIOCGBIT(type, sizeof(bits)), bits);
		close(fd);
		if (rc < 0)
			continue;
		if (test_bit(code, bits)) {
			closedir(dir);
			return 0;
		}
	}
	closedir(dir);

	return -1;
}

int dev_phys_path_by_attr(const char *attr, const char *attr_val,
			const char *base, char *path, int path_max)
{
	char aval[32];
	int rc;
	int notfound = 1;
	int fd;
	DIR * dir;
	struct dirent * item;
	int len = strlen(attr_val);

	dir = opendir(base);
	if (!dir) {
		LOGE("Unable to open '%s'", base);
		return -1;
	}

	while (NULL != (item = readdir(dir))) {
		if (item->d_type != DT_DIR && item->d_type != DT_LNK)
			continue;
		if (item->d_name[0] == '.')
			continue;
		rc = snprintf(path, path_max, "%s/%s/%s",
				base, item->d_name, attr);
		if (rc >= path_max) {
			LOGD("Entry name truncated '%s'", path);
			continue;
		}
		fd = open(path, O_RDONLY);
		if (fd < 0) {
			LOGE("Unable to open '%s'", path);
			continue;
		}
		rc = read(fd, aval, sizeof(aval) - 1);
		close(fd);
		if (rc < 0) {
			LOGE("Unable to read '%s'", path);
			continue;
		}
		aval[rc] = 0;
		notfound = strncmp(aval, attr_val, len);
		if (!notfound) {
			path[strlen(path) - strlen(attr)] = 0;
			LOGD("'%s' = '%s' found on  path '%s'",
					attr, attr_val, path);
			break;
		}
	}
	closedir(dir);

	return notfound;
}
