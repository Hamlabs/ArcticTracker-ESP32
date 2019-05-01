/*
 * This is the APRS tracking code
 */
 
#include <string.h>
#include <stdio.h>
#include "defines.h"
#include "config.h"
#include "ax25.h"
#include "hdlc.h"
#include "system.h"
#include "ui.h"
#include "radio.h"
#include "tracker.h"


// FIXME
#define adc_read_batt() 1

#define TAG "tracker"

posdata_t prev_pos; 
posdata_t prev_pos_gps;
int16_t course=-1, prev_course=-1, prev_gps_course=-1;

static fbq_t* outframes = NULL;   
static fbq_t* gate = NULL; 

static bool maxpause_reached = false;
static uint8_t pause_count = 0;
static bool waited = false;

static void activate_tx(void);
static bool should_update(posdata_t*, posdata_t*, posdata_t*);
static bool course_change(uint16_t, uint16_t, uint16_t);
static void report_status(posdata_t*);
static void report_station_position(posdata_t*, bool);
static void report_object_position(posdata_t*, char*, bool);
static void report_objects(bool);

static void send_pos_report(FBUF*, posdata_t*, char, char, bool, bool);
static void send_header(FBUF*, bool);
static void send_timestamp(FBUF* packet, posdata_t* pos);
static void send_timestamp_z(FBUF* packet, posdata_t* pos);
static void send_timestamp_compressed(FBUF* packet, posdata_t* pos);
static void send_latlong_compressed(FBUF*, double, bool);
static void send_csT_compressed(FBUF*, posdata_t*);

static void putPos(posdata_t);
static posdata_t getPos(void);
static bool posBuf_empty(void);



double fabs(double); 
int abs(int);  
double round(double);
double log(double);
long lround(double);


/***********************************************************
 * Set packet queue to igate
 ***********************************************************/

void tracker_setGate(FBQ* gt)
  { gate = gt; }


  
/***********************************************************
 * Force position report
 ***********************************************************/

void tracker_posReport()
{
    if (!gps_is_fixed())
        return;
    report_station_position(&current_pos, false);
    activate_tx();
}




/*******************************************************
 * Posreport buffer - a queue of positions. 
 *  
 *   - putPos - add a position and remove the 
 *     oldest if necessary.
 *   - getPos - remove and return the oldest position
 *   - posBuf_empty - return true if empty
 *******************************************************/

#define MAX_BUFFERPOS 5
static posdata_t pos_buf[MAX_BUFFERPOS];
static int8_t nPos = 0, nextPos = 0;


static void putPos(posdata_t p)
{
   if (nPos < MAX_BUFFERPOS)
      nPos++;
   pos_buf[nextPos] = p;
   nextPos = (nextPos + 1) % MAX_BUFFERPOS;
}

static posdata_t getPos()
{
    int8_t i = nPos <= nextPos ? 
        nextPos - nPos : MAX_BUFFERPOS+(nextPos-nPos);
    nPos--;
    return pos_buf[i];
}

static bool posBuf_empty()
{
   return (nPos == 0);
}



/***********************************************************************
 * Object buffer - store up to 4 object reports
 * 
 *  - tracker_addObject      - Create a new object on current position
 *  - tracker_clearObjects   - Empty object buffer
 *  - tracker_report_object  - Send object report for given object
 ***********************************************************************/

#define MAX_OBJECTS 4

static posdata_t object_pos[MAX_OBJECTS];
static int8_t nObjects = 0, nextObj = 0;
static void report_object(int8_t, bool);

void tracker_addObject()
{  
    if (!gps_is_fixed())
        return;
          
    if (nObjects >= MAX_OBJECTS)  
         report_object(nextObj, false); /* Delete existing object */
    else
       nObjects++;
    
    object_pos[nextObj] = current_pos;
    report_object(nextObj, true);
    nextObj = (nextObj + 1) % MAX_OBJECTS;
    activate_tx();
}


void tracker_clearObjects()
{ 
    report_objects(false);
    nObjects = 0; nextObj = 0;
    activate_tx();
}



static void report_object(int8_t pos, bool add)
{
    uint8_t i = 0;
    char id[11];
    GET_STR_PARAM("OBJ.ID", id, 10);
    uint8_t len = strlen( id );
    if (len>=8) 
       len=8; 
    else for (i=len; i<9; i++)
       id[i] = ' ';
    id[len] = 48+pos;   
    id[9] = '\0';  
    report_object_position(&(object_pos[pos]), id, add); 
}



static void report_objects(bool keep)
{
    for (int8_t i=0; i<nObjects; i++) { 
       int8_t pos = nextObj-i-1; 
       if (pos<0) 
          pos = MAX_OBJECTS + pos;
       report_object(pos, keep);
    }
}




static TaskHandle_t trackert = NULL; 

/***************************************************************
 * main thread for tracking
 ***************************************************************/

static void tracker(void* arg) 
{
    uint8_t t;
    uint8_t st_count = GET_BYTE_PARAM("STATUSTIME"); 
    ESP_LOGI(TAG, "Starting tracker task");
    gps_on();    
    if (!TRACKER_TRX_ONDEMAND)
       radio_require();
    while (GET_BYTE_PARAM("TRACKER.on"))
    {
       /*
        * Wait for a fix on position. But with timeout to allow status and 
        * object reports to be sent. 
        */  
        uint8_t statustime = GET_BYTE_PARAM("STATUSTIME"); 
        waited = gps_wait_fix( GPS_TIMEOUT * TRACKER_SLEEP_TIME * TIMER_RESOLUTION);
        if (!gps_is_fixed())
           st_count += GPS_TIMEOUT-1; 
        
        /*
         * Send status report and object reports.
         */
        if (++st_count >= statustime) {
           report_status(&current_pos);
           st_count = 0;
           report_objects(true);
           activate_tx();
        }       

        /*
         * Send position report
         */  
        if (gps_is_fixed()) {
           if (should_update(&prev_pos_gps, &prev_pos, &current_pos)) {
              if (GET_BYTE_PARAM("REPORT.BEEP.on")) 
                 { beep(10); }
            
              report_station_position(&current_pos, false);
              prev_pos = current_pos;                      
           }
           else
              report_station_position(&current_pos, true);
        
           prev_pos_gps = current_pos;
           activate_tx();
           t = TRACKER_SLEEP_TIME;
 
           t = (t > GPS_FIX_TIME) ?
               t - GPS_FIX_TIME : 1;
        
           sleepMs(t * TIMER_RESOLUTION); 
           sleepMs(GPS_FIX_TIME * TIMER_RESOLUTION);   
        }
    }
    gps_off();
    if (!TRACKER_TRX_ONDEMAND)
        radio_release();
    vTaskDelete(NULL);
    trackert = NULL;
}



/***************************************************************
 * Init tracker. gps_init should be called first.
 ***************************************************************/
 
void tracker_init(FBQ *q)
{
    outframes = q; 
    prev_pos.timestamp=0;
    prev_pos_gps.timestamp=0;
    if (GET_BYTE_PARAM("TRACKER.on"))
        tracker_on();
}



/*********************************************************
 *  Turn tracking on
 *********************************************************/

void tracker_on() 
{
    if (trackert == NULL)
        xTaskCreate(&tracker, "APRS Tracker", 
            STACK_TRACKER, NULL, NORMALPRIO, &trackert);
}


/**********************************************************
 * Turn tracking off 
 **********************************************************/

void tracker_off()
{     
} 



/*********************************************************************
 * Activate transmitter - 
 *  If outgoing packets waiting, turn on transmitter, send packets 
 *  and turn off
 *********************************************************************/

static void activate_tx()
{
      if (hdlc_enc_packets_waiting()) {
         radio_require(); 
         sleepMs(100);
         radio_release();
      }
}



/*********************************************************************
 * SMART BEACONING: 
 * This function should return true if we have moved longer
 * than a certain threshold, changed direction more than a 
 * certain amount or at least a certain time has elapsed since
 * the previous update. 
 *********************************************************************/

static bool should_update(posdata_t* prev_gps, posdata_t* prev, posdata_t* current)
{
    uint16_t turn_limit = GET_U16_PARAM("TURNLIMIT");
    uint8_t  minpause   = GET_BYTE_PARAM("MINPAUSE");
    uint8_t  mindist    = GET_BYTE_PARAM("MINDIST");
    uint32_t dist       = (prev->timestamp==0) ? 0 : gps_distance(prev, current);
    uint16_t tdist      = (current->timestamp < prev->timestamp)
                             ? current->timestamp
                             : (current->timestamp - prev->timestamp);
       
    float est_speed  = (tdist==0) ? 0 : ((float) dist / (float) tdist);
     /* Note that est_speed is in m/s while
      * the speed field in  posdata_t is in knots
      */
        
    maxpause_reached = ( ++pause_count >= GET_BYTE_PARAM("MAXPAUSE"));
    
    /* Send report if bearing has changed more than a certain threshold. 
     *
     * Check for both slow changes and quick changes. In the case of quick
     * changes we may add the previous gps position to the transmission. 
     * We may need to calculate the bearing if speed is too low.
     */
    prev_gps_course = course;
    course = (current->speed > 1 ? current->course 
                                 : gps_bearing(prev_gps, current));

    if ( est_speed > 0.8 && course >= 0 && prev_course >= 0 && 
          course_change(course, prev_course, turn_limit))
    {
        /* If previous gps-pos hasn't been reported already and most of the course change
         * has happened the last period, we may add it to the transmission 
	     */
        if ( GET_BYTE_PARAM("EXTRATURN.on") != 0 &&
                prev_gps->timestamp != prev->timestamp && course >= 0 && 
                prev_gps_course >= 0 &&
                course_change(course, prev_gps_course, turn_limit*0.5)
            )
            putPos(*prev_gps);
	 
        prev_course = course;
        pause_count = 0;
        return true;
    }
    if ( maxpause_reached || waited
                
        /* Send report when starting or stopping */             
         || ( pause_count >= minpause &&
             (( current->speed < 3/KNOTS2KMH && prev->speed > 15/KNOTS2KMH ) ||
              ( prev->speed < 3/KNOTS2KMH && current->speed > 15/KNOTS2KMH )))

        /* Distance threshold on low speeds */
         || ( pause_count >= minpause && est_speed <= 1 && dist >= mindist )
         
        /* Time period based on average speed */
         || ( est_speed>0 && pause_count >= (uint8_t)
            ( (mindist / est_speed) / TRACKER_SLEEP_TIME + minpause*1.4 ))
       )
    {
       pause_count = 0;
       prev_course = course;
       return true;
    }
    return false;
}


static bool course_change(uint16_t crs, uint16_t prev, uint16_t limit)
{
     return ( (abs(crs - prev) > limit) &&
         min (abs((crs-360) - prev), abs(crs - (prev-360))) > limit);
}



/**********************************************************************
 * APRS status report. 
 *  What should we put into this report? Currently, I would 
 *  try the battery voltage and a static text. 
 **********************************************************************/

static void report_status(posdata_t* pos)
{
    FBUF packet;   
    fbuf_new(&packet);
    
    /* Create packet header */
    send_header(&packet, false);  
    fbuf_putChar(&packet, '>');
    send_timestamp_z(&packet, pos); 
    
    /* 
     * Get battery voltage - This should perhaps not be here but in status message or
     * telemetry message instead. 
     */
    char vbatt[7];
    sprintf(vbatt, "%.1f%c", ((float) adc_read_batt()/1000 ), '\0');
    
    /* Send firmware version and battery voltage in status report */
    fbuf_putstr(&packet, "FW=AT ");
    fbuf_putstr(&packet, VERSION_STRING);
    fbuf_putstr(&packet, " / VBATT="); 
    fbuf_putstr(&packet, vbatt);
   
    /* Send packet */
    fbq_put(outframes, packet);
}



/**********************************************************************
 * Report position by sending an APRS packet
 **********************************************************************/

#define ASCII_BASE 33
#define log108(x) (log((x))/0.076961) 
#define log1002(x) (log((x))/0.001998)

extern uint16_t course_count; 
extern fbq_t *mqueue;

static void report_station_position(posdata_t* pos, bool no_tx)
{
    char sym[3];
    GET_STR_PARAM("SYMBOL", sym, 2);

    static uint8_t ccount;
    FBUF packet;    
    char comment[40];
    fbuf_new(&packet); 
          
    /* Create packet header */
    send_header(&packet, no_tx);    
    
    /* APRS Position report body
     * with Timestamp if the parameter is set 
     */
    uint8_t tstamp = GET_BYTE_PARAM("TIMESTAMP.on");
    fbuf_putChar(&packet, (tstamp ? '/' : '!')); 
    if (tstamp)
       send_timestamp(&packet, pos);
    send_pos_report(&packet, pos, sym[1], sym[0], 
       (GET_BYTE_PARAM("COMPRESS.on") != 0), false ); 
       
    /* Add extra reports from buffer 
     * FIXME: Max number of reports - configurable 
     */
    int i=0;
    if (!no_tx) 
       while (!posBuf_empty() && i++ <= 3) {
          posdata_t p = getPos();
          fbuf_putstr(&packet, "/#\0");
          send_extra_report(&packet, &p, sym[1], sym[0]);
       }
       
    /* Re-send report in next transmission */
    if (!no_tx && GET_BYTE_PARAM("REPEAT.on") != 0)
          putPos(*pos);
          /* FIXME: symbol may change? */

    /* Comment */
    if (ccount-- == 0) 
    {
       GET_STR_PARAM("REP.COMMENT", comment, 40);
       if (*comment != '\0') {
          fbuf_putChar (&packet, ' ');     /* Or should it be a slash ??*/
          fbuf_putstr (&packet, comment);
       }
       ccount = COMMENT_PERIOD; 
    }
    
    /* Send packet.
     *   send it on radio if no_tx flag is not set
     *   put it on igate-queue (if igate is active)
     */
    bool igtrack = GET_BYTE_PARAM("IGATE.TRACK.on"); 
    
    if (!no_tx) 
       fbq_put(outframes, fbuf_newRef(&packet));
    if (gate != NULL && igtrack) 
       fbq_put(gate, packet);
    else {
       /* Add report to log if not igated */
    //   log_put(pos, sym, symtab); 
       fbuf_release(&packet);
    }
}


/**********************************************************************
 * Report object position by sending an APRS packet
 **********************************************************************/

static void report_object_position(posdata_t* pos, char* id, bool add)
{
    char osym[3];
    FBUF packet; 
    fbuf_new(&packet);
    
    /* Create packet header */
    send_header(&packet, false);   
    
    /* And report body */
    fbuf_putChar(&packet, ';');
    fbuf_putstr(&packet, id);     /* NOTE: LENGTH OF ID MUST BE EXCACTLY 9 CHARACTERS */ 
    fbuf_putChar(&packet, (add ? '*' : '_')); 
    send_timestamp(&packet, pos);
    GET_STR_PARAM("OBJ.SYMBOL", osym, 2); 
    send_pos_report(&packet, pos, osym[1], osym[0],
        (GET_BYTE_PARAM("COMPRESS.on") != 0), true);
    
    /* Comment field may be added later */

    /* Send packet */
    fbq_put(outframes, packet);
}



/**********************************************************************
 * Generate a position report packet (into a fbuf)
 **********************************************************************/

static void send_pos_report(FBUF* packet, posdata_t* pos, 
                            char sym, char symtab, bool compress, bool simple)
{   
    char pbuf[14];
    if (compress)
    {  
       fbuf_putChar(packet, symtab);
       send_latlong_compressed(packet, pos->latitude, false);
       send_latlong_compressed(packet, pos->longitude, true);
       fbuf_putChar(packet, sym);
       send_csT_compressed(packet, pos);
    }
    else
    {
       /* Format latitude and longitude values, etc. */
       char lat_sn = (pos->latitude < 0 ? 'S' : 'N');
       char long_we = (pos->longitude < 0 ? 'W' : 'E');
       float latf = fabs(pos->latitude);
       float longf = fabs(pos->longitude);
      
       sprintf(pbuf, "%02d%05.2f%c%c", (int)latf, (latf - (int)latf) * 60, lat_sn, '\0');
       fbuf_putstr (packet, pbuf);

       fbuf_putChar(packet, symtab);
       
       sprintf(pbuf, "%03d%05.2f%c%c", (int)longf, (longf - (int)longf) * 60, long_we, '\0');
       fbuf_putstr (packet, pbuf);
       fbuf_putChar(packet, sym); 
       
       if (simple)
          return;
          
       sprintf(pbuf, "%03u/%03.0f%c", pos->course, pos->speed, '\0');
       fbuf_putstr (packet, pbuf); 

       /* Altitude */
       if (pos->altitude >= 0 && GET_BYTE_PARAM("ALTITUDE.on")) {
           uint16_t altd = (uint16_t) round(pos->altitude * FEET2M);
           sprintf(pbuf,"/A=%06u%c", altd, '\0');
           fbuf_putstr(packet, pbuf);
       }
    }  
}



/**********************************************************************
 * Add an EXTRA position report to a packet
 **********************************************************************/

void send_extra_report(FBUF* packet, posdata_t* pos, char sym, char symtab)
{
   send_timestamp_compressed(packet, pos);
   send_pos_report(packet, pos, sym, symtab, true, true);
}


 
/**********************************************************************
 * Compress a latlong position and put it into a fbuf
 **********************************************************************/

static void send_latlong_compressed(FBUF* packet, double pos, bool is_longitude)
{
    uint32_t v = (is_longitude ? 190463 *(180+pos) : 380926 *(90-pos));
    fbuf_putChar(packet, (char) (lround(v / 753571) + ASCII_BASE));
    v %= 753571;
    fbuf_putChar(packet, (char) (lround(v / 8281) + ASCII_BASE));
    v %= 8281;
    fbuf_putChar(packet, (char) (lround(v / 91) + ASCII_BASE));
    v %= 91;
    fbuf_putChar(packet, (char) (lround(v) + ASCII_BASE));   
}



/**********************************************************************
 * Compress course/speed of altitude and put it into a fbuf
 **********************************************************************/

static void send_csT_compressed(FBUF* packet, posdata_t* pos)
/* FIXME: Special case where there is no course/speed ? */
{
    if (pos->altitude >= 0 && GET_BYTE_PARAM("ALTITUDE.on")) {
       /* Send altitude */
       uint32_t alt =  (uint32_t) log1002((double)pos->altitude * FEET2M);
       fbuf_putChar(packet, (char) (lround(alt / 91) + ASCII_BASE));
       alt %= 91;
       fbuf_putChar(packet, (char) (lround(alt) + ASCII_BASE));
       fbuf_putChar(packet, 0x10 + ASCII_BASE);
    }
    else {
       /* Send course/speed (default) */
       fbuf_putChar(packet, pos->course / 4 + ASCII_BASE);
       fbuf_putChar(packet, (char) log108((double) pos->speed+1) + ASCII_BASE); 
       fbuf_putChar(packet, 0x18 + ASCII_BASE);
    }
}



/**********************************************************************
 * Generate an AX.25 header from stored station parameters
 **********************************************************************/

static void send_header(FBUF* packet, bool no_tx)
{
    addr_t from, to; 
    char call[10];
    
    GET_STR_PARAM("MYCALL", call, 9); 
    str2addr(&from, call, false); 
    GET_STR_PARAM("DEST", call, 9);
    str2addr(&to, call, false); 

    addr_t digis[7];
    uint8_t ndigis = 0;
    if (no_tx) {
        ndigis = 1;
        str2addr(&digis[0], "TCPIP", false);
    }
    else {
        /* Convert digi path to AX.25 addr list */
        char dpath[50]; 
        GET_STR_PARAM("DIGIPATH", dpath, 50); 
        ndigis = str2digis(digis, dpath); 
    }
    ax25_encode_header(packet, &from, &to, digis, ndigis, FTYPE_UI, PID_NO_L3);
}



static void send_timestamp(FBUF* packet, posdata_t* pos)
{
    char ts[9];
    sprintf(ts, "%02u%02u%02uh%c", 
       (uint8_t) ((pos->timestamp / 3600) % 24), 
       (uint8_t) ((pos->timestamp / 60) % 60), 
       (uint8_t) (pos->timestamp % 60), '\0' );
    fbuf_putstr(packet, ts);   
}


static void send_timestamp_z(FBUF* packet, posdata_t* pos)
{
    char ts[9];
    sprintf(ts, "%02u%02u%02uz%c", 
       (uint8_t) (pos->timestamp / 86400)+1,
       (uint8_t) ((pos->timestamp / 3600) % 24), 
       (uint8_t) ((pos->timestamp / 60) % 60), '\0' ); 
    fbuf_putstr(packet, ts);   
}


static void send_timestamp_compressed(FBUF* packet, posdata_t* pos)
{
    char  ts[5];
    sprintf(ts, "%c%c%c%c",
       (char) ('0' + ((pos->timestamp / 3600) % 24)), 
       (char) ('0' + ((pos->timestamp / 60) % 60)), 
       (char) ('0' + (pos->timestamp % 60)), '\0' );
    fbuf_putstr(packet, ts);
}
