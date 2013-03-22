#ifndef INEMO_ENGINE_API_H_
#define INEMO_ENGINE_API_H_

typedef struct RawCounts
{
	float mag[3];
	float acc[3];
	float gyro[3];
} RawCounts;

int iNemoEngineAPI_Initialization(char LocalEarthMagField, int MagFullScale, int formFactorNumber);
void iNemoEngineAPI_Run(int delta_time, struct RawCounts * RawCnts);
void iNemoEngineAPI_Return_Quaternion(float *quaternion);
void iNemoEngineAPI_Return_Rotation(float *rotation);
void iNemoEngineAPI_Return_Gravity(float *gravity);
void iNemoEngineAPI_Return_Linear_acceleration(float *linear_accel);
int iNemoEngineAPI_Return_NewFullScale(void);
void iNemoEngineAPI_ForceReCalibration(void);
void iNemoEngineAPI_ChangeFormFactor(int formFactorNumber);
void iNemoEngineAPI_getCalibrationData(CalibFactor *CalibrationData);

#endif
