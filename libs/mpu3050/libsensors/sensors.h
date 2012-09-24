/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef LIBS_MPU_SENSORS_H_
#define LIBS_MPU_SENSORS_H_

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <linux/input.h>

#include <hardware/hardware.h>
#include <hardware/sensors.h>

__BEGIN_DECLS

/*****************************************************************************/

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define ID_MPL_BASE (9)
#ifdef ENABLE_GESTURE_MANAGER
#define ID_G_TAP (ID_MPL_BASE)          /* 0 Tap */
#define ID_G_SHA (ID_G_TAP + 1)         /* 1 Shake */
#define ID_G_YIR (ID_G_SHA + 1)         /* 2 Yaw Image Rotate */
#define ID_G_OR6 (ID_G_YIR + 1)         /* 3 orientation */
#define ID_G_GRN (ID_G_OR6 + 1)         /* 4 grid num */
#define ID_G_GRD (ID_G_GRN + 1)         /* 5 grid change */
#define ID_G_CSG (ID_G_GRD + 1)         /* 6 ctrl sig */
#define ID_G_MOT (ID_G_CSG + 1)         /* 7 motion */
#define ID_G_STP (ID_G_MOT + 1)         /* 8 step */
#define ID_G_SNA (ID_G_STP + 1)         /* 9 snap */

#define ID_RV (ID_G_SNA + 1)
#else
#define ID_RV (ID_MPL_BASE) /* if no gesture manager, RV is the first ID */
#endif

#define ID_LA (ID_RV + 1)
#define ID_GR (ID_LA + 1)
#define ID_GY (ID_GR + 1)
#define ID_A  (ID_GY + 1)
#define ID_M  (ID_A + 1)
#define ID_O  (ID_M + 1)

#define ID_OFFSET ID_MPL_BASE

/*****************************************************************************/

#define AKM_DEVICE_NAME     "/dev/akm8973_aot"

#define EVENT_TYPE_ACCEL_X          REL_Y
#define EVENT_TYPE_ACCEL_Y          REL_X
#define EVENT_TYPE_ACCEL_Z          REL_Z

#define EVENT_TYPE_YAW              REL_RX
#define EVENT_TYPE_PITCH            REL_RY
#define EVENT_TYPE_ROLL             REL_RZ
#define EVENT_TYPE_ORIENT_STATUS    REL_WHEEL

/* For AK8973iB */
#define EVENT_TYPE_MAGV_X           REL_DIAL
#define EVENT_TYPE_MAGV_Y           REL_HWHEEL
#define EVENT_TYPE_MAGV_Z           REL_MISC

#define EVENT_TYPE_PROXIMITY        ABS_DISTANCE
#define EVENT_TYPE_LIGHT            ABS_MISC

#define EVENT_TYPE_GYRO_X           REL_RY
#define EVENT_TYPE_GYRO_Y           REL_RX
#define EVENT_TYPE_GYRO_Z           REL_RZ


/* 720 LSG = 1G */
#define LSG                         (720.0f)
#define NUMOFACCDATA                8

/* conversion of acceleration data to SI units (m/s^2) */
#define RANGE_A                     (2*GRAVITY_EARTH)
#define RESOLUTION_A                (RANGE_A/(256*NUMOFACCDATA))
#define CONVERT_A                   (GRAVITY_EARTH / LSG / NUMOFACCDATA)
#define CONVERT_A_X                 (CONVERT_A)
#define CONVERT_A_Y                 (-CONVERT_A)
#define CONVERT_A_Z                 (-CONVERT_A)

/* conversion of magnetic data to uT units */
#define CONVERT_M                   (1.0f/16.0f)
#define CONVERT_M_X                 (-CONVERT_M)
#define CONVERT_M_Y                 (-CONVERT_M)
#define CONVERT_M_Z                 (-CONVERT_M)

/* conversion of orientation data to degree units */
#define CONVERT_O                   (1.0f/64.0f)
#define CONVERT_O_A                 (CONVERT_O)
#define CONVERT_O_P                 (CONVERT_O)
#define CONVERT_O_R                 (-CONVERT_O)

/* conversion of gyro data to SI units (radian/sec) */
#define RANGE_GYRO                  (2000.0f*(float)M_PI/180.0f)
#define CONVERT_GYRO                ((70.0f / 1000.0f) * ((float)M_PI / 180.0f))
#define CONVERT_GYRO_X              (CONVERT_GYRO)
#define CONVERT_GYRO_Y              (-CONVERT_GYRO)
#define CONVERT_GYRO_Z              (CONVERT_GYRO)

#define SENSOR_STATE_MASK           (0x7FFF)

/*****************************************************************************/

__END_DECLS

#endif
