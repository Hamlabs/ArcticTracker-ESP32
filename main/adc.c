#include <stdio.h>
#include <stdlib.h>
#include "system.h"
#include "driver/gpio.h"
#include "esp_adc_cal.h"
#include "defines.h"
#include "config.h"
#include "hal/adc_hal.h"
#include "ui.h"


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
    uint32_t val = 0;
    int out, n = 0; 
    for (int i=0; i<64; i++)
        if (adc2_get_raw(WIDTH, (adc2_channel_t) chan, &out)==ESP_OK)
            {n++; val += out;};
    printf("adc2 n=%d\n", n);
    return (uint16_t) (val/n); 
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
    uint16_t val = adc1_read(BATT_ADC_INPUT);
    if (val==0)
        return 0; 
    return adc_toVoltage(val) * BATT_DIVISOR; 
}



/*************************************************************************
 * Textual description of battery status
 *************************************************************************/
// FIXME: Move to ui.c

uint16_t adc_batt_status(char* line1, char* line2)
{
    uint16_t vbatt = adc_batt();
    if (line2)
        line2[0] = '\0'; 
    
    if (batt_charge()) {
        if (vbatt > 8340) {
            if (line1) sprintf(line1, "Max (charged)");
        }
        else if (vbatt > 8000) { 
            if (line1) sprintf(line1, "Full.");
        }
        else if (vbatt > 7600) {
            if (line1) sprintf(line1, "Ok.");
        }
        else if (vbatt > 7300) {
            if (line1) sprintf(line1, "Low.");  
        }
    }
    else {
        if (vbatt > 8250) {
            if (line1) sprintf(line1, "Max (charged)");
        }
        else if (vbatt > 7800) { 
            if (line1) sprintf(line1, "Full.");
        }
        else if (vbatt > 7400) {
            if (line1) sprintf(line1, "Ok.");
        }
        else if (vbatt > 7100) {
            if (line1) sprintf(line1, "Low.");  
            if (line2) sprintf(line2, "Need charging.");
        }
        else {
            if (line1) sprintf(line1, "Empty.");
            if (line2) sprintf(line2, "Charge ASAP!");
        } 
    }
    
    return vbatt;
}



int adc2_get_rawISR(adc2_channel_t channel)
{
    uint16_t out;
    
    adc2_get_raw(WIDTH, (adc2_channel_t) channel, &out);
    
    return out;
}




/*************************************************************************
 * Sampling of radio channel - to be called from timer ISR 
 *************************************************************************/


void adc_calibrate() {
    /* Calibrate radio channel input */
    dcoffset = adc2_read(RADIO_INPUT);
    printf("DCOFFSET=%d\n", dcoffset);
}



int16_t adc_sample() 
{  
    /* Workaround: Disable interrupts during adc read. The implementation uses a 
     * spinlock and interrupts may happen there interfering with the read. 
     */
    taskDISABLE_INTERRUPTS();
    uint16_t sample = (uint16_t) adc2_get_rawISR((adc2_channel_t) RADIO_INPUT);
  
    taskENABLE_INTERRUPTS();    
    sample &= 0x0FFF;
    int16_t res = ((int16_t) sample) - dcoffset;
    return res;
}
