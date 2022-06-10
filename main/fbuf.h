/*
 * Buffers for frames of data.
 * By LA7ECA, ohanssen@acm.org
 */ 

#if !defined __FBUF_H__
#define __FBUF_H__

#include <inttypes.h>
#include "defines.h"
#include "system.h"

#define NILPTR 0xFFFF


#define fbuf_t FBUF
#define fbq_t FBQ

typedef uint16_t fbindex_t;

/*********************************
   Packet buffer chain
 *********************************/
typedef struct _fb
{
   fbindex_t head, wslot, rslot; 
   uint16_t  rpos; 
   uint16_t  length;
}
FBUF; 


/****************************************
   Operations for packet buffer chain
 ****************************************/

void     fbuf_init      (void);
void     fbuf_new       (FBUF* b);
FBUF     fbuf_newRef    (FBUF* b);
void     fbuf_release   (FBUF* b);
void     fbuf_reset     (FBUF* b);
void     fbuf_rseek     (FBUF* b, const uint16_t pos);
void     fbuf_putChar   (FBUF* b, const char c);
void     fbuf_write     (FBUF* b, const char* data, const uint16_t size);
void     fbuf_putstr    (FBUF* b, const char *data);
char     fbuf_getChar   (FBUF* b);
//void     fbuf_streamRead(Stream *chp, FBUF* b);
uint16_t fbuf_read      (FBUF* b, uint16_t size, char *buf);
void     fbuf_cleanFront(FBUF* b);
void     fbuf_print     (FBUF* b); 
void     fbuf_insert    (FBUF* b, FBUF* x, uint16_t pos);
void     fbuf_connect   (FBUF* b, FBUF* x, uint16_t pos);
void     fbuf_removeLast(FBUF* b);

fbindex_t fbuf_usedSlots(void);
fbindex_t fbuf_freeSlots(void);
uint32_t fbuf_freeMem(void);

#define fbuf_eof(b) ((b)->rslot == NILPTR)
#define fbuf_length(b) ((b)->length)
#define fbuf_empty(b) ((b)->length == 0)



/*********************************
 *   Queue of packet buffer chains
 *********************************/

typedef struct _fbq
{
  uint8_t size, index, cnt; 
  semaphore_t length, capacity; 
  cond_t lock;
  FBUF *buf; 
} FBQ;



/************************************************
   Operations for queue of packet buffer chains
 ************************************************/

void  fbq_init (FBQ* q, const uint16_t size); 
void  fbq_clear (FBQ* q);
void  fbq_put   (FBQ* q, FBUF b); 
FBUF  fbq_get   (FBQ* q);
void  fbq_signal(FBQ* q);

 
#define fbq_eof(q)    ( sem_getCount(((q)->capacity)) >= (q)->size )
#define fbq_full(q)   ( sem_getCount(((q)->capacity)) == 0 )


#endif /* __FBUF_H__ */
