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

$(yes-var-compass-lsm303dlh)-files += lsm303dlhx_acc.c \
				      wrappers/lsm303dlhx_accelerometer.c
$(yes-var-compass-lsm303dlh)-var-xyz = yes


#
# Compasses
#
$(SOMC_CFG_SENSORS_COMPASS_AK8972)-var-compass-ak897xna = yes
$(SOMC_CFG_SENSORS_COMPASS_AK8972)-cflags += -DAK8972 \
					     -DAKM_CHIP_NAME="\"AK8972\"" \
					     -DAKM_CHIP_MAXRANGE=4900 \
					     -DAKM_CHIP_RESOLUTION=60 \
					     -DAKM_CHIP_POWER=0.4
$(SOMC_CFG_SENSORS_COMPASS_AK8972)-static-libs := SEMC_8972 libAK8972

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

#
# Light sensors
#
$(SOMC_CFG_SENSORS_SYSTEM_WIDE_ALS)-files += sys_als.c
$(SOMC_CFG_SENSORS_SYSTEM_WIDE_ALS)-c-includes += $(DASH_ROOT)/libs/libals
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

#
# Shared files
#
$(yes-var-xyz)-files += sensor_xyz.c
