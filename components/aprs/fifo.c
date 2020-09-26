
#include "fifo.h"

/* FIFO of bytes. Max length 256.
 * Classic ringbuffer impl. Not thread safe. 
 */


void fifo_init(fifo_t* f, int8_t* buffer, uint8_t size) {
  f->buffer = buffer; 
  f->size = size;
  f->len = 0;
  f->pos = 0;
}



void fifo_push(fifo_t* f, int8_t x) {
  if (f->len >= f->size)
    return;
  f->len++;
  register int8_t i = (f->pos + f->len) % f->size; 
  f->buffer[i] = x; 
}




int8_t fifo_pop(fifo_t* f) {
  if (f->len == 0)
     return 0;
  f->len--;
  f->pos = (f->pos + 1) % f->size;
  return f->buffer[f->pos]; 
}

