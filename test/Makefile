CC=gcc
LD=ld

SOMC_CFG_SENSORS_ACCEL_DUMMY=yes
SOMC_CFG_SENSORS_COMPASS_DUMMY=yes

ifeq ($(LD_LIBRARY_PATH),)
$(warning Your LD_LIBRARY_PATH is empty. Please run: export LD_LIBRARY_PATH=.)
endif

SRC_PATH = $(PWD)/..

ifeq ($(ANDROID_BUILD_TOP),)
$(error Unable to find Android root. Please set ANDROID_BUILD_TOP.)
endif
ANDROID_ROOT=$(ANDROID_BUILD_TOP)

LOCAL_SRC_FILES += $(SRC_PATH)/sensors_module.c \
		   $(SRC_PATH)/sensors_list.c \
		   $(SRC_PATH)/sensors_config.c \
		   $(SRC_PATH)/sensors_fifo.c \
		   $(SRC_PATH)/sensors_worker.c \
		   $(SRC_PATH)/sensors_select.c \
		   $(SRC_PATH)/sensors_wrapper.c \
		   $(SRC_PATH)/sensors/sensor_util.c

include $(SRC_PATH)/sensors/Sensors.mk

#
# Enable for debug
#
#CFLAGS += -DVERBOSE=1

LOCAL_SRC_FILES += $(patsubst %,$(SRC_PATH)/sensors/%, $(DASH_SENSORS))
LIB_OBJS=$(patsubst %.c,%.o, $(LOCAL_SRC_FILES))

CFLAGS += -ggdb -Wall -I$(ANDROID_ROOT)/hardware/libhardware/include -Imock \
	  -I$(ANDROID_ROOT)/system/core/include -I$(SRC_PATH) -I$(SRC_PATH)/sensors

LDFLAGS += -L.

TEST_CONFIG_TARGET = sensors_test_config

LIB_TARGET = libsensors.so

.PHONY: all
all: $(LIB_TARGET) $(TEST_CONFIG_TARGET)

.PHONY: run_tests
run_tests: all
	 @echo -e "Running $(TEST_CONFIG_TARGET)"  ; ./$(TEST_CONFIG_TARGET)

$(LIB_TARGET): CFLAGS += -c -fPIC
$(LIB_TARGET): LDFLAGS += -lpthread -lrt
$(LIB_TARGET): $(LIB_OBJS)
	$(CC) -shared $(LDFLAGS) -o $(LIB_TARGET) $(LIB_OBJS)

$(TEST_CONFIG_TARGET): LDFLAGS += -lsensors
$(TEST_CONFIG_TARGET): $(TEST_CONFIG_TARGET).o

clean:
	rm -f $(LIB_OBJS) $(LIB_TARGET) $(TEST_CONFIG_TARGET).o $(TEST_CONFIG_TARGET)
