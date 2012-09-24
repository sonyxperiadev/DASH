/*
 * MPU3050 dummy implementation
 *
 * This file provides a dummy implementation of libMMPU3050.so
 *
 */

#include <fcntl.h>
#include "MPLWrapper.h"

static int null_fd;

void *new_object()
{
	return &null_fd;
}


int call_enable(void *the_object, int32_t handle, int enabled)
{
	return 0;
}


int get_numSensors( void *the_object)
{
	return 0;
}


int call_open_fd(void *the_object)
{
	int *fd = the_object;
	*fd = open("/dev/null", O_RDONLY);
	return *fd;
}


int call_setdelay(void *the_object, int32_t handle, int64_t ns)
{
	return 0;
}


void call_close(void *the_object)
{
	int *fd = the_object;
	close(*fd);
}


int call_readEvents(void *the_object, sensors_event_t* data, int count)
{
	return 0;
}


void delete_object(void *the_object)
{
}
