#ifndef ALS_INPUT_H_
#define ALS_INPUT_H_

int als_getlux(float *data);
int als_get_filtered(unsigned long *data);
int als_getlux_filtered(float *data);
int als_enable(void);
int als_disable(void);
int als_available(void);

#endif
