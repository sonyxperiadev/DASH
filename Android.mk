LOCAL_PATH := $(call my-dir)

# HAL module implemenation, not prelinked and stored in
# hw/<SENSORS_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)

DASH_ROOT := $(LOCAL_PATH)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := \
			liblog \
			libcutils

LOCAL_SRC_FILES += 	sensors_module.c \
			sensors_list.c \
			sensors_config.c \
			sensors_fifo.c \
			sensors_worker.c \
			sensors_select.c \
			sensors_wrapper.c \
			sensors_input_cache.c \
			sensors_sysfs.c \
			sensors/sensor_util.c

LOCAL_CFLAGS += -I$(LOCAL_PATH)/sensors

# Uncomment to enable debug
#LOCAL_CFLAGS += -DDEBUG -UNDEBUG

# Comment to enable debug
LOCAL_CFLAGS += -DLOG_NDEBUG

# Set 1 to enable verbose debug
LOCAL_CFLAGS += -DDEBUG_VERBOSE=0

include $(LOCAL_PATH)/sensors/Sensors.mk
LOCAL_SRC_FILES += $(patsubst %,sensors/%, $(yes-files))
LOCAL_CFLAGS += $(yes-cflags)
LOCAL_C_INCLUDES += $(yes-c-includes)
LOCAL_STATIC_LIBRARIES += $(yes-static-libs)
LOCAL_SHARED_LIBRARIES += $(yes-shared-libs)

LOCAL_MODULE := sensors.default
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

include $(call first-makefiles-under, $(LOCAL_PATH)/libs)

