#if !defined __RADIO_H__
#define __RADIO_H__
  
#include <stdint.h>
#include "driver/uart.h"
#include "esp_adc/adc_continuous.h"


 uint8_t radio_getRssi();
 bool radio_is_on(); 
 bool radio_tx_is_on(); 
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

 
 
#if defined(ARCTIC4_UHF)
 
void lora_config(uint8_t spreadingFactor, uint8_t bandwidth, uint8_t codingRate, 
        uint16_t preambleLength, uint8_t payloadLen, bool crcOn, bool invertIrq, uint8_t ldro );

void lora_SetModulationParams(uint8_t spreadingFactor, uint8_t bandwidth, 
        uint8_t codingRate, uint8_t ldro);

void lora_SetCadParams(uint8_t cadSymbolNum, uint8_t cadDetPeak, uint8_t cadDetMin, 
        uint8_t cadExitMode, uint32_t cadTimeout);

void lora_setTxPower(uint8_t lvl);
void lora_SetRfFrequency(uint32_t frequency);
uint8_t lora_ReceivePacket(uint8_t *pData, int16_t len); 
void lora_SendPacket(uint8_t *pData, int16_t len);
void lora_TxOff();
void lora_SetIrqHandler(gpio_isr_t handler, uint16_t mask);
uint16_t lora_GetIrqStatus();
void lora_ClearIrqStatus();
void lora_GetPacketStatus(int8_t *rssiPacket, int8_t *snrPacket);
void lora_GetRxBufferStatus(uint8_t *payloadLength, uint8_t *rxStartBufferPointer);
void lora_SetBufferAddr(uint8_t txBaseAddress, uint8_t rxBaseAddress);
uint8_t lora_ReadBuffer(uint8_t *rxData, int16_t rxDataLen);
void lora_WriteBuffer(uint8_t *txData, int16_t txDataLen);


#else 
 
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


#endif
