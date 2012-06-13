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

#ifndef SENSORS_CONFIG_H_
#define SENSORS_CONFIG_H_

enum config_type_t {
	TYPE_STRING,
	TYPE_ARRAY_INT,
	TYPE_INT
};

int sensors_have_config_file();
int sensors_config_read(char* filename);
int sensors_config_get_key(char* prefix, char* key, enum config_type_t type,
			   void *out_value, int out_size);
void sensors_config_destroy();

#endif
