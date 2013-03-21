#include <linux/types.h>
#include <hardware/sensors.h>
#include <inttypes.h>
#include <SEMC_APIs.h>

int AKM_Init(int maxFormNumber, const register_map_ak897x *regs, const int16_t mag_layout[3][3])
{
    return 0;
}
void AKM_Release(void)
{
}
int AKM_Start(const char *path)
{
    return 0;
}
int AKM_Stop(const char *path)
{
    return 0;
}
int AKM_SaveAcc(int acc_x, int acc_y, int acc_z, int acc_sensitivity)
{
    return 0;
}
int AKM_SaveMag(int mag_x, int mag_y, int mag_z, int mag_status, const int period)
{
    return 0;
}
int AKM_GetOrientationValues(sensors_event_t *data)
{
    return 0;
}
int AKM_GetMagneticValues(sensors_event_t *data)
{
    return 0;
}
unsigned int AKM_GetCalibrationGoodness(void)
{
    return 0;
}
void AKM_ForceReCalibration(void)
{
}
int AKM_ChangeFormFactor(int formFactorNumber)
{
    return 0;
}

