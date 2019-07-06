
/*
 * Digipeater
 * 
 * Stored parameters used by digipeater (can be changed in command interface)
 *    MYCALL            - my callsign
 *    DIGI_WIDE1.on     - true if wide1/fill-in digipeater mode. Meaning that only WIDE1 alias will be reacted on. 
 *    DIGI_SAR.on       - true if SAR preemption mode. If an alias SAR is found anywhere in the path, it will 
 *                        preempt others (moved first) and digipeated upon.  
 * 
 * Macros for configuration (defined in defines.h)
 *    HDLC_DECODER_QUEUE_SIZE - size (in packets) of receiving queue. Normally 7.
 *    STACK_DIGIPEATER        - size of stack for digipeater task.
 *    STACK_HLIST             - size of stack for tick_thread (for heard list).
 *   
 */

#include "defines.h"
#include <stdlib.h>
#include "config.h"
#include "ax25.h"
#include "afsk.h"
#include "hdlc.h"
#include "ui.h"
#include "radio.h"
#include "freertos/task.h"
#include "heardlist.h"
#include "digipeater.h"
#include <string.h>
#include "esp_log.h"
   
static bool digi_on = false;
static FBQ rxqueue;
static TaskHandle_t digithr;

static fbq_t* outframes; 

static void check_frame(FBUF *f);

#define TAG "digi"

/***********************************************
 *  digipeater main thread 
 ***********************************************/

static void digipeater(void* arg)
{
    sleepMs(5000);
    beeps("-..  "); blipUp();
    while (digi_on)
    {
        /* Wait for frame. 
        */
        FBUF frame = fbq_get(&rxqueue);
        if (fbuf_empty(&frame)) {
            ESP_LOGW(TAG, "Got empty frame"); 
            fbuf_release(&frame); 
            continue;    
        }
    
        /* Do something about it */       
        ESP_LOGI(TAG, "Got frame"); 
        check_frame(&frame);
    
        /* And dispose it */
        fbuf_release(&frame);
    }
    sleepMs(500);
    beeps("-..  "); blipDown();
    sleepMs(100);
    vTaskDelete(NULL);
}



/**********************
 *  digipeater_init
 **********************/

void digipeater_init(fbq_t* out)
{
    fbq_init(&rxqueue, HDLC_DECODER_QUEUE_SIZE);
    outframes = out;
    if (get_byte_param("DIGIPEATER.on", 0))
        digipeater_activate(true);
}



/***************************************************************
 * Activate the digipeater if argument is true
 * Deactivate if false
 ***************************************************************/

void digipeater_activate(bool m)
{ 
    bool tstart = m && !digi_on;
    bool tstop = !m && digi_on;
    digi_on = m;
    FBQ* mq = (digi_on? &rxqueue : NULL);
 
    if (tstart) {
        ESP_LOGI(TAG, "starting.."); 
        /* Subscribe to RX packets and start treads */
        hdlc_subscribe_rx(mq, 1);
        xTaskCreatePinnedToCore(&digipeater, "Digipeater", 
            STACK_DIGI, NULL, NORMALPRIO, &digithr, CORE_DIGI);
        hlist_start();
      
        /* Turn on radio and enable RX */
        radio_require();
        afsk_rx_enable();
    } 
    if (tstop) {
        ESP_LOGI(TAG, "stopping..");
        
        /* Turn off radio and disable RX */
        afsk_rx_disable();
        radio_release();
      
        /* Unsubscribe to RX packets and stop threads */
        fbq_signal(&rxqueue);   
        digithr = NULL;
        hdlc_subscribe_rx(NULL, 1);
    }
}



/*******************************************************************
 * Check a frame if it is to be digipeated
 * If yes, digipeat it :)
 *******************************************************************/

static void check_frame(FBUF *f)
{
   FBUF newHdr;
   char mycall_s[10];
   addr_t mycall, from, to; 
   addr_t digis[7], digis2[7];
   bool widedigi = false;
   uint8_t ctrl, pid;
   uint8_t i, j; 
   int8_t  sar_pos = -1;
   uint8_t ndigis =  ax25_decode_header(f, &from, &to, digis, &ctrl, &pid);
   
   if (hlist_duplicate(&from, &to, f, ndigis))
       return;
   get_str_param("MYCALL", &mycall_s, 10, DFL_MYCALL);
   str2addr(&mycall, mycall_s, false);

   /* Copy items in digi-path that has digipeated flag turned on, 
    * i.e. the digis that the packet has been through already 
    */
   for (i=0; i<ndigis && (digis[i].flags & FLAG_DIGI); i++) 
       digis2[i] = digis[i];   

   /* Return if it has been through all digis in path */
   if (i==ndigis)
       return;

   /* Check if the WIDE1-1 alias is next in the list */
   if (get_byte_param("DIGI.WIDE1.on", 0) 
           && strncasecmp("WIDE1", digis[i].callsign, 5) == 0 && digis[i].ssid == 1)
       widedigi = true; 
  
   /* Look for SAR alias in the rest of the path 
    * NOTE: Don't use SAR-preemption if packet has been digipeated by others first 
    */    
   if (get_byte_param("DIGI.SAR.on", 0) && i<=0)
     for (j=i; j<ndigis; j++)
       if (strncasecmp("SAR", digis[j].callsign, 3) == 0) 
          { sar_pos = j; break; } 
   
   /* Return if no SAR preemtion and WIDE1 alias not found first */
   if (sar_pos < 0 && !widedigi)
      return;
   
   /* Mark as digipeated through mycall */
   j = i;
   mycall.flags = FLAG_DIGI;
   digis2[j++] = mycall; 
   
   
   /* do SAR preemption if requested  */
   if (sar_pos > -1) 
       str2addr(&digis2[j++], "SAR", true);
 
   /* Otherwise, use wide digipeat method if requested and allowed */
   else if (widedigi) {
       i++;
       str2addr(&digis2[j++], "WIDE1", true);
   }

   /* Copy rest of the path, exept the SAR alias (if used) */
   for (; i<ndigis; i++) 
       if (sar_pos < 0 || i != sar_pos)
          digis2[j++] = digis[i];
   
   /* Write a new header -> newHdr */
   fbuf_new(&newHdr);
   ax25_encode_header(&newHdr, &from, &to, digis2, j, ctrl, pid);

   /* Replace header in original packet with new header. 
    * Do this non-destructively: Just add rest of existing packet to new header 
    */
   fbuf_connect(&newHdr, f, AX25_HDR_LEN(ndigis) );

   /* Send packet */
    ESP_LOGI(TAG, "Resend (digipeat) frame"); 
    beeps(". ");
    sleepMs(60);
    fbq_put(outframes, newHdr);  
    sleepMs(200);
}




