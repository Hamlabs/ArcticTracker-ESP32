/*
 * Adapted from Polaric Tracker code. 
 * By LA7ECA, ohanssen@acm.org
 */

#if !defined __AFSK_H__
 #define __AFSK_H__

 #include "freertos/FreeRTOS.h"
 #include "freertos/queue.h"

 #define AFSK_CLOCK_DIVIDER 16
 
 #define AFSK_BITRATE 1200
 #define AFSK_SAMPLERATE 9600
 
 #define AFSK_MARK  1200
 #define AFSK_SPACE 2200
 
 void tone_init(void);
 void tone_start(void);
 void tone_setHigh(bool hi);
 void tone_toggle(void);
 void tone_stop(void);

 void afsk_init(void); 
 
 
 QueueHandle_t afsk_tx_init(void);
 void afsk_tx_start(void);
 void afsk_tx_stop(void);
 
 QueueHandle_t afsk_rx_init(void);
 void afsk_rx_start(void); 
 void afsk_rx_stop(void);
 
#endif
