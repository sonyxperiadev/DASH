LOCAL_PATH := $(call my-dir)

ifneq ($(TARGET_SIMULATOR),true)

include $(CLEAR_VARS)

LOCAL_MODULE := libMPU3050
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\" -DCONFIG_MPU_SENSORS_MPU3050=1 -DDEBUG -UNDEBUG -DLOG_NDEBUG
LOCAL_SRC_FILES := 			\
			SensorBase.cpp	\
			MPLSensor.cpp	\
			MPLWrapper.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/mlsdk/platform/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/mlsdk/platform/linux
LOCAL_C_INCLUDES += $(LOCAL_PATH)/mlsdk/platform/include/linux
LOCAL_C_INCLUDES += $(LOCAL_PATH)/mlsdk/mllite
LOCAL_C_INCLUDES += $(LOCAL_PATH)/mlsdk/mldmp
LOCAL_C_INCLUDES += $(DASH_ROOT)

LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libdl libmllite libmlplatform
LOCAL_CPPFLAGS+=-DLINUX=1
LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
include $(BUILD_MULTI_PREBUILT)
endif # !TARGET_SIMULATOR

include $(call all-subdir-makefiles)
