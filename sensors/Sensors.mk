#
# Accelerometers
#
$(SOMC_CFG_SENSORS_ACCEL_BMA150_INPUT)-files += bma150_input.c
$(SOMC_CFG_SENSORS_ACCEL_BMA150_INPUT)-cflags += -DACC_BMA150_INPUT

$(SOMC_CFG_SENSORS_ACCEL_BMA250_INPUT)-files += bma250_input.c
$(SOMC_CFG_SENSORS_ACCEL_BMA250_INPUT)-cflags += -DACC_BMA250_INPUT

$(SOMC_CFG_SENSORS_ACCEL_BMA250NA_INPUT)-files += bma250na_input.c \
					wrappers/bma250na_input_accelerometer.c
$(SOMC_CFG_SENSORS_ACCEL_BMA250NA_INPUT)-cflags += -DACC_BMA250_INPUT

$(SOMC_CFG_SENSORS_COMPASS_LSM303DLH)-cflags += -DST_LSM303DLH
$(SOMC_CFG_SENSORS_COMPASS_LSM303DLH)-var-compass-lsm303dlh = yes

$(SOMC_CFG_SENSORS_COMPASS_LSM303DLHC)-cflags += -DST_LSM303DLHC
$(SOMC_CFG_SENSORS_COMPASS_LSM303DLHC)-var-compass-lsm303dlh = yes

$(yes-var-compass-lsm303dlh)-files += wrappers/lsm303dlhx_accelerometer.c
$(yes-var-compass-lsm303dlh)-var-xyz = yes
ifeq ($(SOMC_CFG_SENSORS_ACCELEROMETER_LSM303DLHC_LT),yes)
$(yes-var-compass-lsm303dlh)-files += lsm303dlhc_acc_lt.c
else
$(yes-var-compass-lsm303dlh)-files += lsm303dlhx_acc.c
endif

#
# Compasses
#
$(SOMC_CFG_SENSORS_COMPASS_AK8963)-var-compass-ak896xna = yes
$(SOMC_CFG_SENSORS_COMPASS_AK8963)-cflags += -DAK8963 \
					     -DAKM_CHIP_NAME="\"AK8963\"" \
					     -DAKM_CHIP_MAXRANGE=4900 \
					     -DAKM_CHIP_RESOLUTION=60 \
					     -DAKM_CHIP_POWER=0.4
$(SOMC_CFG_SENSORS_COMPASS_AK8963)-c-includes += $(DASH_ROOT)/libs/akm8963
$(SOMC_CFG_SENSORS_COMPASS_AK8963)-shared-libs += libsensors_akm8963

$(SOMC_CFG_SENSORS_COMPASS_AK8972)-var-compass-ak897xna = yes
$(SOMC_CFG_SENSORS_COMPASS_AK8972)-cflags += -DAK8972 \
					     -DAKM_CHIP_NAME="\"AK8972\"" \
					     -DAKM_CHIP_MAXRANGE=4900 \
					     -DAKM_CHIP_RESOLUTION=60 \
					     -DAKM_CHIP_POWER=0.4
$(SOMC_CFG_SENSORS_COMPASS_AK8972)-c-includes += $(DASH_ROOT)/libs/akm8972
$(SOMC_CFG_SENSORS_COMPASS_AK8972)-shared-libs += libsensors_akm8972

$(SOMC_CFG_SENSORS_COMPASS_AK8973)-var-compass-ak897x = yes
$(SOMC_CFG_SENSORS_COMPASS_AK8973)-cflags += -DAK8973 \
					     -DAKM_CHIP_NAME="\"AK8973\"" \
					     -DAKM_CHIP_MAXRANGE=2000 \
					     -DAKM_CHIP_RESOLUTION=100 \
					     -DAKM_CHIP_POWER=0.8

$(SOMC_CFG_SENSORS_COMPASS_AK8975)-var-compass-ak897x = yes
$(SOMC_CFG_SENSORS_COMPASS_AK8975)-cflags += -DAK8975 \
					     -DAKM_CHIP_NAME="\"AK8975\"" \
					     -DAKM_CHIP_MAXRANGE=2000 \
					     -DAKM_CHIP_RESOLUTION=100 \
					     -DAKM_CHIP_POWER=0.8

$(yes-var-compass-ak897x)-files   += ak897x.c
$(yes-var-compass-ak897xna)-files += ak897xna.c wrappers/ak897xna_sensors.c
$(yes-var-compass-ak896xna)-files += ak896xna.c wrappers/ak896xna_sensors.c

#
# Light sensors
#

$(SOMC_CFG_SENSORS_LIGHT_AS3676)-files += as3676_als.c
$(SOMC_CFG_SENSORS_LIGHT_AS3676)-cflags += \
    -DALS_CHIP_MAXRANGE=$(if $(SOMC_CFG_SENSORS_LIGHT_MAXRANGE),$(SOMC_CFG_SENSORS_LIGHT_MAXRANGE),4900) \
    -DALS_PATH=\"$(SOMC_CFG_SENSORS_LIGHT_AS3676_PATH)\"

$(SOMC_CFG_SENSORS_LIGHT_LM3533)-files += lm3533_als.c
$(SOMC_CFG_SENSORS_LIGHT_LM3533)-cflags += \
    -DALS_CHIP_MAXRANGE=$(if $(SOMC_CFG_SENSORS_LIGHT_MAXRANGE),$(SOMC_CFG_SENSORS_LIGHT_MAXRANGE),1530) \
    -DLM3533_DEV=\"$(SOMC_CFG_SENSORS_LIGHT_LM3533_PATH)\"

$(SOMC_CFG_SENSORS_LIGHT_LIBALS)-files += light_sensor_als.c
$(SOMC_CFG_SENSORS_LIGHT_LIBALS)-c-includes += $(DASH_ROOT)/libs/libals
$(SOMC_CFG_SENSORS_LIGHT_LIBALS)-shared-libs += libals
$(SOMC_CFG_SENSORS_LIGHT_LIBALS)-var-set-light-range = yes

$(SOMC_CFG_SENSORS_SYSTEM_WIDE_ALS)-files += sys_als.c
$(SOMC_CFG_SENSORS_SYSTEM_WIDE_ALS)-c-includes += $(DASH_ROOT)/libs/lights
$(SOMC_CFG_SENSORS_SYSTEM_WIDE_ALS)-shared-libs += liblights-core
$(SOMC_CFG_SENSORS_SYSTEM_WIDE_ALS)-var-set-light-range = yes

$(yes-var-set-light-range)-cflags += \
	-DLIGHT_RANGE=$(if $(SOMC_CFG_SENSORS_LIGHT_RANGE),$(SOMC_CFG_SENSORS_LIGHT_RANGE),900)

#
# Proximity sensors
#
$(SOMC_CFG_SENSORS_PROXIMITY_APDS9700)-files  += apds970x.c
$(SOMC_CFG_SENSORS_PROXIMITY_APDS9700)-cflags += -DPROXIMITY_SENSOR_NAME="\"APDS9700 Proximity\""

$(SOMC_CFG_SENSORS_PROXIMITY_APDS9702)-files  += apds970x.c
$(SOMC_CFG_SENSORS_PROXIMITY_APDS9702)-cflags += -DPROXIMITY_SENSOR_NAME="\"APDS9702 Proximity\""

$(SOMC_CFG_SENSORS_PROXIMITY_SHARP_GP2)-files += sharp_gp2.c

$(SOMC_CFG_SENSORS_PROXIMITY_NOA3402)-files  += noa3402.c
$(SOMC_CFG_SENSORS_PROXIMITY_NOA3402)-cflags += -DPROXIMITY_PATH="\"$(SOMC_CFG_SENSORS_PROXIMITY_NOA3402_PATH)\""

$(SOMC_CFG_SENSORS_PROXIMITY_TLS2772)-files += tsl2772.c

#
# Pressure sensors
#
$(SOMC_CFG_SENSORS_PRESSURE_BMP180)-files += bmp180_input.c

$(SOMC_CFG_SENSORS_PRESSURE_LPS331AP)-files += lps331ap_input.c

#
# Gyro sensors
#
$(SOMC_CFG_SENSORS_GYRO_MPU3050)-files += mpu3050.c
$(SOMC_CFG_SENSORS_GYRO_MPU3050)-shared-libs += libMPU3050
$(SOMC_CFG_SENSORS_GYRO_MPU3050)-cflags += \
			-I$(LOCAL_PATH)/libs/mpu3050/libsensors

$(SOMC_CFG_SENSORS_GYRO_L3G4200D)-files += l3g4200d_gyro.c \
					   wrappers/l3g4200d_gyroscope.c
$(SOMC_CFG_SENSORS_GYRO_L3G4200D)-cflags += -DGYRO_L3G4200D_INPUT
$(SOMC_CFG_SENSORS_GYRO_L3G4200D)-var-xyz = yes

#
# Wrapper sensors
#

#
# LinearAcceleration, RotationVector, Gravity, Magnetometer
#
ifeq ($(SOMC_CFG_SENSORS_GYRO_L3G4200D),yes)
$(yes-var-compass-lsm303dlh)-files += wrappers/inemo.c
$(yes-var-compass-lsm303dlh)-static-libs += iNemoEngine
$(yes-var-compass-lsm303dlh)-cflags += -O3 -DUSE_MMAP
$(yes-var-compass-lsm303dlh)-c-includes += $(LOCAL_PATH)/libs/inemo \
		$(LOCAL_PATH)/libs/inemo/lib/sensors_compass_API \
		$(LOCAL_PATH)/libs/inemo/lib/MEMSAlgLib_eCompass \
		$(LOCAL_PATH)/libs/inemo/lib/libSpacePointAPI_opt2_1
endif

#
# eCompass, Magnetometer
#
ifneq ($(SOMC_CFG_SENSORS_GYRO_L3G4200D),yes)
$(yes-var-compass-lsm303dlh)-files += wrappers/lsm303dlhx_compass.c
$(yes-var-compass-lsm303dlh)-static-libs += libLSM303DLH
$(yes-var-compass-lsm303dlh)-cflags += -I$(LOCAL_PATH)/libs/lsm303dlh \
				       -DLSM303DLHC
endif
#
# Shared files
#
$(yes-var-xyz)-files += sensor_xyz.c
