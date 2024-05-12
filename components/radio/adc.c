#include <stdio.h>
#include <stdlib.h>
#include "system.h"
#include "driver/gpio.h"
#include "defines.h"
#include "config.h"
#include "ui.h"
#include "hal/adc_hal.h"
#include "esp_adc_cal.h"


#define TAG "adc"

#define WIDTH  ADC_BITWIDTH_12


static uint16_t dcoffset = 0;

// FIXME: Use new ADC driver and continous ADC mode for sampling RX


#if DEVICE == T_TWR || DEVICE == ARCTIC4

#define ATTEN ADC_ATTEN_DB_0



void radio_adc_init()
{
    /* Calibrate radio channel input */
    dcoffset = radio_adc_sample(RADIO_INPUT);
}



void adc_init()
{
    radio_adc_init();
}



int16_t radio_adc_sample() 
{      
    /* Workaround: Disable interrupts during adc read. The implementation uses a 
     * spinlock and interrupts may happen there interfering with the read. 
     */
    taskDISABLE_INTERRUPTS();
    uint16_t sample = (uint16_t) adc1_get_raw((adc1_channel_t) RADIO_INPUT);
    taskENABLE_INTERRUPTS();    
    sample &= 0x0FFF;
    int16_t res = ((int16_t) sample) - dcoffset;
    return res;
}




#else

#define ATTEN         ADC_ATTEN_DB_11
#define WIDTH         ADC_BITWIDTH_12
#define DEFAULT_VREF  1100
#define BATT_DIVISOR  3
#define UNIT          ADC_UNIT_1

static esp_adc_cal_value_t char_type;
static esp_adc_cal_characteristics_t *adc_chars;

void adc_calibrate();


/*************************************************************************
 * Print info about characterisation
 *************************************************************************/

void adc_print_char()
{
    if (char_type == ESP_ADC_CAL_VAL_EFUSE_TP_FIT)
        printf("Characterized using Two point value FIT\n");
    else if (char_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (char_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using configured Vref\n");
    }
}


/*************************************************************************
 * Set up ADC
 *************************************************************************/


void adc_init()
{
    //Characterize ADC
    uint16_t ref = get_u16_param("ADC.REF", DEFAULT_VREF);
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    char_type = esp_adc_cal_characterize(UNIT, ATTEN, WIDTH, ref, adc_chars);
    adc_print_char();
    
    /* Configure ADC input channels */
    adc1_config_width(WIDTH);

#if !defined(RADIO_DISABLE)
    adc2_config_channel_atten(RADIO_INPUT, ATTEN);
#endif
    adc1_config_channel_atten(X1_ADC_INPUT, ATTEN);
    adc1_config_channel_atten(BATT_ADC_INPUT, ATTEN);
    adc_calibrate(); 
}


/*************************************************************************
 * Read an ADC channel. 
 * Return an average of 64 samples. Use adc_toVoltage to convert 
 * to voltage
 *************************************************************************/


uint16_t adc1_read(uint8_t chan) 
{ 
    uint32_t val = 0;
    for (int i=0; i<64; i++)
        val+= adc1_get_raw((adc1_channel_t) chan);
    return (uint16_t) (val/64); 
}

uint16_t adc2_read(uint8_t chan) 
{ 
    return 0;
    uint32_t val = 0;
    int out, n = 0; 
    for (int i=0; i<64; i++)
        if (adc2_get_raw(WIDTH, (adc2_channel_t) chan, &out) == ESP_OK)
            {n++; val += out;};
    printf("adc2 n=%d\n", n);
    return (uint16_t) (val/n); 
}


/*************************************************************************
 * Convert ADC reading to voltage
 *************************************************************************/

uint16_t adc_toVoltage(uint16_t val)
    { return 0;  return (uint16_t) esp_adc_cal_raw_to_voltage((uint32_t) val, adc_chars); }

    
/*************************************************************************
 * Get battery voltage estimate
 *************************************************************************/

uint16_t adc_batt()
{ 
    uint16_t val = adc1_read(BATT_ADC_INPUT);
    if (val==0)
        return 0; 
    return adc_toVoltage(val) * BATT_DIVISOR; 
}


/*************************************************************************
 * Sampling of radio channel - to be called from timer ISR 
 *************************************************************************/

void adc_calibrate() {
    /* Calibrate radio channel input */
    dcoffset = adc1_read(RADIO_INPUT);
    printf("DCOFFSET=%d\n", dcoffset);
}



int16_t radio_adc_sample() 
{      
    /* Workaround: Disable interrupts during adc read. The implementation uses a 
     * spinlock and interrupts may happen there interfering with the read. 
     */
    taskDISABLE_INTERRUPTS();
    uint16_t sample = sample = (uint16_t) adc1_get_raw((adc1_channel_t) RADIO_INPUT);
  
    taskENABLE_INTERRUPTS();    
    sample &= 0x0FFF;
    int16_t res = ((int16_t) sample) - dcoffset;
    return res;
}



#endif



