/*
 * Read and process NMEA data from gps 
 * By LA7ECA, ohanssen@acm.org
 */

#if !defined __DEF_NMEA_H__
#define __DEF_NMEA_H__


#include "defines.h"
#include "driver/uart.h"



/* Position report */
typedef struct _PosData {    
    float    latitude;
    float    longitude;
    float    speed, altitude;
    uint16_t course;
    time_t   timestamp;
} posdata_t;



/* Access to current position. Note that current_time can be 
 * different from timestamp in current_pos since GPS is not always in fix 
 */

extern posdata_t gps_current_pos;
extern time_t    gps_current_time;

#define TIME_STR(buf) time2str((buf), gps_current_time)
#define DATE_STR(buf) date2str((buf), gps_current_time)

#define TIME_HOUR(time) (uint8_t) (((time) / 3600) % 24)

/* GPS API */
void        gps_init(uart_port_t uart);
posdata_t*  gps_get_pos(void);
time_t      gps_get_time(void);
uint32_t    gps_distance(posdata_t*, posdata_t*);
uint16_t    gps_bearing(posdata_t *from, posdata_t *to);
void        gps_mon_pos (void);
void        gps_mon_raw (void);
void        gps_mon_off (void);
bool        gps_is_fixed (void);
bool        gps_wait_fix (uint16_t);
void        gps_on(void);
void        gps_off(void);

/* Position and time formatting */
char*  pos2str_lat(char*, posdata_t*);
char*  pos2str_long(char*, posdata_t*);


#endif



