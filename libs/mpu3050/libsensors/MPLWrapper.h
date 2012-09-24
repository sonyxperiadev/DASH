//#ifndef __CPPSUB_H__
//#define __CPPSUB_H__ 1

#include <sys/types.h>
#include <hardware/sensors.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
	extern void initMPL();
	extern void setupCallbacks();
	extern void setupFIFO();
	extern void cb_onMotion(uint16_t);
	extern void cb_procData();
	extern void set_power_states(int);
	extern void log_sys_api_addr();
	extern int selfTest();
/*******************/
/* wrapper function */
/*******************/
	extern void *new_object();
	extern int call_enable(void *the_object, int32_t handle, int enabled);
	extern int get_numSensors(void *the_object);
	extern int call_setdelay(void *the_object, int32_t handle, int64_t ns);
	extern void call_close(void *the_object);
	extern int call_readEvents(void *the_object, sensors_event_t* data, int count);
	extern void delete_object(void *the_object);
	extern int call_open_fd(void *the_object);
	extern int call_open_fd(void *the_object);
#ifdef __cplusplus
}
#endif /* __cplusplus */



//#endif /* __CPPSUB_H__ */


