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

#ifndef SENSOR_WORKER_H_
#define SENSOR_WORKER_H_
#include <stdint.h>
#include <pthread.h>

enum sensors_worker_mode {
	SENSOR_NO_INIT,
	SENSOR_SLEEP,
	SENSOR_RUNNING,
	SENSOR_DESTROY
};

struct sensors_worker_t {
	enum sensors_worker_mode mode;
	pthread_mutex_t	mode_mutex;

	pthread_cond_t suspend_cond;
	pthread_t worker_thread_id;

	void *arg;
	int64_t delay_ns;

	void (*suspend)(struct sensors_worker_t* worker);
	void (*resume)(struct sensors_worker_t* worker);
	void (*set_delay)(struct sensors_worker_t* worker, int64_t ns);
	void (*destroy)(struct sensors_worker_t* worker);
	void* (*poll_callback)(void* arg);
};

void sensors_worker_init(struct sensors_worker_t* worker,
			void* (*work_func)(void *arg), void* arg);

#endif
