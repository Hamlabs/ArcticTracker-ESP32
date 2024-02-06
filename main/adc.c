#include <stdio.h>
#include <stdlib.h>
#include "system.h"
#include "driver/gpio.h"
#include "defines.h"
#include "config.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"


#include "ui.h"

#define TAG "adc"
#define ATTEN         ADC_ATTEN_DB_11
#define WIDTH         ADC_BITWIDTH_12
#define BATT_DIVISOR  3.3

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;
    
static bool calib1 = false;
                          
static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle);



/*************************************************************************
 * Set up ADC
 *************************************************************************/

void adc_init()
{
    /* ADC1 Init*/
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    /* Channels Config */
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = WIDTH,
        .atten = ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, X1_ADC_INPUT, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, BATT_ADC_INPUT, &config));
    /* RADIO INPUT is on ADC2 */
    
    /* ADC1 Calibration Init */
 //   calib1 = adc_calibration_init(ADC_UNIT_1, ATTEN, &adc1_cali_handle);

   // FIXME: Add code for ADC2 
}




static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = WIDTH,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = WIDTH,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}


static void adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}




/*************************************************************************
 * Read an ADC channel. 
 * Return an average of 64 samples. Use adc_toVoltage to convert 
 * to voltage
 *************************************************************************/

uint16_t adc1_read(uint8_t chan) 
{ 
    int raw=0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, chan, &raw));
    ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, chan, raw);
    return raw;
}


uint16_t adc2_read(uint8_t chan) 
{ 
    uint32_t val = 0;
    return (uint16_t) (val); 
}




/*************************************************************************
 * Convert ADC reading to voltage
 *************************************************************************/

uint16_t adc_toVoltage(uint16_t val) 
{ 
    int voltage=0; 
    if (!calib1) 
        calib1 = adc_calibration_init(ADC_UNIT_1, ATTEN, &adc1_cali_handle);
    if (calib1) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, val, &voltage));
        ESP_LOGI(TAG, "ADC Cali Voltage: %d mV", voltage);
    }
    return voltage;
}


    
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



int adc2_get_rawISR(adc2_channel_t channel)
{
    int out = 0;
    return out;
}




/*************************************************************************
 * Sampling of radio channel - to be called from timer ISR 
 *************************************************************************/


void adc_calibrate() {
}



int16_t adc_sample() 
{  
    return 0;
}
