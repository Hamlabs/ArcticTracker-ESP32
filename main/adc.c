#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "system.h"
#include "esp_adc_cal.h"
#include "defines.h"
#include "config.h"

#define RADIO_ADC_SAMPLE_FREQ    9600 

#define ATTEN         ADC_ATTEN_DB_11
#define WIDTH         ADC_WIDTH_BIT_12
#define UNIT          ADC_UNIT_1
#define DEFAULT_VREF  1100
#define BATT_DIVISOR  3


static esp_adc_cal_value_t char_type;
static esp_adc_cal_characteristics_t *adc_chars;
static uint16_t dcoffset = 0;


/*************************************************************************
 * Print info about characterisation
 *************************************************************************/

void adc_print_char()
{
    if (char_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
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
    
    /* Configure ADC input channels */
    adc1_config_width(WIDTH);
    adc1_config_channel_atten(RADIO_INPUT, ATTEN);
    adc1_config_channel_atten(X1_ADC_INPUT, ATTEN);
    adc1_config_channel_atten(BATT_ADC_INPUT, ATTEN);
    
    /* Calibrate radio channel input */
    dcoffset = adc_read(RADIO_INPUT);
}



/*************************************************************************
 * Read an ADC channel. 
 * Return an average of 64 samples. Use adc_toVoltage to convert 
 * to voltage
 *************************************************************************/

uint16_t adc_read(uint8_t chan) 
{ 
    uint32_t val = 0;
    for (int i=0; i<64; i++)
        val+= adc1_get_raw((adc1_channel_t) chan);
    return (uint16_t) val/64; 
}



/*************************************************************************
 * Convert ADC reading to voltage
 *************************************************************************/

uint16_t adc_toVoltage(uint16_t val)
    { return (uint16_t) esp_adc_cal_raw_to_voltage((uint32_t) val, adc_chars); }


    
/*************************************************************************
 * Get battery voltage estimate
 *************************************************************************/

uint16_t adc_batt()
{ 
    uint16_t val = adc_read(BATT_ADC_INPUT);
    if (val==0)
        return 0; 
    return adc_toVoltage(val) * BATT_DIVISOR; 
}



/*************************************************************************
 * Start sampling of radio channel input using clock 
 *************************************************************************/

void adc_start_sampling() 
{
    /* TBD */
}


void adc_stop_sampling() 
{
    /* TBD */
}


/* ISR function */

static void adc_sample() 
{
    uint16_t sample = (uint16_t) adc1_get_raw((adc1_channel_t) RADIO_INPUT);
 //   afsk_process_sample(sample - dcoffset);
}
