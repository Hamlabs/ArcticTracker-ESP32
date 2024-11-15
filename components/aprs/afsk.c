
#include <stdbool.h>
#include "defines.h"
#include "afsk.h"
#include "system.h"
#include "config.h"



/* Important: Sample rate must be divisible by bitrate */
/* Sampling clock setup */
#define FREQ2CNT(f) AFSK_RESOLUTION/(f) 
#define AFSK_CNT FREQ2CNT(1200)


/* Common stuff for afsk-RX and afsk-TX. */
static bool    squelchOff = false; 
static bool    rxMode = false; 
static bool    txOn = false; 
static int     rxEnable = 0;
static mutex_t afskmx; 
static clock_t afskclk;

void afsk_rxSampler(void *arg);
void afsk_txBitClock(void *arg);


bool afsk_isRxMode() {
    return rxMode;
}


bool afsk_isSquelchOff() {
    return squelchOff; 
}


void afsk_setSquelchOff(bool off) {
    squelchOff = off;
}


/*********************************************************
 * ISR for clock
 *********************************************************/

static bool afsk_sampler(struct gptimer_t * t, const gptimer_alarm_event_data_t * a, void * arg) 
{   
    if (!rxMode)
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
    if (GET_BYTE_PARAM("TRX_SQUELCH") == 0)
        afsk_setSquelchOff(true);
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
            
    /* If transmitter is on, turn it off */ 
    if (txOn) {
        afsk_PTT(false); 
        clock_stop(afskclk);   
        txOn = false;
    }
    rxMode = true; 
    afsk_rx_nextFrame(); 
}

   
void afsk_rx_stop() {
    if (!rxEnable)
        return;
    rxMode=false; 
}


void afsk_rx_nextFrame() {
    rxSampler_nextFrame(); 
    afsk_rx_newFrame();
}


 
/***********************************************************
 * Turn transmitter clock on/off
 ***********************************************************/
 
void afsk_tx_start() {
    mutex_lock(afskmx);    
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
    mutex_unlock(afskmx);
 }
 
 
