#include <stdlib.h>
#include "defines.h"
#include "config.h"
#include "ax25.h"
#include "hdlc.h"
#include "freertos/task.h"
   
static bool mon_on = false;
static bool mon_ax25 = true; 
static FBQ mon;
static FBUF frame; 


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
                ax25_display_frame(&frame);
            else 
                fbuf_print(&frame);
            printf("\n");
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
//        hdlc_subscribe_rx(mq, 0);
        if ( GET_BYTE_PARAM("TXMON.on") )
            hdlc_monitor_tx(mq); 
        xTaskCreate(&monitor, "Packet monitor", 
            STACK_MONITOR, NULL, NORMALPRIO, NULL);
    }
    if (tstop) {
        fbq_signal(&mon);
        hdlc_monitor_tx(NULL);
 //       hdlc_subscribe_rx(NULL, 0);
    }
}


/******************************************************************************
 * Activate/deactivate monitor for text format packets
 ******************************************************************************/

FBQ* mon_text_activate(bool m)
{ 
    /* AX.25 or text mode */
    mon_ax25 = false; 
  
    /* Start if not on already */
    bool tstart = m && !mon_on;
  
    /* Stop if not stopped already */
    bool tstop = !m && mon_on;
  
    mon_on = m;
  
    if (tstart) {
        FBQ* mq = (mon_on? &mon : NULL);
        xTaskCreate(&monitor, "Packet monitor", 
            STACK_MONITOR, NULL, NORMALPRIO, NULL);
        return mq;
    }
    if (tstop) 
        fbq_signal(&mon);
    return NULL;
}


