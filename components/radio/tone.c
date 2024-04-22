/* 
 * Generate a tone (sine wave) using the DAC.
 */


#include "defines.h"
#include <stdbool.h>
#include <stdint.h>
#include "system.h"

#include "afsk.h"

#if defined(TONE_ADC_ENABLE)
#include "driver/dac.h"
#endif
#if defined(TONE_SDELTA_ENABLE)
#include "driver/sigmadelta.h"
#endif

/* 
 * It would be optimal to use the least common multiplier (LCM) of 
 * the sampling frequencies (16 x 1200 and 16 x 2200) as the clock 
 * frequency (resolution) of the timer. 211.2 KHz.
 */

#define TONE_RESOLUTION 211200 
#define FREQ2CNT(f) TONE_RESOLUTION/(f) 
#define TONE_CNT FREQ2CNT(AFSK_MARK*STEPS)

/* The sine wave is generated in 16 steps */
#define STEPS 16

static clock_t toneclk;

static bool _toneHigh = false;
static bool _on = false; 
static uint8_t sine[STEPS] = 
    {125, 173, 213, 240, 250, 240, 213, 173, 125, 77,  37,  10,  0,   10,  37,  77};   
    
/* 80% */
static uint8_t sine2[STEPS] = 
    {125, 163, 196, 217, 225, 217, 196, 163, 125, 87,  54,  33,  25,  33,  54,  87};
   
    
static bool sinewave(struct gptimer_t * t, const gptimer_alarm_event_data_t * a, void * arg); 
    
 
    
/*****************************************************************
 * Call periodically from timer ISR to generate a sine wave.
 * This is to be done 16 X the frequency of the generated tone. 
 *****************************************************************/
volatile uint8_t i = 0;
volatile uint16_t cnt = 0;
volatile bool high = false;

static bool sinewave(struct gptimer_t * t, const gptimer_alarm_event_data_t * a, void * arg) 
{
#if defined(TONE_ADC_ENABLE)
    dac_output_voltage(TONE_DAC_CHAN, sine[i]);
#endif
#if defined(TONE_SDELTA_ENABLE)
    
    sigmadelta_set_duty(TONE_SDELTA_CHAN, (_toneHigh ? sine[i]-128 : sine2[i]-128));
#endif
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
      
#if defined(TONE_ADC_ENABLE)
        /* DAC in ESP32 */
        dac_output_enable(TONE_DAC_CHAN);
        dac_output_voltage(TONE_DAC_CHAN, 0);
#endif
#if defined(TONE_SDELTA_ENABLE)  
        /* Sigma delta method */
        sigmadelta_config_t sigmadelta_cfg = {
            .channel = TONE_SDELTA_CHAN,
            .sigmadelta_prescale = 5,
            .sigmadelta_duty = 0,
            .sigmadelta_gpio = TONE_SDELTA_PIN,
        };
        sigmadelta_config(&sigmadelta_cfg);
        sigmadelta_set_duty(TONE_SDELTA_CHAN, 0);
#endif        
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
// FIXME: May be called from ISR

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
#if defined(TONE_ADC_ENABLE)
    dac_output_voltage(TONE_DAC_CHAN, 0);
#endif
#if defined(TONE_SDELTA_ENABLE) 
    sigmadelta_set_duty(TONE_SDELTA_CHAN, 0);
#endif
}

#if !defined(TONE_ADC_ENABLE) && !defined(TONE_SDELTA_ENABLE)

void tone_init() {}
void tone_start() {}
void tone_setHigh(bool hi) {}
void tone_toggle() {}
void tone_stop() {}

#endif




