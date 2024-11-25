    
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
#include "math.h"


#define TAG "tracker"

posdata_t prev_pos; 
posdata_t prev_pos_gps;
int16_t course=-1, prev_course=-1, prev_gps_course=-1;

static fbq_t* outframes = NULL;   
static fbq_t* gate = NULL; 

static bool maxpause_reached = false;
static uint8_t pause_count = 0;
static bool waited = false;
static uint32_t posreports = 0;

static void activate_tx(void);
static bool should_update(posdata_t*, posdata_t*, posdata_t*);
static uint8_t smartbeacon(uint8_t mindist, float speed, uint8_t minpause);
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



double fabs(double); 
int abs(int);  
double round(double);
double log(double);
long lround(double);


uint32_t tracker_posReports() 
    { return posreports; }


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
    report_station_position(&gps_current_pos, false);
    activate_tx();
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
    
    object_pos[nextObj] = gps_current_pos;
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
    get_str_param("OBJ.ID", id, 10, DFL_OBJ_ID);
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
    sleepMs(2000);
    ESP_LOGI(TAG, "Starting tracker task");
    uint8_t st_count = get_byte_param("STATUSTIME", DFL_STATUSTIME); 
    gps_on();    
    if (!TRACKER_TRX_ONDEMAND)
       radio_require();
    while (GET_BYTE_PARAM("TRACKER.on"))
    {
       /*
        * Wait for a fix on position. But with timeout to allow status and 
        * object reports to be sent. 
        */  
        uint8_t statustime = get_byte_param("STATUSTIME", DFL_STATUSTIME); 
        waited = gps_wait_fix( GPS_TIMEOUT * TRACKER_SLEEP_TIME * TIMER_RESOLUTION);
        if (!gps_is_fixed())
           st_count += GPS_TIMEOUT-1; 
        
        /*
         * Send status report and object reports.
         */
        if (++st_count >= statustime) {
           report_status(&gps_current_pos);
           st_count = 0;
           report_objects(true);
           activate_tx();
        }       

        /*
         * Send position report
         */  
        if (gps_is_fixed()) {
           if (should_update(&prev_pos_gps, &prev_pos, &gps_current_pos)) {
              if (GET_BYTE_PARAM("REPORT.BEEP.on")) 
                 { beep(10); }
            
              report_station_position(&gps_current_pos, false);
              prev_pos = gps_current_pos;                      
           }
           else
              report_station_position(&gps_current_pos, true);
        
           prev_pos_gps = gps_current_pos;
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
    trackert = NULL;
    vTaskDelete(NULL);
}



/***************************************************************
 * Init tracker. gps_init should be called first.
 ***************************************************************/
 
void tracker_init(FBQ *q)
{
    xreport_init();
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
        xTaskCreatePinnedToCore(&tracker, "APRS Tracker", 
            STACK_TRACKER, NULL, NORMALPRIO, &trackert, CORE_TRACKER);
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
#if !defined(ARCTIC4_UHF)
      if (hdlc_enc_packets_waiting()) {
         radio_require(); 
         sleepMs(100);
         radio_release();
      }
#endif
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
    uint16_t turn_limit = get_u16_param ("TURNLIMIT", DFL_TURNLIMIT);
    uint8_t  minpause   = get_byte_param("MINPAUSE",  DFL_MINPAUSE);
    uint8_t  maxpause   = get_byte_param("MAXPAUSE",  DFL_MAXPAUSE);
    uint8_t  mindist    = get_byte_param("MINDIST",   DFL_MINDIST);
    
    uint32_t dist       = (prev->timestamp==0) ? 0 : gps_distance(prev, current);
    uint16_t tdist      = (current->timestamp < prev->timestamp)
                             ? current->timestamp
                             : (current->timestamp - prev->timestamp);
       
    float est_speed  = (tdist==0) ? 0 : ((float) dist / (float) tdist);
     /* Note that est_speed is in m/s while
      * the speed field in  posdata_t is in knots
      */
        
    maxpause_reached = ( ++pause_count * TRACKER_SLEEP_TIME >= maxpause);
    
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
        /* 
         * If previous gps-pos hasn't been reported already and most of the course change
         * has happened the last period, we may add it to the transmission 
	     */
        if ( GET_BYTE_PARAM("EXTRATURN.on") != 0 &&
                prev_gps->timestamp != prev->timestamp && course >= 0 && 
                prev_gps_course >= 0 &&
                course_change(course, prev_gps_course, turn_limit*0.5)
            )
            xreport_queue(*prev_gps, 1);
	 
        prev_course = course;
        pause_count = 0;
        return true;
    }
    if ( maxpause_reached || waited
                
        /* Send report when starting or stopping */             
         || ( pause_count * TRACKER_SLEEP_TIME >= minpause &&
             (( current->speed < 3/KNOTS2KMH && prev->speed > 15/KNOTS2KMH ) ||
              ( prev->speed < 3/KNOTS2KMH && current->speed > 15/KNOTS2KMH )))

        /* Distance threshold on low speeds */
         || ( pause_count * TRACKER_SLEEP_TIME >= minpause && est_speed <= 1 && dist >= mindist )
         
        /* Time period based on average speed */
         || ( est_speed>0 && pause_count >= smartbeacon(mindist, est_speed, minpause ))
        )
       
    {
       pause_count = 0;
       prev_course = course;
       return true;
    }
    return false;
}




static uint8_t smartbeacon(uint8_t mindist, float speed, uint8_t minpause) {
    float k = 0.5;
    if (minpause > 30)
        k = 0.3;
    
    float x = mindist - log10f(speed*k) * (mindist-32); 
    if (speed < 4)
        x -= (5-speed)*4;
    if (x < minpause)
        x = minpause;
    return (uint8_t) (x / TRACKER_SLEEP_TIME);
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
    ESP_LOGI(TAG, "Report status");
    
    /* Create packet header */
    send_header(&packet, false);  
    fbuf_putChar(&packet, '>');
    send_timestamp_z(&packet, pos); 
    
    /* 
     * Get battery voltage - This should perhaps not be here but in status message or
     * telemetry message instead. 
     */
    char vbatt[7];
    sprintf(vbatt, "%.1f%c", ((double) batt_voltage()/1000 ), '\0');

    
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
    ESP_LOGI(TAG, "Report position %s", (no_tx ? ": no_tx" : ""));
    char sym[3];
    get_str_param("SYMBOL", sym, 3, DFL_SYMBOL);

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
       
    /* 
     * Add extra reports from buffer 
     */
    if (!no_tx) 
        xreport_send(&packet, pos);
       
    /* Re-send report in later transmissions */
    uint8_t repeat = GET_BYTE_PARAM("REPEAT"); 
    if (!no_tx && repeat > 0) {
        xreport_queue(*pos, 1);
        if (repeat>1)
            xreport_queue(*pos, 3);
        if (repeat>2)
            xreport_queue(*pos, 6);
    }

    /* Comment */
    if (ccount-- == 0) 
    {
       get_str_param("REP.COMMENT", comment, 40, DFL_REP_COMMENT);
       if (*comment != '\0') {
          fbuf_putChar (&packet, ' ');     /* Or should it be a slash ??*/
          fbuf_putstr (&packet, comment);
       }
       ccount = COMMENT_PERIOD; 
    }
    
    /* Send packet.
     *   send it on radio if no_tx flag is set
     *   put it on igate-queue (if igate is active)
     */
    uint8_t igtrack = GET_BYTE_PARAM("IGATE.TRACK.on"); 
    
    if (!no_tx) 
       fbq_put(outframes, fbuf_newRef(&packet));
    if (gate != NULL && igtrack) 
       fbq_put(gate, packet);
    else {
       /* Add report to log if not igated */
    //   log_put(pos, sym, symtab); 
       fbuf_release(&packet);
    }
    if (!no_tx)
        posreports++;
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
    get_str_param("OBJ.SYMBOL", osym, 2, DFL_OBJ_SYMBOL); 
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
    char pbuf[18];
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
    uint32_t v = (is_longitude ? 190463 *(180+pos) : 380926 *(90-pos)); // FIXME
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
    
    get_str_param("MYCALL", call, 10, DFL_MYCALL); 
    str2addr(&from, call, false); 
    get_str_param("DEST", call, 10, DFL_DEST);
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
        get_str_param("DIGIPATH", dpath, 50, DFL_DEST); 
        ndigis = str2digis(digis, dpath); 
    }
    ax25_encode_header(packet, &from, &to, digis, ndigis, FTYPE_UI, PID_NO_L3);
}



static void send_timestamp(FBUF* packet, posdata_t* pos)
{
    char ts[12];
    sprintf(ts, "%02u%02u%02u%c%c", 
       (uint8_t) ((pos->timestamp / 3600) % 24), 
       (uint8_t) ((pos->timestamp / 60) % 60), 
       (uint8_t) (pos->timestamp % 60), 'h', '\0' );
    fbuf_putstr(packet, ts);   
}


static void send_timestamp_z(FBUF* packet, posdata_t* pos)
{
    char ts[13];
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
