#include "sensors_log.h"
#include <sys/types.h>
#include "MPLSensor.h"
#include "MPLWrapper.h"

enum {
	mpl = 0,
	numSensorDrivers,
	numFds,
};

static const size_t wake = numFds - 1;
static const char WAKE_MESSAGE = 'W';
int mWritePipeFd;

void *new_object()
{
	MPLSensor *pc = new MPLSensor();

	if (!pc)
		return pc;

	setCallbackObject(pc);

	return static_cast<void*>(pc);
}


int call_enable(void *the_object, int32_t handle, int enabled)
{
	MPLSensor *pc = static_cast<MPLSensor*>(the_object);

	int err = pc->enable(handle, enabled);
	int fd = pc->getFd();

   return fd;
}


int get_numSensors( void *the_object)
{
	MPLSensor *pc = static_cast<MPLSensor*>(the_object);
	return pc->numSensors;
}


int call_open_fd(void *the_object)
{
	MPLSensor *pc = static_cast<MPLSensor*>(the_object);
	return pc->getFd();
}

int mpl_power_fd(void *the_object)
{
	MPLSensor *pc = static_cast<MPLSensor*>(the_object);
	return pc->getPowerFd();
}

void call_power_event(void *the_object)
{
	MPLSensor *pc = static_cast<MPLSensor*>(the_object);
	pc->handlePowerEvent();
}

int call_setdelay(void *the_object, int32_t handle, int64_t ns)
{
	MPLSensor *pc = static_cast<MPLSensor*>(the_object);
	return pc->setDelay(handle, ns);
}


void call_close(void *the_object)
{
	MPLSensor *pc = static_cast<MPLSensor*>(the_object);
	pc->~MPLSensor();
	delete pc;
}


int call_readEvents(void *the_object, sensors_event_t* data, int count)
{
	MPLSensor *pc = static_cast<MPLSensor*>(the_object);
	return pc->readEvents(data, count);
}


void delete_object(void *the_object)
{
	MPLSensor *pc = static_cast<MPLSensor*>(the_object);
	delete pc;
}

