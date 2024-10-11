#if !defined __RADIO_H__
#define __RADIO_H__
  
#include <stdint.h>
#include "driver/uart.h"
#include "esp_adc/adc_continuous.h"

 bool radio_is_on(); 
 void radio_require(); 
 void radio_release(); 
 void radio_wait_enabled();
 void radio_init();
 bool radio_setFreq(uint32_t txfreq, uint32_t rxfreq);
 bool radio_setSquelch(uint8_t sq); 
 bool radio_getSquelch();
 void radio_on(bool on);
 void radio_PTT(bool on);
 void radio_PTT_I(bool on);
 bool radio_setVolume(uint8_t vol);
 bool radio_setMicLevel(uint8_t level); 
 bool radio_powerSave(bool on);
 bool radio_setLowTxPower(bool on);
 bool radio_isLowTxPower(void);
 void wait_channel_ready(void);
 void wait_tx_off(void);

 
/* ADC sampler */
#define ADC_GET_CHANNEL(p)     ((p)->type2.channel)
#define ADC_GET_DATA(p)        ((p)->type2.data)

#define ADC_DATA_VALID(p) ADC_GET_CHANNEL(p) < SOC_ADC_CHANNEL_NUM(ADC_UNIT)
#define ADC_RESULT(buf, i) (void*)&buf[i]
#define ADC_RESULT_BYTES SOC_ADC_DIGI_RESULT_BYTES

typedef adc_continuous_handle_t adcsampler_t; 

void adcsampler_init( adcsampler_t *handle, uint8_t channel);
uint32_t adcsampler_read(adcsampler_t handle, uint8_t result[], uint32_t len );
void adcsampler_calibrate(adcsampler_t handle);
void adcsampler_start(adcsampler_t handle);
void adcsampler_stop(adcsampler_t handle);
void adcsampler_deinit(adcsampler_t handle);

#endif
