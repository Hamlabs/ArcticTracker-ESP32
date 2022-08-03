/*
 * AFSK Demodulation. 
 * Get samples from the ADC
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
static int8_t* start;
static int8_t* curr;
static int8_t* end_frame;
static int8_t* curr_put;
static int8_t* buf_end;



void rxSampler_init() {
    sample_buffer = malloc(RX_SAMPLE_BUF_SIZE);
    start = curr = end_frame = curr_put = sample_buffer;
    buf_end = sample_buffer + RX_SAMPLE_BUF_SIZE-1;
}


int8_t rxSampler_get() {
    register int8_t x = *curr;
    if (curr++ == buf_end)
        curr = sample_buffer;
    return x;
}


bool rxSampler_eof() {
    return (curr == end_frame);        
}


void rxSampler_reset() {
    curr = start;
}


void rxSampler_nextFrame() {
    start = curr = end_frame;
    end_frame = curr_put;
}


  
/*****************************
 * Sampler ISR 
 *****************************/

void rxSampler_isr(void *arg) 
{
    /* Get a sample from the ADC */
    if (curr_put != start-1 && 
        (curr_put != buf_end+1 || start != sample_buffer))
    {
        *curr_put = (int8_t) (adc_sample()/14);
        if (curr_put++ == buf_end-1)
            curr_put = sample_buffer;
    } 
}


