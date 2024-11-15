
#ifndef PMU_HEADER
#define PMU_HEADER

#ifdef __cplusplus
extern "C" {
#endif

    
esp_err_t pmu_init(void);
void      pmu_power_setup(void);
void      pmu_batt_setup(void);
void      pmu_gps_on(bool on);
uint16_t  pmu_getBattVoltage(void);
uint16_t  pmu_getBattPercent(void);
float     pmu_getTemperature(void);
bool      pmu_isCharging();
void      pmu_disableShutdown(bool on);
void      pmu_dc3_on(bool on);
bool      pmu_dc3_isOn();
void      pmu_printInfo();


#ifdef __cplusplus
}
#endif

#endif
