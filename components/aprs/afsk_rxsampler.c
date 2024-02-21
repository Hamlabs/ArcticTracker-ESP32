/*
 * AFSK Demodulation. 
 * Get samples from the ADC and store them in a buffer
 * 
 */

      
#include <string.h>
#include "defines.h"
#include "afsk.h"
#include "ui.h"
#include "fifo.h"
#include "radio.h"
#include "system.h"

/* 
 * 100 kilobytes can contain 10 seconds of transmission 
 * (when sampling rate is 9600 Hz)
 */ 
#define RX_SAMPLE_BUF_SIZE 100000

static int8_t* sample_buffer; 
static int8_t* start;      // Start of frame
static int8_t* curr;       // Current read position
static int8_t* end_frame;  // End of frame
static int8_t* curr_put;   // Next pos of input
static int8_t* buf_end;    // End of buffer



void rxSampler_init() {
    sample_buffer = malloc(RX_SAMPLE_BUF_SIZE);
    start = curr = end_frame = curr_put = sample_buffer;
    buf_end = sample_buffer + RX_SAMPLE_BUF_SIZE-1;
}


/* Get next sample */
int8_t rxSampler_get() {
    register int8_t x = *curr;
    if (curr++ == buf_end)
        curr = sample_buffer;
    return x;
}


/* Return true if read has reached end of frame */
bool rxSampler_eof() {
    return (curr == end_frame);        
}


/* Reset read position to start of frame */
void rxSampler_reset() {
    curr = start;
}


/* Ready for next frame */
void rxSampler_nextFrame() {
    start = curr = end_frame;
    end_frame = curr_put;
}


void print_samples() {
    rxSampler_reset();
    int8_t * ptr; 
    for (ptr = start; ptr < end_frame; ptr++)
        printf("%d ", *ptr); 
    printf("\n");
    rxSampler_nextFrame();
}
            
            
/*****************************
 * Sampler ISR 
 *****************************/

#if DEVICE == T_TWR
#define DIVISOR 6
#else
#define DIVISOR 14
#endif


void rxSampler_isr(void *arg) 
{
    /* Get a sample from the ADC */
    if (curr_put != start-1 && 
        (curr_put != buf_end+1 || start != sample_buffer))
    {
        *curr_put = (int8_t) (radio_adc_sample()/DIVISOR);
        if (curr_put++ == buf_end-1)
            curr_put = sample_buffer;
    } 
}


