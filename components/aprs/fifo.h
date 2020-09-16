#if !defined __FIFO_H__
#define __FIFO_H__

#include <stdint.h>

/* FIFO of bytes. Max length 256.
 * Classic ringbuffer impl. Not thread safe. 
 */

 
typedef struct _fifo_t {
    uint8_t size; 
    uint8_t len;
    uint8_t pos;
    int8_t* buffer; 
} fifo_t;



void fifo_init(fifo_t* f, int8_t* buffer, uint8_t size);
void fifo_push(fifo_t* f, int8_t x);
int8_t fifo_pop(fifo_t* f);


#endif
