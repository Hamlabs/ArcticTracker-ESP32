 
#if !defined __DEF_TRACKER_H__
#define __DEF_TRACKER_H__

#include "gps.h"


void tracker_setGate(FBQ* gt);
void tracker_on(void);
void tracker_off(void);
void tracker_posReport(void);
void tracker_init(void);
void tracker_addObject(void);
void tracker_clearObjects(void);

void send_extra_report(FBUF*, posdata_t*, char, char);

#endif
