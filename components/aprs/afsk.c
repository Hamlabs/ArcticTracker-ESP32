
#include <stdbool.h>
#include "defines.h"
#include "afsk.h"
#include "system.h"



/* Important: Sample rate must be divisible by bitrate */
/* Sampling clock setup */
#define CLOCK_DIVIDER 16
#define CLOCK_FREQ (TIMER_BASE_CLK / CLOCK_DIVIDER)
#define FREQ(f) (CLOCK_FREQ/(f))

/* Common stuff for afsk-RX and afsk-TX. */
static bool rxMode = false; 
static bool txOn = false; 

void afsk_rxSampler(void *arg);
void afsk_txBitClock(void *arg);


/*********************************************************
 * ISR for clock
 *********************************************************/

static void afsk_sampler(void *arg) 
{
    if (rxMode)
        afsk_rxSampler(arg); 
    else
        afsk_txBitClock(arg); 
}



/**********************************************************
 *  Init shared clock for AFSK RX and TX 
 **********************************************************/

void afsk_init() 
{
    clock_init(AFSK_TIMERGRP, AFSK_TIMERIDX, CLOCK_DIVIDER, afsk_sampler, false);
}




/**********************************************************
 * Turn receiving on and off
 * These are called from ISR handlers when squelch opens
 **********************************************************/
 
void afsk_rx_start() {
    clock_stop(AFSK_TIMERGRP, AFSK_TIMERIDX);     
    rxMode = true; 
    clock_start(AFSK_TIMERGRP, AFSK_TIMERIDX, FREQ(AFSK_SAMPLERATE));
}

   
void afsk_rx_stop() {
    clock_stop(AFSK_TIMERGRP, AFSK_TIMERIDX);  
    rxMode=false; 
    if (txOn)
        clock_start(AFSK_TIMERGRP, AFSK_TIMERIDX, FREQ(AFSK_BITRATE));
}


 
/***********************************************************
 * Turn transmitter clock on/off
 ***********************************************************/
 
void afsk_tx_start() {
    clock_stop(AFSK_TIMERGRP, AFSK_TIMERIDX);
    clock_start(AFSK_TIMERGRP, AFSK_TIMERIDX, FREQ(AFSK_BITRATE));
    txOn = true; 
}

 
 
/***********************************************************
 *  Stop transmitter.
 ***********************************************************/
 
 void afsk_tx_stop() {
    clock_stop(AFSK_TIMERGRP, AFSK_TIMERIDX); 
    txOn = false; 
    if (rxMode)
        clock_start(AFSK_TIMERGRP, AFSK_TIMERIDX, FREQ(AFSK_SAMPLERATE));
 }
 
 
