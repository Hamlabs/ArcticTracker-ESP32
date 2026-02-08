#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>




/* 
 * 100 kilobytes can contain 10 seconds of transmission 
 * (when sampling rate is 9600 Hz)
 */ 
#define RX_SAMPLE_BUF_SIZE 20

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

void rxSampler_init() 
{
    sample_buffer = malloc(RX_SAMPLE_BUF_SIZE);
    start = wstart = curr = end_frame = curr_put = sample_buffer;
    buf_end = sample_buffer + RX_SAMPLE_BUF_SIZE-1;
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


void rxSampler_readLast() {
    curr = start = wstart;
    length = wlength;
}


uint16_t rxSampler_length() {
    return length;
}


void printFrame() {
    printf("FRAME LENGTH: %d\n", rxSampler_length());
    while (!rxSampler_eof())
        printf("%d  ", rxSampler_get());
    printf("\n\n");
}




int main() {
    rxSampler_init();
    
    rxSampler_put(1); 
    rxSampler_put(2);
    rxSampler_put(3);
    rxSampler_put(4);

    rxSampler_readLast(); 
    printFrame(); 

    
    rxSampler_nextFrame();

    rxSampler_put(5); 
    rxSampler_put(6);
    rxSampler_put(7);
    rxSampler_put(8);
    rxSampler_put(9); 
    rxSampler_put(10);
    rxSampler_put(11);
    rxSampler_put(12);

    printFrame(); 
    
    rxSampler_reset();
    printFrame(); 

    rxSampler_readLast(); 
    printFrame(); 
    
    rxSampler_nextFrame();
    rxSampler_put(13); 
    rxSampler_put(14);
    rxSampler_put(15);
    rxSampler_put(16);
    rxSampler_put(17); 
    rxSampler_put(18);
    rxSampler_put(19); 
    rxSampler_put(20);
    rxSampler_put(21);
    rxSampler_put(22);
    rxSampler_put(23); 
    rxSampler_put(24);
    
    rxSampler_reset();
    printFrame(); 
    
    rxSampler_readLast(); 
    printFrame(); 
    
    
    
    rxSampler_nextFrame();
    
    rxSampler_put(25); 
    rxSampler_put(26);
    rxSampler_put(27);
    rxSampler_put(28);
    rxSampler_put(29); 
    rxSampler_put(30);
    rxSampler_put(31); 
    rxSampler_put(32);
    
    rxSampler_reset();
    printFrame(); 
    
    rxSampler_readLast(); 
    printFrame(); 
}



