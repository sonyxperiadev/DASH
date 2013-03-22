LOCAL_PATH:= $(call my-dir)

ifneq ($(SOMC_CFG_SENSORS_GYRO_L3G4200D),yes)
include $(CLEAR_VARS)
LOCAL_SRC_FILES:= sensors_compass_API_dummy.c
LOCAL_MODULE := libLSM303DLH
LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)
endif
