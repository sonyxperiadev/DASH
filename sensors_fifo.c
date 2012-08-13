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
#define LOG_TAG "DASH - fifo"

#include "sensors_log.h"
#include <pthread.h>
#include "sensors_fifo.h"

#define FIFO_LEN 8

static struct sensors_fifo_t {
	pthread_mutex_t mutex;
	pthread_cond_t data_cond;

	int fifo_i;
	sensors_event_t fifo[FIFO_LEN];
} sensors_fifo;

void sensors_fifo_init()
{
	pthread_mutex_init(&sensors_fifo.mutex, NULL);
	pthread_cond_init(&sensors_fifo.data_cond, NULL);
}

void sensors_fifo_deinit()
{
	pthread_mutex_destroy(&sensors_fifo.mutex);
}

void sensors_fifo_put(sensors_event_t *data)
{
	pthread_mutex_lock(&sensors_fifo.mutex);

	if (sensors_fifo.fifo_i < FIFO_LEN)
		sensors_fifo.fifo[sensors_fifo.fifo_i++] = *data;

	pthread_cond_broadcast(&sensors_fifo.data_cond);
	pthread_mutex_unlock(&sensors_fifo.mutex);
}

int sensors_fifo_get_all(sensors_event_t *data, int len)
{
	int i;

	/* This function deliberately drops all packets above len. */
	pthread_mutex_lock(&sensors_fifo.mutex);
	pthread_cond_wait(&sensors_fifo.data_cond, &sensors_fifo.mutex);

	for (i = 0; (i < sensors_fifo.fifo_i) && (i < len); i++)
		data[i] = sensors_fifo.fifo[i];
	sensors_fifo.fifo_i = 0;
	pthread_mutex_unlock(&sensors_fifo.mutex);

	return i;
}

