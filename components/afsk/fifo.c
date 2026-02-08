
#include "fifo.h"

/* Blocking FIFO of bytes. Max length 64k
 * Classic ringbuffer impl. Limited to one producer and one consumer! 
 */


void fifo_init(fifo_t* f, uint16_t size) {
    f->buffer = malloc(size); 
    f->size = size;
    f->wpos = 0;
    f->pos = 0;
    f->capacity = sem_create(size);
    f->elements = sem_create(0);
    f->mutex = mutex_create();
}



void fifo_put(fifo_t* f, uint8_t x) 
{
    sem_down(f->capacity);
    mutex_lock(f->mutex);
    
    f->buffer[f->wpos] = x;
    f->wpos = (f->wpos + 1) % f->size;
    
    mutex_unlock(f->mutex);
    sem_up(f->elements);
}




uint8_t fifo_get(fifo_t* f) 
{
    register uint8_t res;
    sem_down(f->elements);
    mutex_lock(f->mutex);
     
    res = f->buffer[f->pos];
    f->pos = (f->pos + 1) % f->size;
  
    mutex_unlock(f->mutex);
    sem_up(f->capacity);
    return res;
}
 
    
