LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifneq ($(SOMC_CFG_SENSORS_AKM8963_DUMMY),)

LOCAL_MODULE := libsensors_akm8963
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES += ak8963_dummy.c
include $(BUILD_SHARED_LIBRARY)

endif
