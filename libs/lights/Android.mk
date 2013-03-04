LOCAL_PATH := $(call my-dir)

ifeq ($(SOMC_CFG_SENSORS_SYSTEM_WIDE_ALS),yes)
include $(CLEAR_VARS)
LOCAL_MODULE := liblights-core
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES += liblights_dummy.c
include $(BUILD_SHARED_LIBRARY)
endif
