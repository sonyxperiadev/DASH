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

#include <cutils/log.h>

#ifndef SENSORS_LOG_H_
#define SENSORS_LOG_H_

/*
 * LOGV has been replaced by ALOGV from Jelly Bean. This little quirk
 * adds backwards compatibility.
 */
#if !defined(ALOGV) && defined(LOGV)
#define ALOGV LOGV
#define ALOGV_IF LOGV_IF
#define ALOGD LOGD
#define ALOGD_IF LOGD_IF
#define ALOGI LOGI
#define ALOGI_IF LOGI_IF
#define ALOGW LOGW
#define ALOGW_IF LOGW_IF
#define ALOGE LOGE
#define ALOGE_IF LOGE_IF
#endif

#endif
