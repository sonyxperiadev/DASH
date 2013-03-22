ifeq ($(SOMC_CFG_SENSORS_COMPASS_AK8972),yes) 

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libsensors_akm8972
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES += libsensors_akm8972_dummy.c
include $(BUILD_SHARED_LIBRARY)

endif
