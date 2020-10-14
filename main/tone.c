/* 
 * Generate a tone (sine wave) using the DAC.
 */


//#include "afsk.h"
#include <stdbool.h>
#include <stdint.h>
#include "driver/dac.h"
#include "defines.h"
#include "system.h"
#include "afsk.h"


#define STEPS 16

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
   {0x80,0xa0,0xd0,0xe0,0xf0,0xe0,0xd0,0xa0,0x70,0x50,0x20,0x10,0,0x10,0x20,0x50};   

static void sinewave(void* arg); 
    
 
    
/*****************************************************************
 * Call periodically from timer ISR to generate a sine wave.
 * This is to be done 16 X the frequency of the generated tone. 
 *****************************************************************/
volatile uint8_t i = 0;

static void IRAM_ATTR sinewave(void *arg) 
{
  clock_clear_intr(TONE_TIMERGRP, TONE_TIMERIDX);
  dac_output_voltage(TONE_DAC, sine[i++]);
  if (i >= STEPS) 
     i=0;
}


/**************************************************************************
 * Init tone generator
 **************************************************************************/
static bool _inited = false; 
void tone_init() {
    if (!_inited) {
        dac_output_enable(TONE_DAC);
        clock_init(TONE_TIMERGRP, TONE_TIMERIDX, CLOCK_DIVIDER, sinewave, false);
        _inited = true;
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
    dac_output_voltage(TONE_DAC, 0);
}


