
#include "defines.h"
#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "radio.h"
#include "system.h"



#define BUF_SIZE        2048   // set the maximum size of the pool in bytes
#define READ_LEN        128    // set the size of the ADC conversion frame, in bytes.
#define SAMPLE_FREQ     9600

#if DEVICE == T_TWR
#define ADC_ATTEN       ADC_ATTEN_DB_12
#else
#define ADC_ATTEN       ADC_ATTEN_DB_6
#endif

#define ADC_BIT_WIDTH   ADC_BITWIDTH_12          // the bitwidth of the raw conversion result. 9, 10, 11, 12 or 13
#define ADC_CONV_MODE   ADC_CONV_SINGLE_UNIT_1
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2




/*
CONV MODES:
    ADC_CONV_SINGLE_UNIT_1
    ADC_CONV_SINGLE_UNIT_2 
    ADC_CONV_BOTH_UNIT 
    ADC_CONV_ALTER_UNIT
    
    
Output type: 
    ESP32S3 has only type 2
    union {
        struct {
            uint32_t data:          12;  !<ADC real output data info. Resolution: 12 bit. 
            uint32_t reserved12:    1;   !<Reserved12. 
            uint32_t channel:       4;   !<ADC channel index info.
                                            If (channel < ADC_CHANNEL_MAX), The data is valid.
                                            If (channel > ADC_CHANNEL_MAX), The data is invalid. 
            uint32_t unit:          1;   !<ADC unit index info. 0: ADC1; 1: ADC2.  
            uint32_t reserved17_31: 14;  !<Reserved17. 
        } type2;                         !<When the configured output format is 12bit. 
        uint32_t val;                    !<Raw data value 
    };
    
*/    
    

static semaphore_t data_ready; 
uint32_t adcsampler_nullpoint = 2048; 





/**************************************************************************************
  Setup: 
    - Pointer to the ADC handle to initialize
    - number of channels to initialize
 **************************************************************************************/

void adcsampler_init( adcsampler_t *handle, uint8_t ionr)
{
    adc_unit_t unit;
    adc_channel_t channel;
    ESP_ERROR_CHECK( adc_continuous_io_to_channel(ionr, &unit, &channel) );
    data_ready = sem_create(0);
    
    
    /* Create a handle */
    adc_continuous_handle_t myhandle = NULL;
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = BUF_SIZE,
        .conv_frame_size = READ_LEN,    //set the size of the ADC conversion frame, in bytes.
    };
    ESP_ERROR_CHECK( adc_continuous_new_handle(&adc_config, &myhandle) );

    
    /* Configure it */
    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = SAMPLE_FREQ,
        .conv_mode = ADC_CONV_MODE,
        .format = ADC_OUTPUT_TYPE,
    };

    /* Part of the configuration is the adc_pattern.
     * It is an array of channel-configs. Here we set up one such
     * channel. 
     */
    adc_digi_pattern_config_t  pattern[1] = {0}; 
    dig_cfg.pattern_num = 1; 
    
    /* Set up channel */
    pattern[0].atten = ADC_ATTEN;
    pattern[0].channel = channel & 0x7;   // the IO corresponding ADC channel number
    pattern[0].unit = unit;               // the ADC that the IO is subordinate to.
    pattern[0].bit_width = ADC_BIT_WIDTH; // the bitwidth of the raw conversion result.
    
    dig_cfg.adc_pattern = pattern;
    ESP_ERROR_CHECK( adc_continuous_config(myhandle, &dig_cfg) );
    *handle = myhandle;
}   




static bool IRAM_ATTR _callback (adcsampler_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    //Notify that ADC continuous driver has done enough number of conversions
    sem_up(data_ready);
    return false; 
}




/**************************************************************************************
  Read samples
  This is a blocking read. It waits until input is available in the buffer.     
 **************************************************************************************/

uint32_t adcsampler_read(adcsampler_t handle, uint8_t result[], uint32_t len )
{
    uint32_t ret_num;
    sem_down(data_ready);
    esp_err_t ret = adc_continuous_read(handle, result, len, &ret_num, 0);

    if (ret == ESP_OK) 
        return ret_num;
    return -1;
}


/**************************************************************************************
  Calibrate sampler, find null point
 **************************************************************************************/

void adcsampler_calibrate(adcsampler_t handle) 
{
    uint8_t buf[READ_LEN]; 
    uint32_t len, sum=0, nresults=0; 
    adcsampler_start(handle);
    len = adcsampler_read(handle, buf, READ_LEN);
    if (len == -1)
        return;
    
    for (int i = 0; i < len; i += ADC_RESULT_BYTES) {
        adc_digi_output_data_t  *p = ADC_RESULT(buf,i);
        if (ADC_DATA_VALID(p)) { 
            sum += ADC_GET_DATA(p);            
            nresults++;
        }
    }
    adcsampler_stop(handle);
    adcsampler_nullpoint = sum / nresults; 
    printf("*** nullpoint = %ld\n", adcsampler_nullpoint);
    
}



/**************************************************************************************
  Start sampling
  
 **************************************************************************************/

void adcsampler_start(adcsampler_t handle) 
{
    /* Register the callback */
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = _callback,
    };
    ESP_ERROR_CHECK( adc_continuous_register_event_callbacks(handle, &cbs, NULL) );
    /* Start the conversion */
    ESP_ERROR_CHECK( adc_continuous_start(handle) );
}



/**************************************************************************************
  Stop sampling
  Deinit sampler
  
 **************************************************************************************/

void adcsampler_stop(adcsampler_t handle) {
    /* Stop the conversion */
    ESP_ERROR_CHECK( adc_continuous_stop(handle) );
}


void adcsampler_deinit(adcsampler_t handle)  {
    ESP_ERROR_CHECK( adc_continuous_deinit(handle) );
}
