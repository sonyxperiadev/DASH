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

#ifndef SENSORS_ID_H_
#define SENSORS_ID_H_

#define SENSOR_ORIENTATION_HANDLE            0
#define SENSOR_ACCELEROMETER_HANDLE          1
#define SENSOR_TEMPERATURE_HANDLE            2
#define SENSOR_MAGNETIC_FIELD_HANDLE         3
#define SENSOR_LIGHT_HANDLE                  4
#define SENSOR_PROXIMITY_HANDLE              5
#define SENSOR_TRICORDER_HANDLE              6
#define SENSOR_LIGHTSENSOR_HANDLE            SENSOR_TRICORDER_HANDLE
#define SENSOR_ORIENTATION_RAW_HANDLE        7
#define SENSOR_PRESSURE_HANDLE               8
#define SENSOR_GRAVITY_HANDLE                9
#define SENSOR_LINEAR_ACCELERATION_HANDLE   10
#define SENSOR_ROTATION_VECTOR_HANDLE       11
#define SENSOR_GYROSCOPE_HANDLE             12
#define SENSOR_ORIENTATION_2_HANDLE         13
/* range for sensors not exposed to android */
#define SENSOR_INTERNAL_HANDLE_MIN         100
#define SENSOR_INTERNAL_HANDLE_MAX         110

#endif
