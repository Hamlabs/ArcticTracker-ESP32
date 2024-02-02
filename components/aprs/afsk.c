
#include <stdbool.h>
#include "defines.h"
#include "afsk.h"
#include "system.h"



/* Important: Sample rate must be divisible by bitrate */
/* Sampling clock setup */
#define FREQ2CNT(f) AFSK_RESOLUTION/(f) 
#define AFSK_CNT FREQ2CNT(1200)


/* Common stuff for afsk-RX and afsk-TX. */
static bool    rxMode = false; 
static bool    txOn = false; 
static int     rxEnable = 0;
static mutex_t afskmx; 
static clock_t afskclk;

void afsk_rxSampler(void *arg);
void afsk_txBitClock(void *arg);


/*********************************************************
 * ISR for clock
 *********************************************************/



static bool afsk_sampler(struct gptimer_t * t, const gptimer_alarm_event_data_t * a, void * arg) 
{   
    if (rxMode)
        rxSampler_isr(arg); 
    else
        afsk_txBitClock(arg); 
    return true;
}



/**********************************************************
 *  Init shared clock for AFSK RX and TX 
 **********************************************************/

void afsk_init() 
{
    afskmx = mutex_create();
    clock_init(&afskclk, AFSK_RESOLUTION, AFSK_CNT, afsk_sampler, NULL);
}


/**********************************************************
 * Allow app to enable/disable starting of receiver
 * clock. To save battery. 
 **********************************************************/

void afsk_rx_enable() {
    rxEnable++; 
}
void afsk_rx_disable() {
    if (rxEnable > 0)
       rxEnable--; 
}


/**********************************************************
 * Turn receiving on and off
 * These are called from ISR handlers when squelch opens
 **********************************************************/
 
void afsk_rx_start() {
    if (!rxEnable) 
        return;
    afsk_PTT(false); 
    if (txOn) 
        clock_stop(afskclk);     
    rxMode = true; 
    txOn = false;
    clock_start(afskclk);
}

   
void afsk_rx_stop() {
    if (!rxEnable)
        return;
    if (rxMode) 
        clock_stop(afskclk);  
    rxMode=false; 
    rxSampler_nextFrame(); 
    afsk_rx_newFrame();
}


 
/***********************************************************
 * Turn transmitter clock on/off
 ***********************************************************/
 
void afsk_tx_start() {
    mutex_lock(afskmx);
    if (rxMode) 
        clock_stop(afskclk);
    
    clock_set_interval(afskclk, FREQ2CNT(AFSK_BITRATE));
    clock_start(afskclk);
    txOn = true; 
    rxMode = false;
    mutex_unlock(afskmx);
}

 
 
/***********************************************************
 *  Stop transmitter.
 ***********************************************************/
 
 void afsk_tx_stop() {
    mutex_lock(afskmx);
    if (txOn) 
        clock_stop(afskclk); 
    afsk_PTT(false); 
    txOn = false; 
    if (rxMode) {
        clock_set_interval(afskclk, FREQ2CNT(AFSK_SAMPLERATE));
        clock_start(afskclk);
    }
    mutex_unlock(afskmx);
 }
 
 
