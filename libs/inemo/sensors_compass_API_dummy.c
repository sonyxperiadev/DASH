#include "sensors_compass_API.h"

int compass_API_Init(int MagFullScale, int formFactorNumber, CalibFactor* Calibration)
{
	return 0;
}

int compass_API_SaveAcc(int acc_x, int acc_y, int acc_z)
{
	return 0;
}

int compass_API_SaveMag(int mag_x, int mag_y, int mag_z)
{
	return 0;
}

int compass_API_GetNewFullScale(void)
{
	return 0;
}

int compass_API_Run(CalibFactor *Calib)
{
	return 0;
}

int compass_API_OrientationValues(sensors_event_t *data)
{
	return 0;
}

unsigned int compass_API_GetCalibrationGodness(void)
{
	return 0;
}

void compass_API_ForceReCalibration(void) {}
void compass_API_ChangeFormFactor(int formFactorNumber) {}
