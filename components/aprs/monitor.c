#include <stdlib.h>
#include "defines.h"
#include "config.h"
#include "ax25.h"
#include "hdlc.h"
#include "system.h"
#include "aprs.h"
   
   
static bool mon_on = false;
static bool mon_ax25 = true; 
static FBQ mon;
static FBUF frame; 
static uint8_t subscription;
static uint8_t txsubscr = 255;



void mon_init()
{
    fbq_init(&mon, HDLC_DECODER_QUEUE_SIZE);
}


/******************************************************************************
 *  Monitor thread 
 *   Just write out incoming frames. 
 ******************************************************************************/

static void monitor (void *arg)
{
    (void) arg; 
    while (mon_on)
    {
        /* Wait for frame and then to AFSK decoder/encoder 
         * is not running. 
         */
        frame = fbq_get(&mon);
        if (!fbuf_empty(&frame)) {
            /* Display it */
            if (mon_ax25)
                ax25_display_frame(stdout, &frame);
            else 
                fbuf_print(stdout, &frame);
            printf("\n");
            sleepMs(10);
        }
    
        /* And dispose the frame. Note that also an empty frame should be disposed! */
        fbuf_release(&frame);    
    }
  
    if (!mon_ax25)
        printf("\n**** Connection closed ****\n");
    sleepMs(100);
    vTaskDelete(NULL);
}



/******************************************************************************
 * Activate/deactivate monitor for AX25 encoded packets
 ******************************************************************************/

void mon_activate(bool m)
{ 
    /* AX.25 or text mode */
    mon_ax25 = true; 
   
    /* Start if not on already */
    bool tstart = m && !mon_on;
   
    /* Stop if not stopped already */
    bool tstop = !m && mon_on;
   
    mon_on = m;
   
    if (tstart) {
        FBQ* mq = (mon_on? &mon : NULL);
        
        subscription = APRS_SUBSCRIBE_RX(mq);
        if (GET_BOOL_PARAM("TXMON.on", DFL_TXMON_ON))
            txsubscr = APRS_SUBSCRIBE_TXMON(mq);

        xTaskCreate(&monitor, "Packet monitor", 
            STACK_MONITOR, NULL, NORMALPRIO, NULL);
    }
    
    
    if (tstop) {
        fbq_signal(&mon, SRC_RX);
        APRS_UNSUBSCRIBE_RX(subscription);
        if (txsubscr != 255)
            APRS_UNSUBSCRIBE_TXMON(txsubscr);
    }
}


