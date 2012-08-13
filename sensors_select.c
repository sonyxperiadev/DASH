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

#define LOG_TAG "DASH - select"

#include <unistd.h>
#include <string.h>
#include "sensors_log.h"
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "sensors_select.h"

extern pthread_mutex_t wrapper_mutex;

#define LOCK(p) do { \
	ALOGV("%s(%d): %s: lock\n", __FILE__, __LINE__, __func__); \
	pthread_mutex_lock(p); \
} while (0)

#define UNLOCK(p) do { \
	ALOGV("%s(%d): %s: unlock\n", __FILE__, __LINE__, __func__); \
	pthread_mutex_unlock(p); \
} while (0)

static void *sensors_select_callback(void *arg)
{
	fd_set readfds;
	int ret;
	struct sensors_select_t *s = arg;
	int maxfd;

	LOCK(&s->fd_mutex);
	maxfd = s->ctl_fds[0] > s->fd ? s->ctl_fds[0] : s->fd;
	FD_ZERO(&readfds);
	FD_SET(s->ctl_fds[0], &readfds);
	FD_SET(s->fd, &readfds);
	UNLOCK(&s->fd_mutex);
	ret = select(maxfd + 1, &readfds, NULL, NULL, NULL);

	if (ret < 0) {
		ALOGE("%s: select failed!\n", __func__);
	} else if (ret) {
		if (FD_ISSET(s->ctl_fds[0], &readfds)) {
			read(s->ctl_fds[0], &ret, sizeof(ret));
		} else if (FD_ISSET(s->fd, &readfds)) {
			LOCK(&wrapper_mutex);
			LOCK(&s->fd_mutex);
			if (s->fd >= 0)
			    s->select_callback(s->arg);
			UNLOCK(&s->fd_mutex);
			UNLOCK(&wrapper_mutex);
		}
	}
	return NULL;
}

static void scheduler_select_notify(struct sensors_select_t* s)
{
	int rc = write(s->ctl_fds[1], &rc, sizeof (rc));
	if (rc < 0)
		ALOGE("%s: write failed: %s", __func__, strerror(errno));
}

static void sensors_select_set_delay(struct sensors_select_t* s, int64_t ns)
{
	s->delay = ns;
}

static void sensors_select_suspend(struct sensors_select_t* s)
{
	s->worker.suspend(&s->worker);
	scheduler_select_notify(s);
}

static void sensors_select_resume(struct sensors_select_t* s)
{
	s->worker.resume(&s->worker);
	scheduler_select_notify(s);
}

static void sensors_select_destroy(struct sensors_select_t* s)
{
	s->worker.destroy(&s->worker);
	LOCK(&s->fd_mutex);
	if (s->fd > 0) {
		close(s->fd);
		s->fd = -1;
	}
	if (s->ctl_fds[0] > 0)
		close(s->ctl_fds[0]);
	if (s->ctl_fds[1] > 0)
		close(s->ctl_fds[1]);
	UNLOCK(&s->fd_mutex);
}

void sensors_select_set_fd(struct sensors_select_t* s, int fd)
{
	LOCK(&s->fd_mutex);
	if (s->fd > 0)
		close(s->fd);
	s->fd = fd;
	UNLOCK(&s->fd_mutex);
	scheduler_select_notify(s);
}

int sensors_select_get_fd(struct sensors_select_t* s)
{
	return s->fd;
}

void sensors_select_init(struct sensors_select_t* s,
			void* (*select_func)(void *arg), void* arg, int fd)
{
	s->suspend = sensors_select_suspend;
	s->resume = sensors_select_resume;
	s->destroy = sensors_select_destroy;
	s->set_delay = sensors_select_set_delay;
	s->set_fd = sensors_select_set_fd;
	s->get_fd = sensors_select_get_fd;
	s->select_callback = select_func;
	s->arg = arg;
	s->fd = fd;
	s->delay = 0;

        if (pipe(s->ctl_fds) < 0)
		ALOGE("%s: pipe failed: %s", __func__, strerror(errno));

	sensors_worker_init(&s->worker, sensors_select_callback, s);
	s->worker.set_delay(&s->worker, 0);
	pthread_mutex_init(&s->fd_mutex, NULL);
}

