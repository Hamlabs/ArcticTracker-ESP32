/*
 * Buffers for frames of data.
 * By LA7ECA, ohanssen@acm.org
 */ 


#include "defines.h"
#include "fbuf.h"
#include <string.h>
#include "esp_log.h"



/********************************************************************
 * Storage for packet buffer chains. Works as a kind of memory pool.
 * For each buffer slot we have 
 *    - reference count.
 *    - Length of content in bytes
 *    - Index of next buffer in chain (NILPTR if this is the last)
 *    - Storage for actual content
 *
 *********************************************************************/



typedef struct _slot {
   uint8_t   refcnt; 
   uint8_t   length; 
   fbindex_t next; 
   char      buf[FBUF_SLOTSIZE]; 
} fbslot_t; 


static fbslot_t _pool[FBUF_SLOTS]; 


static fbindex_t _free_slots = FBUF_SLOTS; 

static void(*memFullError)(void) = NULL;

static fbindex_t _split(fbindex_t islot, uint16_t pos);
static fbindex_t _fbuf_newslot (void);
static void _fbuf_releaseslot(fbindex_t i);

fbindex_t fbuf_freeSlots()
   { return _free_slots; }

fbindex_t fbuf_usedSlots()
   { return FBUF_SLOTS - _free_slots; }
   
uint16_t fbuf_freeMem()
   { return _free_slots * FBUF_SLOTSIZE; } 
   
   
   
/*
 * Note: We assume that FBUF objects are not shared between 
 * ISRs/threads. 
 * 
 * We also assume that FBUF objects are not accessed from interrupt handlers. 
 * That may change later. In that case, check if we need to protect parts of 
 * code by using locks/disabling interrupts. slots may be shared between
 * FBUF objects but it is complicated and this should be tested and 
 * analysed more. We should disallow writing to a FBUF that contains
 * shared slots.
 */


void fbuf_errorHandler( void(*f)(void) ) 
  { memFullError = f; }



/******************************************************
    Internal: Allocate a new buffer slot 
 ******************************************************/
 
static fbindex_t _fbuf_newslot ()
{
    fbindex_t i; 
    for (i=0; i<FBUF_SLOTS; i++)
       if (_pool[i].refcnt == 0) 
       {
           _pool[i].refcnt = 1;
           _pool[i].length = 0;
           _pool[i].next = NILPTR; 
           _free_slots--;
           return i; 
       }
    return NILPTR; 
}


/******************************************************
    Internal: Release a buffer slot 
 ******************************************************/

static void _fbuf_releaseslot(fbindex_t i) {
    if (_pool[i].refcnt > 0) {
           _pool[i].refcnt--;
           if (_pool[i].refcnt == 0)
             _free_slots++;
    }
}


/*******************************************************
    initialise a buffer chain
 *******************************************************/
 
void fbuf_new (FBUF* bb)
{
    bb->head = bb->wslot = bb->rslot = _fbuf_newslot();
    bb->rpos = 0;
    bb->length = 0;
}


/*******************************************************
    dispose the content of a buffer chain
 *******************************************************/

void fbuf_release(FBUF* bb)
{
    fbindex_t b = bb->head;
    while (b != NILPTR) {
       _fbuf_releaseslot(b);
       b = _pool[b].next; 
    } 
    bb->head = bb->wslot = bb->rslot = NILPTR;
    bb->rpos = bb->length = 0;
}


/*******************************************************
 *   Create a new reference to a buffer chain
 *******************************************************/

FBUF fbuf_newRef(FBUF* bb)
{
    FBUF newb;
    fbindex_t b = bb->head;
    while (b != NILPTR) 
    {
        _pool[b].refcnt++; 
        b = _pool[b].next; 
    } 
    newb.head = bb->head; 
    newb.length = bb->length; 
    fbuf_reset(&newb);
    newb.wslot = bb->wslot;
    return newb;
}


/*******************************************************
    reset  or set read position of a buffer chain
 *******************************************************/
 
void fbuf_reset(FBUF* b)
{
    b->rslot = b->head; 
    b->rpos = 0;
}

void fbuf_rseek(FBUF* b, const uint16_t pos)
{
   uint16_t i=pos;
   if (pos > b->length)
       return;
   fbuf_reset(b);
   while (i > _pool[b->rslot].length) {
        i -= _pool[b->rslot].length;
        b->rslot = _pool[b->rslot].next;
   }
   b->rpos = i;
}




/*******************************************************
    Add a byte to a buffer chain. 
    Add new slots to it if necessary
 *******************************************************/
 
void fbuf_putChar (FBUF* b, const char c)
{
    /* if wslot is NIL it means that writing is not allowed */
    if (b->wslot == NILPTR)
       return;
    
    uint8_t pos = _pool[b->wslot].length; 
    if (pos == FBUF_SLOTSIZE || b->head == NILPTR)
    {
        pos = 0; 
        fbindex_t newslot = _fbuf_newslot();
        if (newslot == NILPTR) {
            if (memFullError != NULL)
               (*memFullError)();
            else
               return;
        }  
        b->wslot = _pool[b->wslot].next = newslot; 
        if (b->head == NILPTR)
            b->rslot = b->head = newslot;
    }
    _pool[b->wslot].buf [pos] =  c; 
    _pool[b->wslot].length++; 
    b->length++;
}


    

/*******************************************************
 * Insert a buffer chain x into another buffer chain b 
 * at position pos
 * 
 * Note: After calling this, writing into the buffers
 * are disallowed.
 *******************************************************/
 
void fbuf_insert(FBUF* b, FBUF* x, uint16_t pos)
{
    fbindex_t islot = b->head;    
    while (pos >= FBUF_SLOTSIZE) {
        pos -= _pool[islot].length; 
        if (pos > 0) 
            islot = _pool[islot].next;
    }
    
    /* Find last slot in x chain and increment reference count*/
    fbindex_t xlast = x->head;
    _pool[xlast].refcnt++;
    while (_pool[xlast].next != NILPTR) {
        xlast = _pool[xlast].next;
        _pool[xlast].refcnt++;
    }
    
    /* Insert x chain after islot */  
    _pool[xlast].next = _split(islot, pos); 
    _pool[islot].next = x->head;
    
    b->wslot = x->wslot = NILPTR; // Disallow writing
    b->length += x->length;
}




/*****************************************************
 * Connect b buffer to x, at position pos. In practice
 * this mean that we get two buffers, with different
 * headers but with a shared last part. 
 * 
 * Note: After calling this, writing into the buffers
 * is not allowed. 
 *****************************************************/

void fbuf_connect(FBUF* b, FBUF* x, uint16_t pos)
{
    fbindex_t islot = x->head;  
    uint16_t p = pos;
    while (p >= FBUF_SLOTSIZE) {
        p -= _pool[islot].length; 
        if (p > 0) 
            islot = _pool[islot].next;
    }

    /* Find last slot of b and connect it to rest of x */
    fbindex_t xlast = b->head;
    while (_pool[xlast].next != NILPTR) 
        xlast = _pool[xlast].next;

    _pool[xlast].next = _split(islot, p);

    /* Increment reference count of rest of x */
    while (_pool[xlast].next != NILPTR) {
        xlast = _pool[xlast].next;
        _pool[xlast].refcnt++;
    }

    b->wslot = x->wslot = NILPTR; // Disallow writing
    b->length = b->length + x->length - pos;
}



static fbindex_t _split(fbindex_t islot, uint16_t pos)
{
      if (pos == 0)
          return _pool[islot].next;
      fbindex_t newslot = _fbuf_newslot();
      _pool[newslot].next = _pool[islot].next;
      _pool[islot].next = newslot;
      _pool[newslot].refcnt = _pool[islot].refcnt; 
      
      /* Copy last part of slot to newslot */
      for (uint8_t i = 0; i<_pool[islot].length - pos; i++)
          _pool[newslot].buf[i] = _pool[islot].buf[pos+i];   

      _pool[newslot].length = _pool[islot].length - pos;
      _pool[islot].length = pos; 
      return newslot;
}




/*******************************************************
    Write a string to a buffer chain
 *******************************************************/
 
void fbuf_write (FBUF* b, const char* data, const uint16_t size)
{
    uint16_t i; 
    for (i=0; i<size; i++)
        fbuf_putChar(b, data[i]);
}



/*******************************************************
    Write a null terminated string to a buffer chain
 *******************************************************/
 
void fbuf_putstr(FBUF* b, const char *data)
{ 
    while (*data != 0)
        fbuf_putChar(b, *(data++));
}




/*******************************************************
    Read a byte from a buffer chain. 
    (this will increment the read-position)
 *******************************************************/
 
char fbuf_getChar(FBUF* b)
{
    char x = _pool[b->rslot].buf[b->rpos]; 
    if (b->rpos == _pool[b->rslot].length-1)
    {
        b->rslot = _pool[b->rslot].next;
        b->rpos = 0;
    }
    else
        b->rpos++; 
    return x;          
}


/*********************************************************
 * Remove slots from a chain that are read. 
 *********************************************************/

void fbuf_cleanFront(FBUF* b)
{
    while (b->head != b->rslot) {
        fbindex_t hd = b->head;
        b->head = _pool[b->head].next;
        _fbuf_releaseslot(hd);
        b->rpos = 0;
        b->length -= FBUF_SLOTSIZE;
    }
}



/********************************************************
    Print a buffer chain to a stream.
 ********************************************************/ 

void fbuf_print(FBUF* b) 
{
    fbuf_reset(b);
    for (int i=0; i < b->length; i++)
        putchar(fbuf_getChar(b));
}
  
   
  
/**************************************************************
  Read up to size bytes from buffer chain into a string. 
  if 'size' == 0, it will try to read the rest of the buffer 
  chain.    
 **************************************************************/
 
uint16_t fbuf_read (FBUF* b, uint16_t size, char *buf)
{
    uint16_t n; 
    fbindex_t bb, r=0;
    
    if (b->length < size || size == 0)
       size = b->length; 
    bb = b->head; 
    while ( bb != NILPTR )
    {
       n = size - r; 
       if (n > _pool[bb].length) 
           n = _pool[bb].length;
            
       strncpy(buf+r, _pool[bb].buf, n);
       r += n; 
       bb = _pool[bb].next;
       if (r >= size ) 
           break;
    }
    // buf[r] = '\0';
    // Should not null terminate. Return number of read characters. 
    
    return r; 
}


/*******************************************************
  Remove the last byte of a buffer chain.
 *******************************************************/

void fbuf_removeLast(FBUF* x)
{
  register fbindex_t xlast = x->head;
  register fbindex_t prev = xlast;
  
  while (_pool[xlast].next != NILPTR) {
    xlast = _pool[xlast].next;
    if (_pool[xlast].next != NILPTR)
      prev = xlast;
  }
  
  _pool[xlast].length--;
  if (_pool[xlast].length == 0 && prev != xlast) {
    _pool[xlast].refcnt--;
    if (_pool[xlast].refcnt == 0)
      _free_slots++;
    _pool[prev].next = NILPTR;
  }
  x->length--;
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
bool clr = false;
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
        fbuf_new(&x);
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
 
void fbq_signal(FBQ* q)
{
   FBUF b; 
   fbuf_new(&b);
   fbq_put(q, b);
}







