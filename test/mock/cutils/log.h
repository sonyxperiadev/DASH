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
#ifndef LOG_H_
#define LOG_H_
#include <stdio.h>

#ifndef LOG_TAG
#define LOG_TAG
#endif

#ifndef VERBOSE
#define VERBOSE 0
#endif

#if VERBOSE
#define LOGD(x...) { printf(LOG_TAG ": " x); printf("\n"); }
#define LOGI(x...) { printf(LOG_TAG ": " x); printf("\n"); }
#define LOGE(x...) { printf(LOG_TAG ": " x); printf("\n"); }
#define LOGV(x...) { printf(LOG_TAG ": " x); printf("\n"); }
#define LOGW(x...) { printf(LOG_TAG ": " x); printf("\n"); }
#else
#define LOGD(x...)
#define LOGI(x...)
#define LOGE(x...)
#define LOGV(x...)
#define LOGW(x...)
#endif

#endif
