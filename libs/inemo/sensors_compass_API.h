#ifndef SENSORS_COMPASS_API_H_
#define SENSORS_COMPASS_API_H_

#include <hardware/sensors.h>

typedef struct {
	signed short magOffX;
	signed short magOffY;
	signed short magOffZ;
	float magGainX;
	float magGainY;
	float magGainZ;
	float expMagVect;
} CalibFactor;

int compass_API_Init(int MagFullScale, int formFactorNumber, CalibFactor* Calibration);
int compass_API_SaveAcc(int acc_x, int acc_y, int acc_z);
int compass_API_SaveMag(int mag_x, int mag_y, int mag_z);
int compass_API_GetNewFullScale(void);
int compass_API_Run(CalibFactor *Calib);
int compass_API_OrientationValues(sensors_event_t *data);
unsigned int compass_API_GetCalibrationGodness(void);
void compass_API_ForceReCalibration(void);
void compass_API_ChangeFormFactor(int formFactorNumber);

#endif
