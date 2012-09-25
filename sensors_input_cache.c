#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/input.h>
#include "sensors_log.h"
#include "sensor_util.h"
#include "sensor_util_list.h"
#include "sensors_input_cache.h"

#define MAX_EVENT_DRIVERS 100

static pthread_mutex_t util_mutex = PTHREAD_MUTEX_INITIALIZER;

struct input_dev_list {
	struct list_node node;
	struct sensors_input_cache_entry_t entry;
};

static struct list_node head;
static int list_initialized;

static void *close_input_dev_fd(void *arg)
{
	close((int) arg); /* pass by copy */

	return NULL;
}

static struct input_dev_list *lookup(const char *name, const char *path)
{
	struct list_node *member;
	struct input_dev_list *temp;

	for (member = head.n; member != &head; member = member->n) {
		temp = container_of(member, struct input_dev_list, node);
		if (name && !strncmp(temp->entry.dev_name, name,
					sizeof(temp->entry.dev_name) - 1))
			return temp;
		if (path && !strncmp(temp->entry.event_path, path,
					sizeof(temp->entry.event_path) - 1))
			return temp;
	}

	return NULL;
}

const struct sensors_input_cache_entry_t *sensors_input_cache_get(
							const char *name)
{
	int rc;
	int fd;
	DIR *dir;
	struct dirent * item;
	struct list_node *member;
	struct input_dev_list *temp;
	pthread_t id[MAX_EVENT_DRIVERS];
	unsigned int i = 0;
	unsigned int threads = 0;
	const struct sensors_input_cache_entry_t *found = NULL;

	pthread_mutex_lock(&util_mutex);
	if (!list_initialized) {
		node_init(&head);
		list_initialized = 1;
	}

	temp = lookup(name, NULL);
	if (temp) {
		found = &temp->entry;
		goto exit;
	}

	dir = opendir(INPUT_EVENT_DIR);
	if (!dir) {
		ALOGE("%s: error opening '%s'\n", __func__,
				INPUT_EVENT_DIR);
		goto exit;
	}

	while ((item = readdir(dir)) != NULL) {
		if (strncmp(item->d_name, INPUT_EVENT_BASENAME,
		    sizeof(INPUT_EVENT_BASENAME) - 1) != 0) {
			continue;
		}

		temp = (temp ? temp : malloc(sizeof(*temp)));
		if (temp == NULL) {
			ALOGE("%s: malloc error!\n", __func__);
			break;
		}

		/* skip already cached entries */
		snprintf(temp->entry.event_path, sizeof(temp->entry.event_path),
			 "%s%s", INPUT_EVENT_DIR, item->d_name);
		if (lookup(NULL, temp->entry.event_path))
			continue;

		/* make sure we have access */
		fd = open(temp->entry.event_path, O_RDONLY);
		if (fd < 0) {
			ALOGE("%s: cant open %s", __func__,
			     item->d_name);
			continue;
		}

		rc = ioctl(fd, EVIOCGNAME(sizeof(temp->entry.dev_name)),
				temp->entry.dev_name);

		/* close in parallell to optimize boot time */
		pthread_create(&id[threads++], NULL,
				close_input_dev_fd, (void*) fd);

		if (rc < 0) {
			ALOGE("%s: cant get name from  %s", __func__,
					item->d_name);
			continue;
		}

		temp->entry.nr = atoi(item->d_name +
				      sizeof(INPUT_EVENT_BASENAME) - 1);

		node_add(&head, &temp->node);
		if (!found && !strncmp(temp->entry.dev_name, name,
				       sizeof(temp->entry.dev_name) - 1))
			found = &temp->entry;
		temp = NULL;
	}

	closedir(dir);

	for(i = 0; i < threads; ++i)
		pthread_join(id[i], NULL);

exit:
	pthread_mutex_unlock(&util_mutex);
	return found;
}
