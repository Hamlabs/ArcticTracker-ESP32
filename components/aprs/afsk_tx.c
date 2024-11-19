/*
 * AFSK modulation for transmitter. 
 * 
 * Adapted from Polaric Tracker code. 
 * By LA7ECA, ohanssen@acm.org and LA3T.
 */

#include "defines.h"
#if !defined(ARCTIC4_UHF)

#include <stdbool.h>
#include <stdint.h>
#include "afsk.h"
#include "radio.h"
#include "system.h"


static QueueHandle_t oq;
static bool transmit = false; 



/*********************************************************************
 * Turn on/off transmitter and tone generator
 * To be called from ISR
 *********************************************************************/

void afsk_PTT(bool on) 
{
    transmit = on; 
    radio_PTT_I(on);
    
    if (on) 
        tone_start();
    else
        tone_stop();
}



/**************************************************************************
 * Get next bit from stream
 * Note: see also get_bit() in hdlc_decoder.c 
 * Note: The next_byte must ALWAYS be called before get_bit is called
 * to get new bytes from stream. 
 *************************************************************************/

static uint8_t bits;
static uint8_t bit_count = 0;

static uint8_t get_bit(void)
{
    if (bit_count == 0) 
        return 0;
    uint8_t bit = bits & 0x01;
    bits >>= 1;
    bit_count--;
    return bit;
}


static void next_byte(void)
{
    if (bit_count == 0) 
    {
        /* Turn off TX if queue is empty (have reached end of frame) */
        if (xQueueIsQueueEmptyFromISR(oq)) {
            afsk_PTT(false);  
            return;
        }
        xQueueReceiveFromISR( oq, (void *) &bits, NULL);
        bit_count = 8;    
    } 
}



/*******************************************************************************
 * When transmitting, this function should be called periodically, 
 * at same rate as wanted baud rate.
 *
 * It is responsible for transmitting frames by toggling the frequency of
 * the tone generated by the timer handler below. 
 *******************************************************************************/ 

void afsk_txBitClock(void *arg) 
{
    if (!transmit) {
        if (xQueueIsQueueEmptyFromISR(oq))   
            return;
        else {
            /* If bytes in queue, start transmitting */
            next_byte();
            afsk_PTT(true);
        }
    }       
    if ( ! get_bit() ) 
        /* Toggle TX frequency */ 
        tone_toggle(); 
     
    /* Update byte from stream if necessary. We do this 
     * separately, after the get_bit, to make the timing more precise 
     */  
    next_byte();  
}
 

 
 
/***********************************************************
 *  Init.
 ***********************************************************/

QueueHandle_t afsk_tx_init()
{
    tone_init();
    oq =  xQueueCreate(AFSK_TX_QUEUE_SIZE, 1);
    return oq;
}
 
 
#endif
 
