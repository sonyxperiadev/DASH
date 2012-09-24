LOCAL_PATH := $(call my-dir)

ifeq ($(SOMC_CFG_SENSORS_HAVE_LIBMPU3050), yes)
# Add prebuilt libraries if available

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_PREBUILT_LIBS := libMPU3050.so
include $(BUILD_MULTI_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_PREBUILT_LIBS := libmpl.so
include $(BUILD_MULTI_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_PREBUILT_LIBS := libmllite.so
include $(BUILD_MULTI_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_PREBUILT_LIBS := libmlplatform.so
include $(BUILD_MULTI_PREBUILT)
else
# Or else build the dummy lib

include $(CLEAR_VARS)

LOCAL_MODULE := libMPU3050
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := libMPU3050_dummy.c
include $(BUILD_SHARED_LIBRARY)
endif

