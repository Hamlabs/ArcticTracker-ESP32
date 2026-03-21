/*
 * Queue for frame buffers
 * By LA7ECA, ohanssen@acm.org
 */ 


#include "defines.h"
#include "fbuf.h"
#include <string.h>
#include "esp_log.h"


struct fbqsw {
    int size;
    int last;
    FBQ *mq[1];
}; 





/* 
 *  Distribution of packets through a publish and subscribe service. 
 *  Multiple receivers may subscribe and packets will be copied to each of them.
 */   


/****************************************************************************
 * Create a pub-sub service
 ****************************************************************************/

FBQSW_t* fbqsw_create(int capacity) {
    
    FBQSW_t* sw = malloc(sizeof(struct fbqsw) + sizeof(FBQ*) * (capacity-1));
    sw->size = capacity;
    sw->last = -1;
    return sw;
}


/****************************************************************************
 * Subscribe a FBQ
 ****************************************************************************/

int fbqsw_subscribe(FBQSW_t* sw, FBQ * mq) {
    if (sw->last+1 >= sw->size)
        return -1;
    sw->last++;
    sw->mq[sw->last] = mq;
    
    return sw->last;
}

/****************************************************************************
 * Cancel a subscription identified by index returned from subscribe func
 ****************************************************************************/

void fbqsw_unsubscribe(FBQSW_t* sw, int index) {
    if (index < 0 || index >= sw->size)
        return;
    sw->mq[index] = NULL;
    while (sw->mq[sw->last] == NULL)
        sw->last--;
}


/****************************************************************************
 * Distribute (publish) a packet to subscribers
 ****************************************************************************/

uint8_t fbqsw_publish(FBQSW_t* sw, FBUF buf) {
    uint8_t n = 0;
    if (sw->last < 0)
        return 0;
    for (int i=0; i<=sw->last; i++)
        if (sw->mq[i] != NULL) {
            fbq_put(sw->mq[i], (n==0 ? buf : fbuf_newRef(&buf, SRC_DUPLICATE)));
            n++;
        }
    return n;
}


/* 
 *  FBQ: QUEUE OF BUFFER-CHAINS
 */   


/*******************************************************
 *    initialise a queue
 *******************************************************/

void fbq_init(FBQ* q, const uint16_t sz)
{
    q->buf = malloc(sz * sizeof(FBUF));
    if (q->buf == NULL) {
        ESP_LOGE("fbuf", "Failed to allocate memory for queue buffer");
        q->size = 0;
        q->index = 0;
        q->cnt = 0;
        q->length = NULL;
        q->capacity = NULL;
        q->lock = NULL;
        return;
    }
    q->size = sz;
    q->index = 0;
    q->cnt = 0;
  
    q->length = sem_create(0); 
    q->capacity = sem_create(sz);
    q->lock = cond_create();
}


/**************************************************************************** 
 * Clear a queue. Release all items and reset semaphores. 
 * IMPORTANT: Be sure that no thread blocks on the queue when calling this.
 * TODO: Check that this is correct wrt thread behaviour.  
 ****************************************************************************/
static bool clr = false;
void fbq_clear(FBQ* q)
{
    cond_wait(q->lock);
    clr=true;
    uint16_t i;
    for (i = q->index;  i < q->index + sem_getCount(q->length);  i++)
        fbuf_release(&q->buf[(uint8_t) (i % q->size)]);
  
    sem_delete(q->length); 
    q->length = sem_create(0);
    sem_delete(q->capacity); 
    q->capacity = sem_create(q->size);
    q->index = 0;
    q->cnt = 0;
    clr=false; 
}



/********************************************************
 *   put a buffer chain into the queue
 ********************************************************/

void fbq_put(FBQ* q, FBUF b)
{
    if (clr)
        return;
    cond_clear(q->lock);
    if (sem_down(q->capacity) == pdTRUE) {
        q->cnt++;
        uint8_t i = (q->index + q->cnt) % q->size; 
        q->buf[i] = b; 
        sem_up(q->length);
    }
    cond_set(q->lock);
}



/*********************************************************
 *   get a buffer chain from the queue (block if empty)
 *********************************************************/

FBUF fbq_get(FBQ* q)
{
    FBUF x; 
    if (clr) {
        fbuf_new(&x, SRC_UNKNOWN);
        return x;
    }
    cond_clear(q->lock);
    if (sem_down(q->length) == pdTRUE) {  
        q->index = (q->index + 1) % q->size;
        x = q->buf[q->index];
        q->cnt--;
        sem_up(q->capacity);
    }
    cond_set(q->lock);
    return x;
}




/**********************************************************
 * put an empty buffer onto the queue. 
 **********************************************************/
 
void fbq_signal(FBQ* q, uint8_t tag)
{
   FBUF b; 
   fbuf_new(&b, tag);
   fbq_put(q, b);
}







