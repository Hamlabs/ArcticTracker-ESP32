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



inline void fifo_init(fifo_t* f, int8_t* buffer, uint8_t size) {
  f->buffer = buffer; 
  f->size = size;
  f->len = 0;
  f->pos = 0;
}



inline void fifo_push(fifo_t* f, int8_t x) {
  if (f->len >= f->size)
    return;
  f->len++;
  register int8_t i = (f->pos + f->len) % f->size; 
  f->buffer[i] = x; 
}




inline int8_t fifo_pop(fifo_t* f) {
  if (f->len == 0)
     return 0;
  f->len--;
  f->pos = (f->pos + 1) % f->size;
  return f->buffer[f->pos]; 
}


#endif