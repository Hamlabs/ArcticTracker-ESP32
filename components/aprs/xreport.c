#include "system.h"
#include "gps.h"
#include "fbuf.h"
#include "tracker.h"

#define TAG "tracker"

static const char b64tab[65] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	
	
/*******************************************************
 * Posreport buffer - a queue of positions. 
 *  
 *   - putPos - add a position and remove the 
 *     oldest if necessary.
 *   - getPos - remove and return the oldest position
 *   - posBuf_empty - return true if empty
 *******************************************************/

#define MAX_BUFFERPOS 5
#define NBUFFERS 10

typedef struct {
    posdata_t buf[MAX_BUFFERPOS];
    int8_t nPos; 
    int8_t nextPos;
} posbuf_t; 

static posbuf_t buffers[NBUFFERS];
static int8_t firstbuf = 0;




static void putPos(posbuf_t *pb, posdata_t p)
{
   if (pb->nPos < MAX_BUFFERPOS)
      pb->nPos++;
   pb->buf[pb->nextPos] = p;
   pb->nextPos = (pb->nextPos + 1) % MAX_BUFFERPOS;
}


static posdata_t getPos(posbuf_t *pb) {
    int8_t i = pb->nPos <= pb->nextPos ? 
        pb->nextPos - pb->nPos : MAX_BUFFERPOS + (pb->nextPos - pb->nPos);
    pb->nPos--;
    return pb->buf[i];
}


static int8_t nPos(posbuf_t *pb) {
    return pb->nPos;
}

static bool posBuf_empty(posbuf_t *pb) {
   return (pb->nPos == 0);
}

static void rotateBuf() {
    firstbuf = (firstbuf+1) % NBUFFERS;
}

static posbuf_t* getBuf(int8_t i) {
    return &buffers[ (i+firstbuf) % NBUFFERS ];
}


void xreport_init() {
    for (int i=0; i<NBUFFERS; i++)
        buffers[i].nPos = buffers[i].nextPos = 0;
}



/********************************************************************
 * Queue extra-reports for later transmissions 
 * n is the how long to wait (number of transmissions) to send it
 * 0 means next packet, 1 means the packet after the next packet, etc
 ********************************************************************/

void xreport_queue(posdata_t pos, int n) {
    posbuf_t* buf = getBuf(n);
    putPos(buf, pos);
}



/*********************************************************************
 * Add extra-reports onto this transmission 
 *********************************************************************/

void xreport_send(FBUF* packet, posdata_t* prev) {
    posbuf_t *buf = getBuf(0);

    /* 
     * Use deltas for timestamp (12 bit unsigned), latitude and longitude 
     * (18 bit signed number). Base64 encode these numbers. 
     * This generates 8 characters per record. 
     */
    ESP_LOGI(TAG, "Adding repeated pos reports: %d", nPos(buf) );
    if(!posBuf_empty(buf))
        fbuf_putstr(packet, "/*\0");
    while(!posBuf_empty(buf)) {
        posdata_t pos = getPos(buf);
        uint32_t ts_delta  = (uint32_t) pos.timestamp - (uint32_t) prev->timestamp;
        uint32_t lat_delta = (uint32_t) ((pos.latitude - prev->latitude) * 100000); 
        uint32_t lng_delta = (uint32_t) ((pos.longitude - prev->longitude) * 100000);
        b64from12bit(packet, ts_delta); 
        b64from18bit(packet, signed18bit(lat_delta));
        b64from18bit(packet, signed18bit(lng_delta));
    }
    rotateBuf();
}


/********************************************************************
 * Create a 18 bit signed number
 ********************************************************************/

int32_t signed18bit(int32_t x) {
    if (x < 0)
        return (x & 0x1ffff) | 0x20000;
    else 
        return x & 0x1ffff;
}


/********************************************************************
 * Base64 encode a 12 bit binary value
 ********************************************************************/

int b64from12bit(FBUF* out, uint16_t x) {
    uint8_t ls = x & 0x003f; 
    uint8_t ms = x>>6 & 0x003f;
    fbuf_putChar(out, b64tab[ms]);
    fbuf_putChar(out, b64tab[ls]);
    return 12;  
}


/********************************************************************
 * Base64 encode a 18 bit binary value (no padding - returns 3 chars)
 ********************************************************************/

int b64from18bit(FBUF* out, uint32_t x) {
    uint8_t ms = x>>12 & 0x003f;
    fbuf_putChar(out, b64tab[ms]);
    b64from12bit(out, x); 
    return 18;
}



