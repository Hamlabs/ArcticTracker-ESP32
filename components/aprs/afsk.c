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
static bool    rxMode = false; 
static bool    txOn = false; 
static bool    rxEnable = false;
static mutex_t afskmx; 

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
    afskmx = mutex_create();
    clock_init(AFSK_TIMERGRP, AFSK_TIMERIDX, CLOCK_DIVIDER, afsk_sampler, false);
}


/**********************************************************
 * Allow app to enable/disable starting of receiver
 * clock. To save battery. 
 **********************************************************/

void afsk_rx_enable() {
    rxEnable = true; 
}
void afsk_rx_disable() {
    rxEnable = false; 
}


/**********************************************************
 * Turn receiving on and off
 * These are called from ISR handlers when squelch opens
 **********************************************************/
 
void afsk_rx_start() {
    
    if (!rxEnable) 
        return;
    mutex_lock(afskmx);
    clock_stop(AFSK_TIMERGRP, AFSK_TIMERIDX);     
    rxMode = true; 
    clock_start(AFSK_TIMERGRP, AFSK_TIMERIDX, FREQ(AFSK_SAMPLERATE));
    mutex_unlock(afskmx);
}

   
void afsk_rx_stop() {
    mutex_lock(afskmx);
    clock_stop(AFSK_TIMERGRP, AFSK_TIMERIDX);  
    rxMode=false; 
    if (txOn)
        clock_start(AFSK_TIMERGRP, AFSK_TIMERIDX, FREQ(AFSK_BITRATE));
    mutex_unlock(afskmx);
}


 
/***********************************************************
 * Turn transmitter clock on/off
 ***********************************************************/
 
void afsk_tx_start() {
    mutex_lock(afskmx);
    clock_stop(AFSK_TIMERGRP, AFSK_TIMERIDX);
    clock_start(AFSK_TIMERGRP, AFSK_TIMERIDX, FREQ(AFSK_BITRATE));
    txOn = true; 
    mutex_unlock(afskmx);
}

 
 
/***********************************************************
 *  Stop transmitter.
 ***********************************************************/
 
 void afsk_tx_stop() {
    mutex_lock(afskmx);
    clock_stop(AFSK_TIMERGRP, AFSK_TIMERIDX); 
    txOn = false; 
    if (rxMode)
        clock_start(AFSK_TIMERGRP, AFSK_TIMERIDX, FREQ(AFSK_SAMPLERATE));
    mutex_unlock(afskmx);
 }
 
 
