
#if !defined __TRACKSTORE_H__
#define __TRACKSTORE_H__


#define MAX_UINT16     65000
#define BLOCK_SIZE     512
#define MAX_BLOCKS     512
#define POS_RESOLUTION 100000

/* 
 * 512 * 512 = 262144 records = 4MB data 
 * that is enough for a position every second for 3 days
 */

/*
 * 16 byte position report entry. 
 */
typedef struct _entry {
    uint32_t  time;
    uint32_t  lat, lng; 
    uint16_t  altitude;
    uint16_t  reserved; 
} posentry_t;


typedef uint16_t blkno_t; 

typedef struct _meta {
    uint16_t nblocks;
    uint16_t first, last;
    blkno_t lastblk, firstblk;
} ts_meta_t;


void trackstore_start();
void trackstore_stop();
void trackstore_put(posdata_t *x);
posentry_t* trackstore_getEnt(posentry_t* pbuf);
posdata_t* trackstore_get(posdata_t* pbuf);
void trackstore_reset();

#endif
