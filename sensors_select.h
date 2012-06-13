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

#ifndef SENSORS_SELECT_H_
#define SENSORS_SELECT_H_
#include "sensors_worker.h"

struct sensors_select_t {
	void (*suspend)(struct sensors_select_t* s);
	void (*resume)(struct sensors_select_t* s);
	void (*set_delay)(struct sensors_select_t* s, int64_t ns);
	void (*destroy)(struct sensors_select_t* s);
	void (*set_fd)(struct sensors_select_t* s, int fd);
	int (*get_fd)(struct sensors_select_t* s);
	void* (*select_callback)(void* arg);

	struct sensors_worker_t worker;
	int ctl_fds[2];
	int fd;
	pthread_mutex_t fd_mutex;
	void *arg;
	int64_t delay;
};

void sensors_select_init(struct sensors_select_t* s,
			void* (*select_func)(void *arg), void* arg, int fd);
#endif
