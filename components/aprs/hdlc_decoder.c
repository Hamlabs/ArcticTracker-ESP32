
/*
 * Adapted from the original Polaric Tracker code. 
 * By LA7DJA, LA7ECA and LA3T
 */
#include "defines.h"
#if !defined(ARCTIC4_UHF)

#include <stdlib.h>
#include "crc16.h"
#include "system.h"
#include "config.h"
#include "hdlc.h"
#include "ax25.h"
#include "fbuf.h"
#include "ui.h"
#include "fifo.h"

#define TAG "hdlc-dec"


static fifo_t* inq;
static fbuf_t fbuf;
static fbq_t* mqueue[3];

static uint16_t tag_seq = 0;
static uint16_t prev_seq = 0;
static uint16_t prev_crc = 0;

static uint8_t get_bit (void); 
static bool crc_match(FBUF*, uint8_t, uint16_t *crc);
static bool success = false; 




bool hdlc_isSuccess() {return success;}


/***********************************************************
 * Subscribe or unsubscribe to packets from decoder
 * packets are put into the given buffer queue.
 ***********************************************************/
 
void hdlc_subscribe_rx(fbq_t* q, uint8_t i)
{
    if (i > 2)
        return;
    if (mqueue[i] != NULL)
        fbq_clear(mqueue[i]);
    mqueue[i] = q;
}





/***********************************************************
 * Get a bit from the demodulator layer.
 * .. or a FLAG
 ***********************************************************/
 
static uint8_t get_bit ()
{
   static uint16_t bits = 0xffff;
   static uint8_t bit_count = 0;
   static uint8_t byte; 
   
   if (bit_count < 8) {
      byte = fifo_get(inq);
      bits |= ((uint16_t) byte << bit_count);
      bit_count += 8;
   }
   if ((uint8_t) (bits & 0x00ff) == HDLC_FLAG) {
      bits >>= 8;
      bit_count -= 8;
      return HDLC_FLAG;
   } 
   else {
      uint8_t bit = (uint8_t) bits & 0x0001;
      bits >>= 1;
      bit_count--;
      return bit;
  }
}




/***********************************************************
 * Main decoder thread.
 ***********************************************************/

static void hdlc_rxdecoder (void* arg)
{  
   uint8_t bit;
   
   /* Sync to next flag */
   flag_sync:
   ESP_LOGD(TAG, "Flag sync"); 
   do {  
      bit = get_bit ();
   }  while (bit != HDLC_FLAG);

   
   /* Sync to next frame */
   frame_sync:
   ESP_LOGD(TAG, "Frame sync"); 
   do {     
      bit = get_bit ();
   }  while (bit == HDLC_FLAG);

   /* Receiving frame */
   uint8_t bit_count = 0;
   uint8_t ones_count = 0;
   uint16_t length = 0;
   uint8_t octet = 0;
   
   ESP_LOGD(TAG, "Start receiving frame"); 
   fbuf_release (&fbuf);       // In case we had an abort or checksum
   fbuf_new(&fbuf, SRC_RX);    // mismatch on the previous frame
   
   do {
      if (length > MAX_HDLC_FRAME_SIZE) 
         goto flag_sync; // Lost termination flag or only receiving noise?
      
      for (bit_count = 0; bit_count < 8; bit_count++) 
      {
         octet = (bit ? 0x80 : 0x00) | (octet >> 1);
         if (bit) ones_count++;   
         else ones_count = 0; 
         if (bit == HDLC_FLAG)    // Not very likely, but could happen
            goto frame_sync;
         bit = get_bit ();
         if (ones_count == 5) {
            if (bit)              // Got more than five consecutive one bits,
               goto flag_sync;    // which is a certain error
            ones_count = 0;
            bit = get_bit();      // Bit stuffing - skip one bit
         }
      }
      fbuf_putChar(&fbuf, octet);
      length++;
   } while (bit != HDLC_FLAG);
   
   
   uint16_t rcrc; 
   
   if (length > AX25_HDR_LEN(0)+2 && crc_match(&fbuf, length, &rcrc)) 
   {     
      /* Ignore if it is a duplicate of the previous frame*/
      if (tag_seq == prev_seq) {
          ESP_LOGI(TAG, "Duplicate frame received. seq=%d, length=%d", tag_seq, length); 
          fbuf_release(&fbuf); 
          fbuf_new(&fbuf, SRC_RX);
          goto frame_sync;
      }
      else
          ESP_LOGI(TAG, "VALID frame received. length=%d", length);
      prev_seq = tag_seq;
      success = true; 
      
      /* Send packets to subscribers, if any. 
       * Note that every receiver should release the buffers after use. 
       * Note also that receiver queues should not share the fbuf, use newRef to create a new reference
       */
      fbuf_removeLast(&fbuf);
      fbuf_removeLast(&fbuf);
      
      if (mqueue[0] || mqueue[1] || mqueue[2]) { 
         if (mqueue[0]) fbq_put( mqueue[0], fbuf);                       /* Monitor */
         if (mqueue[1]) fbq_put( mqueue[1], fbuf_newRef(&fbuf, SRC_RX)); /* Digipeater */
         if (mqueue[2]) fbq_put( mqueue[2], fbuf_newRef(&fbuf, SRC_RX)); /* Igate */
      }
      if (mqueue[0]==NULL)
         fbuf_release(&fbuf); 
      fbuf_new(&fbuf, SRC_RX);
   }
   else if (length > AX25_HDR_LEN(0)+2)
       ESP_LOGI(TAG, "Invalid frame received. length=%d", length);
   
   goto frame_sync; // Two consecutive frames may share the same flag
}



/***********************************************************
 * CRC check
 ***********************************************************/

bool crc_match(FBUF* b, uint8_t length, uint16_t *crc)
{
   /* Calculate FCS from frame content */
   *crc = 0xFFFF;
   uint8_t i;
   for (i=0; i<length-2; i++)
       *crc = _crc_ccitt_update(*crc, fbuf_getChar(b)); 
       
   /* Get FCS from 2 last bytes of frame */
   uint16_t rcrc; 
   fbuf_rseek(b, length-2);
   rcrc = (uint16_t) fbuf_getChar(b) ^ 0xFF;
   rcrc |= (uint16_t) (fbuf_getChar(b) ^ 0xFF) << 8;
 
   return (*crc==rcrc);
}



/*********************************************************************
 * Increment sequence number and filter-tag for incoming frame. 
 * This is used to detect duplicates. This is a hack. It depends on 
 * the queue of input bits is shorter than any valid frame. Alternatively 
 * we could find a way to synchronise with the queeue. 
 *********************************************************************/

void hdlc_next_frame() {
    tag_seq = (tag_seq + 1) % 65000; 
    success = false; 
}




/***********************************************************
 * init hdlc-deoder
 ***********************************************************/

void hdlc_init_decoder (fifo_t* s)
{   
  inq = s;
  mqueue[0] = mqueue[1] = mqueue[2] = NULL;
  fbuf_new(&fbuf, SRC_RX);  
  xTaskCreatePinnedToCore(&hdlc_rxdecoder, "HDLC RX decoder", 
        STACK_HDLC_RXDECODER, NULL, NORMALPRIO, NULL, CORE_HDLC_RXDECODER);
}


#endif
