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

#define LOG_TAG "DASH - config"

#include <stdio.h>
#include "sensors_log.h"
#include <stdlib.h>
#include <string.h>
#include "sensors_config.h"

#define PRIMARY_CONFIG "/etc/dash.conf"
#define SECONDARY_CONFIG "/etc/sensors.conf"

#define MAX_STR_LEN 64
struct config_entry_t {
	char prefix[MAX_STR_LEN];
	char key[MAX_STR_LEN];
	char value[MAX_STR_LEN];
	struct config_entry_t *next;
};

static struct config_entry_t *config_list;

static void insert_entry(struct config_entry_t *entry)
{
	struct config_entry_t *p = config_list;

	if (!config_list) {
		config_list = entry;
		config_list->next = NULL;
		return;
	}

	while (p) {
		if (!p->next) {
			p->next = entry;
			p->next->next = NULL;
			return;
		}
		p = p->next;
	}
}

static int parse_value(char *value, enum config_type_t type, void *out_value,
		       unsigned int out_size)
{
	switch (type) {
	default:
		return -1;

	case TYPE_STRING:
	{
		unsigned int bytes;
		bytes = strlen(value) + 1;
		if (out_size < bytes)
			return -1;

		memcpy(out_value, value, bytes);
		break;
	}

	case TYPE_ARRAY_INT:
	{
		char *token;
		char *saveptr;
		unsigned int i = 0;
		char restore[MAX_STR_LEN];
		unsigned int bytes;
		bytes = strlen(value) + 1;
		if (MAX_STR_LEN < bytes)
			return -1;

		memcpy(restore, value, bytes);
		token = strtok_r(value, ",", &saveptr);
		while (token) {
			((int*)out_value)[i] = atoi(token);
			if (++i > out_size) {
				memcpy(value, restore, bytes);
				return -1;
			}
			token = strtok_r(NULL, ",", &saveptr);
		}
		memcpy(value, restore, bytes);
		break;
	}

	case TYPE_INT:
		if (out_size < sizeof(int))
			return -1;

		*((int*)out_value) = atoi(value);
		break;
	}
	return 0;
}

int sensors_config_read(char* filename)
{
	FILE *fp;
	char buf[64];
	int retval = 0;

	if (filename) {
		fp = fopen(filename, "r");
	} else {
		fp = fopen(PRIMARY_CONFIG, "r");
		if (!fp)
			fp = fopen(SECONDARY_CONFIG, "r");
	}

	if (!fp) {
		ALOGE("%s: unable to open config file", __func__);
		return -1;
	}

	while (fgets(buf, sizeof(buf), fp)) {
		struct config_entry_t *new_entry;

		if (buf[0] == '#')
			continue;

		new_entry = malloc(sizeof(*new_entry));

		if (!new_entry) {
			retval = -1;
			goto exit;
		}

		/* Arguments have length MAX_STR_LEN=64 */
		if (sscanf(buf, "%64[^_]_%64[^ =]%*[ =]"
			"%64[^\n]", new_entry->prefix,
			new_entry->key, new_entry->value) == 3) {
			insert_entry(new_entry);
		} else if (sscanf(buf, "%64[^ \n]", new_entry->prefix) == 1) {
			ALOGE("Parse error: %s", buf);
			free(new_entry);
			sensors_config_destroy();
			retval = -1;
		} else {
			free(new_entry);
		}

	}

exit:
	fclose(fp);

	return retval;
}

int sensors_have_config_file()
{
	return (config_list != NULL);
}

int sensors_config_get_key(char* prefix, char* key, enum config_type_t type,
			   void *out_value, int out_size)
{
	struct config_entry_t *p = config_list;

	while (p) {
		if ((strncmp(p->prefix, prefix, strlen(p->prefix)) == 0) &&
		    (strncmp(p->key, key, strlen(p->key)) == 0)) {
		    return parse_value(p->value, type, out_value, out_size);
		}
		p = p->next;
	}
	return -1;
}

void sensors_config_destroy()
{
	struct config_entry_t *p = config_list;
	struct config_entry_t *p_next;

	while (p) {
		p_next = p->next;
		free(p);
		p = p_next;
	}
}

