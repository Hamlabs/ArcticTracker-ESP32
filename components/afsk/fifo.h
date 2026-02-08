#if !defined __FIFO_H__
#define __FIFO_H__

#include <stdint.h>
#include "system.h"

/* FIFO of bytes. Max size: 64k. 
 * Classic ringbuffer impl. Not thread safe. 
 */

 
typedef struct _fifo_t {
    uint16_t size; 
    uint16_t wpos;
    uint16_t pos;
    semaphore_t capacity, elements;
    mutex_t mutex;
    int8_t* buffer; 
} fifo_t;



void fifo_init(fifo_t* f, uint16_t size);
void fifo_put(fifo_t* f, uint8_t x);
uint8_t fifo_get(fifo_t* f);


#endif
