/*
 * Read and process NMEA data from gps 
 * By LA7ECA, ohanssen@acm.org
 */

#if !defined __DEF_NMEA_H__
#define __DEF_NMEA_H__


#include "defines.h"
#include "driver/uart.h"


/* Timestamp: Seconds since first day of month 00:00 */
typedef uint32_t timestamp_t; 

typedef struct _Date {
   uint16_t year;
   uint8_t month;
   uint8_t day;
} date_t;

/* Position report */
typedef struct _PosData {    
    float latitude;
    float longitude;
    float speed, altitude;
    uint16_t course;
    timestamp_t timestamp;
} posdata_t;



/* Access to current position. Note that current_time can be 
 * different from timestamp in current_pos since GPS is not always in fix 
 */

extern posdata_t current_pos;
extern timestamp_t current_time;
extern date_t current_date;

#define TIME_STR(buf) time2str((buf), current_time)
#define DATE_STR(buf) date2str((buf), current_date)

#define TIME_HOUR(time) (uint8_t) (((time) / 3600) % 24)

/* GPS API */
void        gps_init(uart_port_t uart);
posdata_t*  gps_get_pos(void);
timestamp_t gps_get_time(void);
date_t      gps_get_date(void);
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
char*  datetime2str(char*, date_t, timestamp_t);
char*  time2str (char*, timestamp_t);
char*  date2str (char*, date_t);

#endif



