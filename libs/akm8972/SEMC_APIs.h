#ifndef AKM897X_SEMC_APIS_H_
#define AKM897X_SEMC_APIS_H_

#include <hardware/sensors.h>
#include <inttypes.h>

typedef struct _register_map_ak897x {
    uint32_t version;
    uint32_t size;
    uint8_t WIA;
    uint8_t INFO;
    uint8_t ST1;
    uint8_t HXL;
    uint8_t HXH;
    uint8_t HYL;
    uint8_t HYH;
    uint8_t HZL;
    uint8_t HZH;
    uint8_t ST2;
    uint8_t ASAX;
    uint8_t ASAY;
    uint8_t ASAZ;
} register_map_ak897x;

int AKM_Init(int maxFormNumber, const register_map_ak897x *regs, const int16_t mag_layout[3][3]);
void AKM_Release(void);
int AKM_Start(const char *path);
int AKM_Stop(const char *path);
int AKM_SaveAcc(int acc_x, int acc_y, int acc_z, int acc_sensitivity);
int AKM_SaveMag(int mag_x, int mag_y, int mag_z, int mag_status, const int period);
int AKM_GetOrientationValues(sensors_event_t *data);
int AKM_GetMagneticValues(sensors_event_t *data);
unsigned int AKM_GetCalibrationGoodness(void);
void AKM_ForceReCalibration(void);
int AKM_ChangeFormFactor(int formFactorNumber);

#endif
