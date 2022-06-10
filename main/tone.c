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
 * frequency of the timer. 211.2 KHz.
 */

#define CLOCK_DIVIDER 16
#define CLOCK_FREQ (TIMER_BASE_CLK / CLOCK_DIVIDER)
#define FREQ(f)    (CLOCK_FREQ/(f))


/* The sine wave is generated in 16 steps */
#define STEPS 16


static bool _toneHigh = false;
static bool _on = false; 
static uint8_t sine[STEPS] = 
    {125, 173, 213, 240, 250, 240, 213, 173, 125, 77,  37,  10,  0,   10,  37,  77};   
    
/* 80% */
static uint8_t sine2[STEPS] = 
    {125, 163, 196, 217, 225, 217, 196, 163, 125, 87,  54,  33,  25,  33,  54,  87};
   
    
static bool sinewave(void* arg); 
    
 
    
/*****************************************************************
 * Call periodically from timer ISR to generate a sine wave.
 * This is to be done 16 X the frequency of the generated tone. 
 *****************************************************************/
volatile uint8_t i = 0;
volatile uint16_t cnt = 0;
volatile bool high = false;

static bool sinewave(void *arg) 
{
#if defined(TONE_ADC_ENABLE)
    dac_output_voltage(TONE_DAC_CHAN, sine[i]);
#endif
#if defined(TONE_SDELTA_ENABLE)
    
    sigmadelta_set_duty(TONE_SDELTA_CHAN, (_toneHigh ? sine[i]-128 : sine2[i]-128));
//    sigmadelta_set_duty(TONE_SDELTA_CHAN, sine[i]-128);
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
        clock_init(TONE_TIMERGRP, TONE_TIMERIDX, CLOCK_DIVIDER, sinewave, false);
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
    clock_start(TONE_TIMERGRP, TONE_TIMERIDX, 
        _toneHigh ? FREQ(AFSK_SPACE*STEPS) : FREQ(AFSK_MARK*STEPS));
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
        clock_changeInterval(TONE_TIMERGRP, TONE_TIMERIDX, 
            _toneHigh ? FREQ(AFSK_SPACE*STEPS) : FREQ(AFSK_MARK*STEPS) ); 
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
    clock_stop(TONE_TIMERGRP, TONE_TIMERIDX);
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




