 
#if !defined __DEF_TRACKER_H__
#define __DEF_TRACKER_H__

#include "gps.h"
#include "fbuf.h"


void tracker_setGate(FBQ* gt);
void tracker_on(void);
void tracker_off(void);
void tracker_posReport(void);
void tracker_init(FBQ* q);
void tracker_addObject(void);
void tracker_clearObjects(void);

void send_extra_report(FBUF*, posdata_t*, char, char);

/* Tracklogger.c */
void tracklog_init();
void tracklog_on();
void tracklog_off();

/* xreport.c - extra report handling */
int32_t signed18bit(int32_t x);
int b64from12bit(FBUF* out, uint16_t x);
int b64from18bit(FBUF* out, uint32_t x);
void xreport_init(); 
void xreport_queue(posdata_t pos, int n);
void xreport_send(FBUF* packet, posdata_t* prev);

#endif
