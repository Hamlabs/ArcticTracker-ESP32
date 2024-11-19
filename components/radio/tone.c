/* 
 * Generate a tone (sine wave) using the DAC.
 */

#include "defines.h"
#if !defined(ARCTIC4_UHF)

#include <stdbool.h>
#include <stdint.h>
#include "system.h"

#include "afsk.h"
#include "driver/sdm.h"

/* 
 * It would be optimal to use the least common multiplier (LCM) of 
 * the sampling frequencies (16 x 1200 and 16 x 2200) as the clock 
 * frequency (resolution) of the timer. 211.2 KHz.
 */

#define TONE_RESOLUTION 10000000
// #define TONE_RESOLUTION 211200 
#define FREQ2CNT(f) TONE_RESOLUTION/(f) 
#define TONE_CNT FREQ2CNT(AFSK_MARK*STEPS)

/* The sine wave is generated in 16 steps */
#define STEPS 16

static clock_t toneclk;

static bool _toneHigh = true;
static bool _on = false; 

static int8_t sine[STEPS] = 
    {-3, 45, 85, 112, 122, 112, 85, 45, -3, -51, -91, -118,  -128,  -118, -91,  -51};   

/* 80% */
static uint8_t sine2[STEPS] = 
    {2, 36, 68, 90, 98, 90, 68, 36, -2, -41,  -73,  -94,  -102,  -94,  -73,  -41};
    
/* 50% */
static uint8_t sine3[STEPS] = 
    {2, 23, 43, 56, 61, 56, 43, 23, 2, -26,  -46,  -59,  -64,  -59,  -46,  -26};

    
    
static bool sinewave(struct gptimer_t * t, const gptimer_alarm_event_data_t * a, void * arg); 
    
static sdm_channel_handle_t sdm_chan = NULL;



 
    
/*****************************************************************
 * Call periodically from timer ISR to generate a sine wave.
 * This is to be done 16 X the frequency of the generated tone. 
 *****************************************************************/
volatile uint8_t i = 0;
volatile uint16_t cnt = 0;
volatile bool high = false;


/* ISR */

static bool sinewave(struct gptimer_t * t, const gptimer_alarm_event_data_t * a, void * arg) 
{
    sdm_channel_set_pulse_density(sdm_chan, sine[i]);
    
    i++;
    if (i >= STEPS) 
        i=0;
    return true;
}


/**************************************************************************
 * Init tone generator
 **************************************************************************/
static bool _inited = false; 
void tone_init() {
    if (!_inited) {
        clock_init(&toneclk, TONE_RESOLUTION, TONE_CNT, sinewave, NULL);
        _inited = true;

        sdm_config_t config = {
            .clk_src = SDM_CLK_SRC_DEFAULT,
            .gpio_num = TONE_SDELTA_PIN,
            .sample_rate_hz = TONE_RESOLUTION,
        };
        ESP_ERROR_CHECK(sdm_new_channel(&config, &sdm_chan));
        /* Enable the sdm channel */
        ESP_ERROR_CHECK(sdm_channel_enable(sdm_chan));      
    }
}


/**************************************************************************
 * Start generating a tone using the DAC.
 **************************************************************************/

void tone_start() {
    _on = true;
    tone_setHigh(_toneHigh); 
    clock_start(toneclk);  
}


/**************************************************************************
 * Set the frequency of the tone to mark or space. 
 * argument to true to set it to MARK. Otherwise, SPACE. 
 **************************************************************************/

void tone_setHigh(bool hi) 
{
    if (hi==_toneHigh)
        return;
    _toneHigh = hi; 
    if (_on) 
        clock_set_interval(toneclk, 
            _toneHigh ? FREQ2CNT(AFSK_SPACE*STEPS) : FREQ2CNT(AFSK_MARK*STEPS) ); 
} 

  
/**************************************************************************
 * Toggle between the two tone frequencies (mark or space).
 **************************************************************************/

void tone_toggle()
   { tone_setHigh(!_toneHigh); }
   

/**************************************************************************
 * Stop generating a tone. 
 **************************************************************************/

void tone_stop() {
    if (!_on)
        return;
    _on = false;
    clock_stop(toneclk);
    sdm_channel_set_pulse_density(sdm_chan, 0);
}


#endif
