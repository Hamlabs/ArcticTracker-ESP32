/*
 * Enchode HDLC (AX.25) frames.
 * By LA7ECA, ohanssen@acm.org and LA3T
 */

#include "crc16.h"
#include "defines.h"
#include <stdlib.h>
#include "hdlc.h"
#include "system.h"
#include "radio.h"
#include "config.h"
#include "ax25.h"
#include "ui.h"


#define TAG "hdlc-enc"

QueueHandle_t outqueue; 
FBQ encoder_queue; 
FBQ *mqueue = NULL;
static FBUF buffer; 

#define BUFFER_EMPTY (fbuf_eof(&buffer))            


#define PERSISTENCE 80 /* p = x/255 */
#define SLOTTIME    10 /* Milliseconds/10 */

#define WAIT_IDLE xSemaphoreTake(enc_idle, portMAX_DELAY)
#define SIGNAL_IDLE xSemaphoreGive(enc_idle)

static SemaphoreHandle_t enc_idle; 
static bool hdlc_idle = true;
static void hdlc_encode_frames(void);
static void hdlc_encode_byte(uint8_t txbyte, bool flag);


void hdlc_monitor_tx(FBQ* m)
   { mqueue = m; }

fbq_t* hdlc_get_encoder_queue()
   { return &encoder_queue; }

bool hdlc_enc_packets_waiting()
   { return !fbq_eof(&encoder_queue) || !BUFFER_EMPTY; }

void hdlc_wait_idle()
   { while (!hdlc_idle) WAIT_IDLE; }

   
   
   
/*******************************************************
 * Pseudo random function.
 *******************************************************/

static uint64_t seed = 123456789;
uint8_t rand_u8()
{
   seed = (1103515245 * seed + 12345) % 2147483647;
   return seed % 256;
}


/*******************************************************
 * Code for generating a test signal
 *******************************************************/

static bool test_active;
static uint8_t testbyte;
static TaskHandle_t testt = NULL; 

static void hdlc_testsignal(void* arg)
{  
   (void)arg;
   hdlc_idle = false;
  
   while(test_active) 
        xQueueSend(outqueue, &testbyte, portMAX_DELAY);
   hdlc_idle = true;    
   vTaskDelete(NULL);
}


void hdlc_test_on(uint8_t b)
{ 
   testbyte = b;
   test_active = true;
   xTaskCreatePinnedToCore(&hdlc_testsignal, "HDLC TX Testsignal", 
        STACK_HDLC_TEST, NULL, NORMALPRIO, &testt, CORE_HDLC_TEST);
}


void hdlc_test_off()
  { test_active=false; }



/*******************************************************************************
 * TX encoder thread
 *
 * This function gets a frame from buffer-queue, and starts the transmitter
 * as soon as the channel is free.   
 *******************************************************************************/

static void hdlc_txencoder (void* arg)
{ 
  (void)arg;

  while (true)  
  {
     /* Get frame from buffer-queue when available. 
      * This is a blocking call.
      */  
     buffer = fbq_get(&encoder_queue); 
     ESP_LOGI(TAG, "Got frame..");

     /* Wait until channel is free 
      * P-persistence algorithm 
      */
     radio_wait_enabled(); 
     hdlc_idle = false;
     for (;;) {
        wait_channel_ready(); 
        uint8_t r  = rand_u8();    
        if (r > PERSISTENCE)
           sleepMs(SLOTTIME*10); 
        else 
           break;
      } 
      tx_led_on();
      hdlc_encode_frames();
      hdlc_idle = true; 
      SIGNAL_IDLE;
      sleepMs(50);
      wait_tx_off();
      tx_led_off();
  }
}



/*************************************************************
 * Initialize hdlc encoder
 *************************************************************/

FBQ* hdlc_init_encoder(QueueHandle_t oq) 
{
  outqueue = oq;
  enc_idle = xSemaphoreCreateBinary();
  fbq_init(&encoder_queue, HDLC_ENCODER_QUEUE_SIZE);
  xTaskCreatePinnedToCore(&hdlc_txencoder, "HDLC TX Encoder", 
        STACK_HDLC_TXENCODER, NULL, NORMALPRIO, NULL, CORE_HDLC_TXENCODER);
  return &encoder_queue; 
}




/*******************************************************************************
 * HDLC encode and transmit one or more frames (one single transmission)
 * It is responsible for computing checksum, bit stuffing and for adding 
 * flags at start and end of frames.
 *******************************************************************************/

static void hdlc_encode_frames()
{
    uint16_t crc = 0xffff;
    uint8_t txbyte, i; 
    uint8_t txdelay = get_byte_param("TXDELAY", DFL_TXDELAY);
    uint8_t txtail  = get_byte_param("TXTAIL", DFL_TXTAIL);
    uint8_t maxfr   = get_byte_param("MAXFRAME", DFL_MAXFRAME);

    ESP_LOGI(TAG, "Encode frame(s)..");
   
    /* Preamble of TXDELAY flags */
    for (i=0; i<txdelay; i++)
        hdlc_encode_byte(HDLC_FLAG, true);

    for (i=0;i<maxfr;i++) 
    { 
        fbuf_reset(&buffer);
        crc = 0xffff;

        while(!BUFFER_EMPTY)
        {
            txbyte = fbuf_getChar(&buffer);
            crc = _crc_ccitt_update (crc, txbyte);
            hdlc_encode_byte(txbyte, false);
        }
            
        if (mqueue != NULL) {
            /* 
             * Put packet on monitor queue, if active
             */
            fbq_put(mqueue, buffer);
        }
        else {
            ESP_LOGI(TAG, "Release frame.");
            fbuf_release(&buffer);   
        }
        hdlc_encode_byte(crc^0xFF, false);       // Send FCS, LSB first
        hdlc_encode_byte((crc>>8)^0xFF, false);  // MSB
        
        break;
    
        if (!fbq_eof(&encoder_queue) && i < maxfr) {
            hdlc_encode_byte(HDLC_FLAG, true);
            buffer = fbq_get(&encoder_queue); 
            ESP_LOGI(TAG, "Add frame to transmission..");
        }
        else
            break;
    }

    /* Postamble of TXTAIL flags */  
    for (i=0; i<txtail; i++)
        hdlc_encode_byte(HDLC_FLAG, true);
}




/*******************************************************************************
 * HDLC encode and transmit a single byte. Includes bit stuffing if not flag
 *******************************************************************************/
 
 static void hdlc_encode_byte(uint8_t txbyte, bool flag)
 {    
    static uint8_t outbits = 0;
    static uint8_t outbyte;
   
    for (uint8_t i=1; i<8+1; i++)
    { 
       if (!flag && (outbyte & 0x7c) == 0x7c) 
          i--;
     
       else {
          outbyte |= ((txbyte & 0x01) << 7);
          txbyte >>= 1;  
       }
     
       if (++outbits == 8) {
          xQueueSend(outqueue, &outbyte, portMAX_DELAY);
          outbits = 0;
       }
       outbyte >>= 1;      
    }   
 }
 
