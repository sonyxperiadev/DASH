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

#define LOG_TAG "DASH - worker"

#include "sensors_log.h"
#include <time.h>
#include <string.h>
#include <errno.h>
#include "sensors_worker.h"

static void sensor_nano_sleep(int64_t utime)
{
	struct timespec t;

	if (utime <= 0)
		return;

	t.tv_sec = (time_t)(utime / 1e9);
	t.tv_nsec = (long)(utime % 1000000000L);
	nanosleep(&t, NULL);
}

static void *sensors_worker_internal_worker(void *arg)
{
	struct sensors_worker_t* worker = (struct sensors_worker_t*) arg;
	enum sensors_worker_mode mode;

	while (1) {
		pthread_mutex_lock(&worker->mode_mutex);
		mode = worker->mode;
		pthread_mutex_unlock(&worker->mode_mutex);

		switch (mode) {
		case SENSOR_DESTROY:
			goto exit;

		case SENSOR_SLEEP:
			pthread_mutex_lock(&worker->mode_mutex);
			pthread_cond_wait(&worker->suspend_cond, &worker->mode_mutex);
			pthread_mutex_unlock(&worker->mode_mutex);
			break;

		default:
			worker->poll_callback(worker->arg);
			break;
		}
		if (worker->delay_ns)
			sensor_nano_sleep(worker->delay_ns);
	}
exit:
	return NULL;
}

static void sensors_worker_set_delay(struct sensors_worker_t* worker, int64_t ns)
{
	pthread_mutex_lock(&worker->mode_mutex);
	worker->delay_ns = ns;
	pthread_mutex_unlock(&worker->mode_mutex);
}

static void sensors_worker_suspend(struct sensors_worker_t* worker)
{
	enum sensors_worker_mode prev_mode;

	pthread_mutex_lock(&worker->mode_mutex);
	prev_mode = worker->mode;
	worker->mode = SENSOR_SLEEP;
	pthread_mutex_unlock(&worker->mode_mutex);
}

static void sensors_worker_resume(struct sensors_worker_t* worker)
{
	enum sensors_worker_mode prev_mode;

	pthread_mutex_lock(&worker->mode_mutex);
	prev_mode = worker->mode;
	worker->mode = SENSOR_RUNNING;

	if (prev_mode == SENSOR_SLEEP)
		pthread_cond_broadcast(&worker->suspend_cond);

	pthread_mutex_unlock(&worker->mode_mutex);
}

static void sensors_worker_destroy(struct sensors_worker_t* worker)
{
	enum sensors_worker_mode prev_mode;

	pthread_mutex_lock(&worker->mode_mutex);
	prev_mode = worker->mode;
	worker->mode = SENSOR_DESTROY;

	if (prev_mode == SENSOR_SLEEP)
		pthread_cond_broadcast(&worker->suspend_cond);

	pthread_mutex_unlock(&worker->mode_mutex);
	pthread_join(worker->worker_thread_id, NULL);
}

void sensors_worker_init(struct sensors_worker_t* worker,
			void* (*work_func)(void *arg), void* arg)
{
	worker->mode = SENSOR_SLEEP;

	worker->poll_callback = work_func;
	worker->suspend = sensors_worker_suspend;
	worker->resume = sensors_worker_resume;
	worker->destroy = sensors_worker_destroy;
	worker->set_delay = sensors_worker_set_delay;
	worker->delay_ns = 200000000L;
	worker->arg = arg;

	pthread_mutex_init (&worker->mode_mutex, NULL);
	pthread_cond_init (&worker->suspend_cond, NULL);
	pthread_create(&worker->worker_thread_id, NULL,
		       sensors_worker_internal_worker, (void*) worker);
}

