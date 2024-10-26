
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
bool      pmu_dc3_isOn();


#ifdef __cplusplus
}
#endif

#endif
