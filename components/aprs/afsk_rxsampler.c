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

#define TAG "afsk-rx"

/* 
 * 200 kilobytes can contain 20 seconds of transmission 
 * (when sampling rate is 9600 Hz)
 */ 
#define RX_SAMPLE_BUF_SIZE 200000
#define ADC_FRAGMENT_SIZE 1024 /* 256 samples */


static uint8_t *raw_sample_buf;
static int8_t* sample_buffer; 
static int8_t* start;      // Start of frame
static int8_t* wstart;     // Start of frame under writing
static int8_t* curr;       // Current read position
static int8_t* curr_put;   // Current write position
static int8_t* end_frame;  // End of frame
static int8_t* buf_end;    // End of buffer
static uint16_t length = 0; 
static uint16_t wlength = 0; 



static adcsampler_t adc;




void rxSampler_init() 
{
    adcsampler_init( &adc, RADIO_INPUT);
    adcsampler_calibrate(adc);
    
    raw_sample_buf = malloc(ADC_FRAGMENT_SIZE);   
    sample_buffer = malloc(RX_SAMPLE_BUF_SIZE);
    start = wstart = curr = end_frame = curr_put = sample_buffer;
    buf_end = sample_buffer + RX_SAMPLE_BUF_SIZE-1;
}


extern uint32_t adcsampler_nullpoint;
uint8_t divisor = 16;


static int8_t convertSample(uint32_t smpl) {
    return (int8_t) ((smpl - adcsampler_nullpoint) / divisor); 
}


/* 
 * Get the next frame (or sequence of frames?) from ADC 
 */
int rxSampler_getFrame()
{
    int nresults = 0;
    bool breakout = false; 
    rxSampler_nextFrame();

    /* Start sampling */   
    while (radio_getSquelch() || afsk_isSquelchOff()) {
        /* Get raw fragment from ADC */
        int len = adcsampler_read(adc, raw_sample_buf, ADC_FRAGMENT_SIZE);   
        if (len == -1) 
            continue;
        
        /* Convert its content and add it to frame */
        for (int i = 0; i < len; i += ADC_RESULT_BYTES) {
            adc_digi_output_data_t  *p = ADC_RESULT(raw_sample_buf,i);
            if (ADC_DATA_VALID(p)) { 
                int8_t sample = convertSample(ADC_GET_DATA(p));   
                if (afsk_dcd(sample)) {
                    rxSampler_put(sample);
                    nresults++;
                }
                else if (nresults > 0)
                    breakout = true;
            }
        }    

        /* APRS packets of less than 1500 samples are invalid */
        if (breakout && nresults < 1500) {
            if (nresults > 0) {
                rxSampler_nextFrame();
                nresults = 0;
            }
            breakout = false; 
        }
        else if (breakout)
            break;
        
    }
    return nresults;
}



static bool running = false; 

void rxSampler_start() {
    if (!running)
        adcsampler_start(adc);
    running = true;
}


void rxSampler_stop() {
    if (running)
        adcsampler_stop(adc);
    running = false; 
}


void rxSampler_put(int8_t sample) {
    if (curr_put != start-1 && 
        (curr_put != buf_end+1 || wstart != sample_buffer))
    {
        *curr_put = sample;
        wlength++;
        if (curr_put++ == buf_end-1)
            curr_put = sample_buffer;
    } 
}



/* Get next sample */
int8_t rxSampler_get() {
    register int8_t x = *curr;
    if (curr++ == buf_end-1)
        curr = sample_buffer;
    return x;
}


/* Return true if read has reached end of frame */
bool rxSampler_eof() {
    return (length == 0 || curr == curr_put || (curr != start && curr == wstart));      
}


/* Reset read position to start of frame */
void rxSampler_reset() {
    curr = start;
}


/* Ready for next frame */
void rxSampler_nextFrame() {
    wstart = curr_put;
    wlength = 0;
}


/* Set read to last frame written */
void rxSampler_readLast() {
    curr = start = wstart;
    length = wlength;
}



int rxSampler_length() {
    return length;
}



void print_samples() {
    rxSampler_reset();
    int8_t * ptr;
    int i=0;

    for (ptr = start; ptr < end_frame; ptr++) {
        int8_t val = *ptr;
        printf("%4d, ", val);
        if (++i > 30) {
            printf("\n");
            i=0;
        }
    }
    printf("\n\n");
}

