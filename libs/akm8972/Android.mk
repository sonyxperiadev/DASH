ifeq ($(SOMC_CFG_SENSORS_COMPASS_AK8972),yes)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_PREBUILT_LIBS := SEMC_8972.a
include $(BUILD_MULTI_PREBUILT)

include $(CLEAR_VARS)

ifeq ($(SOMC_CFG_SENSORS_HAVE_LIBAK8972), yes)
# Add prebuilt library if available

LOCAL_MODULE_TAGS := optional
LOCAL_PREBUILT_LIBS := libAK8972.a
include $(BUILD_MULTI_PREBUILT)
else
# Or else build the dummy lib

LOCAL_MODULE := libAK8972
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := libAK8972_dummy.c
include $(BUILD_STATIC_LIBRARY)
endif

endif
