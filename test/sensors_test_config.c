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
#include <stdio.h>
#include <string.h>
#include "sensors_config.h"

int main()
{
	int ret = 1;
	int out_int = 0;
	char out_str[64];
	int out_array[2];

	printf("Testing sensor config ... ");
	if (sensors_have_config_file() != 0) {
		printf("\n%u: sensors_have_config_file should fail!\n", __LINE__);
		ret = 0;
		goto exit;
	}
	sensors_config_read("./config_test_1");
	if (sensors_have_config_file() == 0) {
		printf("\n%u: sensors_have_config_file should succeed!\n", __LINE__);
		ret = 0;
		goto exit;
	}
	if (sensors_config_get_key("bma150", "version", TYPE_INT, (void*)&out_int, sizeof(out_int)) >= 0) {
		printf("\n%u: sensors_config_get_key should fail!\n", __LINE__);
		ret = 0;
		goto exit;
	}
	if (sensors_config_get_key("akm8973", "version", TYPE_INT, (void*)&out_int, sizeof(out_int)) < 0) {
		printf("\n%u: sensors_config_get_key should succeed!\n", __LINE__);
		ret = 0;
		goto exit;
	}
	if (out_int != 1) {
		printf("\n%u: out_int != 1!\n", __LINE__);
		ret = 0;
		goto exit;
	}
	if (sensors_config_get_key("akm8973", "name", TYPE_STRING, (void*)&out_str, sizeof(out_str)) < 0) {
		printf("\n%u: sensors_config_get_key should succeed!\n", __LINE__);
		ret = 0;
		goto exit;
	}
	if (strcmp(out_str, "Hej") != 0) {
		printf("\n%u: out_str != Hej\n", __LINE__);
		ret = 0;
		goto exit;
	}
	if (sensors_config_get_key("bma150", "axis_x", TYPE_ARRAY_INT, (void*)&out_array, 3) < 0) {
		printf("\n%u: sensors_config_get_key should succeed!\n", __LINE__);
		ret = 0;
		goto exit;
	}
	if ((out_array[0] != 0)	|| (out_array[1] != 0) || (out_array[2] != 1)) {
		printf("\n%u: out_array != 0 0 1\n", __LINE__);
		ret = 0;
		goto exit;
	}
	if (sensors_config_get_key("akm8973", "version", TYPE_INT, (void*)&out_int, sizeof(out_int)-1) >= 0) {
		printf("\n%u: sensors_config_get_key should fail!\n", __LINE__);
		ret = 0;
		goto exit;
	}
	if (sensors_config_get_key("akm8973", "name", TYPE_STRING, (void*)&out_str, strlen("Hej")) >= 0) {
		printf("\n%u: sensors_config_get_key should fail!\n", __LINE__);
		ret = 0;
		goto exit;
	}
	if (sensors_config_get_key("bma150", "axis_x", TYPE_ARRAY_INT, (void*)&out_array, 2) >= 0) {
		printf("\n%u: sensors_config_get_key should fail!\n", __LINE__);
		ret = 0;
		goto exit;
	}

exit:
	printf("%s\n", ret ? "OK" : "FAILED!");
	sensors_config_destroy();
	return 0;
}
