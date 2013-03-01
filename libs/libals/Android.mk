LOCAL_PATH := $(call my-dir)

ifeq ($(SOMC_CFG_SENSORS_LIGHT_LIBALS),yes)
include $(CLEAR_VARS)
LOCAL_MODULE := libals
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES += libals_dummy.c
include $(BUILD_SHARED_LIBRARY)
endif

