/*
 * Read and process NMEA data from gnss module
 * By LA7ECA, ohanssen@acm.org
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "system.h"
#include "hdlc.h"
#include "fbuf.h"
#include "ax25.h"
#include "config.h"
#include "ui.h"
#include "gps.h"


#define NMEA_BUFSIZE  82
#define NMEA_MAXTOKENS 18

#define GPS_BUF_SIZE 256
#define TAG "gps"



/* Current position and time */
posdata_t gps_current_pos; 
time_t gps_current_time = 0; 
   

/* Local handlers */
static void do_rmc (uint8_t, char**);
static void do_gga (uint8_t, char**);
static void do_gsa (uint8_t, char**);
static void do_gsv (uint8_t, char**);
static void notify_fix (bool);
static void nmeaListener(void* arg);

/* Local variables */
static char buf[NMEA_BUFSIZE+2];
static bool monitor_pos, monitor_raw; 
static bool is_fixed = false;
static float altitude = -1;
static float pdop = -1;


#define WAIT_FIX(timeout) xSemaphoreTake(enc_idle, (timeout) / portTICK_PERIOD_MS)
#define SIGNAL_FIX xSemaphoreGive(enc_idle)

static SemaphoreHandle_t enc_idle; // Binary semaphore
  // Should we 'give' initially? 


static uart_config_t _serialConfigGps = {
#if DEVICE == ARCTIC4
    .baud_rate  = 115200,
#else
    .baud_rate  = 9600,
#endif
    .data_bits  = UART_DATA_8_BITS,
    .parity     = UART_PARITY_DISABLE,
    .stop_bits  = UART_STOP_BITS_1,
    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};




/*************************************************************************
 * Initialize 
 *
 *************************************************************************/

static uart_port_t _uart; 

void gps_init(uart_port_t uart)
{
    ESP_LOGD(TAG, "gps_init, uart=%d", uart);
    ESP_ERROR_CHECK(uart_param_config(uart, &_serialConfigGps));
    ESP_ERROR_CHECK(uart_set_pin(uart, GPS_TXD_PIN, GPS_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(uart, GPS_BUF_SIZE, 0, 0, NULL, 0));
    _uart = uart; 

    enc_idle = xSemaphoreCreateBinary();
    monitor_pos = monitor_raw = false; 
    xTaskCreatePinnedToCore(&nmeaListener, "NMEA Listener", 
        STACK_NMEALISTENER, NULL, NORMALPRIO, NULL, CORE_NMEALISTENER);    
}


/* 
 * Create a fake GPS position
 */
void gps_fake() 
{
    /* Just set the timestamp */
    gps_current_pos.timestamp = getTime();
    gps_current_pos.latitude = 66.0;
    gps_current_pos.longitude = 15.0;
}




/**************************************************************************
 * Read and process NMEA sentences.
 *    Not thread safe. Use in at most one thread, use lock, or allocate
 *    buf on the stack instead. 
 **************************************************************************/
bool nmea_ok = false;

static void nmeaListener(void* arg)
{
  (void)arg;
  char* argv[16];
  uint8_t argc;
  sleepMs(5000);
  ESP_LOGI(TAG, "NMEA listener starts..");
  
  while (1) {
    int checksum = 0; 
    int c_checksum;
    
    readline(_uart, buf, NMEA_BUFSIZE);

    /* If requested, show raw NMEA packet on screen */
    if (monitor_raw) 
      printf("%s\n", buf);
    
    if (buf[0] != '$')
      continue;
     
    /* For now, ignore PQTM... packets */
    if (strncmp(buf, "$PQTM", 5) == 0)
      continue; 
    
    /* Checksum (optional) */
    uint8_t i = 0;
    for (i=1; i<NMEA_BUFSIZE && buf[i] !='*' && buf[i] != 0 ; i++) 
      checksum ^= buf[i];
    if (buf[i] == '*') {
      buf[i] = 0;
      sscanf(buf+i+1, "%x", &c_checksum);
      if (c_checksum != checksum)
         continue;
    } 
    nmea_ok = true;
    
    /* Split input line into tokens */
    argc = tokenize(buf, argv, NMEA_MAXTOKENS, ",", false); 
    
    /* Select command handler */    
    if (strncmp("RMC", argv[0]+3, 3) == 0)
      do_rmc(argc, argv);
    else if (strncmp("GGA", argv[0]+3, 3) == 0)
      do_gga(argc, argv);   
    else if (strncmp("GSA", argv[0]+3, 3) == 0)
      do_gsa(argc, argv);
    else if (strncmp("GSV", argv[0]+3, 3) == 0)
      do_gsv(argc, argv); 
  }
}



/***********************************************************************
 * Get position, time or date from gps 
 ***********************************************************************/

posdata_t*  gps_get_pos()
   { return &gps_current_pos; }
   
time_t gps_get_time()
   { return gps_current_time; } 
   
float gps_get_pdop()
   { return pdop; }
  
  
  
/****************************************************************
 * Monitoring control
 *   nmea_mon_pos - valid GPRMC position reports
 *   nmea_mon_raw - NMEA packets  
 *   nmea_mon_off - turn it all off
 ****************************************************************/

void gps_mon_pos(void)
   { monitor_pos = true; }
void gps_mon_raw(void)
   { monitor_raw = true; }
void gps_mon_off(void)
   { monitor_pos = monitor_raw = false; }
   

  
/************************************************************************
 * Turn on/off GPS
 ************************************************************************/

void gps_on()
{
    ESP_LOGD(TAG, "GNSS on is initiated");
    notify_fix(false);
}


void gps_off()
{ 
   ESP_LOGD(TAG, "GNSS OFF");
   BLINK_NORMAL;
}



/*************************************************************************
 * Compute distance (in meters) between two gps positions
 * compute bearing based on two gps positions
 *************************************************************************/
 
/* The usual PI/180 constant */
static const double DEG_TO_RAD = 0.017453292519943295769236907684886;

/* Earth's quatratic mean radius for WGS-84 */
static const double EARTH_RADIUS_IN_METERS = 6372797.560856;


/*
 * Computes the arc, in radians, between two WGS-84 positions.
 *   Use the Haversine formula. 
 *   http://en.wikipedia.org/wiki/Law_of_haversines
 */
 
static double arcInRadians(posdata_t *from, posdata_t *to)
{
      double latitudeArc  = (from->latitude - to->latitude) * DEG_TO_RAD;
      double longitudeArc = (from->longitude - to->longitude) * DEG_TO_RAD;
      double latitudeH = sin(latitudeArc * 0.5);
      latitudeH *= latitudeH;
      double lontitudeH = sin(longitudeArc * 0.5);
      lontitudeH *= lontitudeH;
      double tmp = cos(from->latitude * DEG_TO_RAD) * cos(to->latitude * DEG_TO_RAD);
      return 2.0 * asin(sqrt(latitudeH + tmp * lontitudeH));
}



uint32_t gps_distance(posdata_t *from, posdata_t *to)
{
    return (uint32_t) round(EARTH_RADIUS_IN_METERS * arcInRadians(from, to));
}


uint16_t gps_bearing(posdata_t *from, posdata_t *to)
{
    double dLon = (from->longitude - to->longitude) * DEG_TO_RAD;
    double toLat = to->latitude * DEG_TO_RAD;
    double fromLat = from->latitude * DEG_TO_RAD;
    if (dLon == 0 && toLat==fromLat)
       return -1;
    double y = sin(dLon) * cos(from->latitude * DEG_TO_RAD);
    double x = cos(toLat) * sin(fromLat) - sin(toLat) * cos(fromLat) * cos(dLon);
    uint16_t brng = (uint16_t) round(atan2(y, x) / DEG_TO_RAD);
    return (brng + 180) % 360; 
}


    
   
/****************************************************************
 * Convert position NMEA fields to float (degrees)
 ****************************************************************/

static void str2coord(const uint8_t ndeg, const char* str, float* coord)
{
    float minutes;
    char dstring[ndeg+1];

    /* Format [ddmm.mmmmm] */
    strncpy(dstring, str, ndeg);
    dstring[ndeg] = 0;
    
    sscanf(dstring, "%f", coord);      /* Degrees */
    sscanf(str+ndeg, "%f", &minutes);  /* Minutes */
    *coord += (minutes / 60);
}


/****************************************************************
 * Convert position to latlong format
 ****************************************************************/

char* pos2str_lat(char* buf, posdata_t *pos)
{
    /* Format latitude values, etc. */
    char lat_sn = (pos->latitude < 0 ? 'S' : 'N');
    float latf = fabs(pos->latitude);
    
    sprintf(buf, "%02d %05.2f %c%c", 
	(int)latf, (latf - (int)latf) * 60, lat_sn,'\0');
    return buf;
}       
 
char* pos2str_long(char* buf, posdata_t *pos)
{
    /* Format longitude values, etc. */
    char long_we = (pos->longitude < 0 ? 'W' : 'E');
    float longf = fabs(pos->longitude);
    
    sprintf(buf, "%03d %05.2f %c%c", 
        (int)longf, (longf - (int)longf) * 60, long_we, '\0');
    return buf;
}  
    

       
/*****************************************************************
 * Convert date/time NMEA fields (timestamp + date)
 *****************************************************************/
 
static void nmea2time( time_t* t, const char* timestr, const char* datestr)
{
    unsigned int hour, min, sec, day, month, year;
    sscanf(timestr, "%2u%2u%2u", &hour, &min, &sec);
    sscanf(datestr, "%2u%2u%2u", &day, &month, &year);
    
    struct tm tm; 
    tm.tm_year = year + 2000 - 1900;
    tm.tm_mon = month - 1; 
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec; 
    
    *t = timegm(&tm);
}



 
/****************************************************************
 * handle changes in GPS fix - mainly change LED blinking
 ****************************************************************/

void notify_fix(bool lock)  
{
   if (lock != is_fixed)
       ESP_LOGI(TAG, "GNSS state: %s", (lock ? "FIXED" : "SEARCHING"));
   if (!lock) 
       BLINK_GPS_SEARCHING;
   else {
       if (!is_fixed) {
          SIGNAL_FIX;
          time_update();
          /* FIXME: Maybe we should suspend SNTP here */
       }     
       BLINK_NORMAL;
   }
   is_fixed = lock;
}


bool gps_is_fixed()
   { return is_fixed; /*&& GET_BYTE_PARAM("TRACKER.on"); */}
   
  
/* Return true if we waited */   
bool gps_wait_fix(uint16_t timeout)
{ 
     if (is_fixed) 
        return false;      
    WAIT_FIX(timeout==0 ? portMAX_DELAY : timeout);
    return true;
}         



/****************************************************************
 * Handle GSV line
 ****************************************************************/

static void get_sysname(char* name, char syst) 
{
   switch (syst) {
      case 'P': strcpy(name, "GPS"); break;
      case 'L': strcpy(name, "Glonass"); break;
      case 'A': strcpy(name, "Galileo"); break;
      case 'B': strcpy(name, "BeiDou"); break;
      case 'Q': strcpy(name, "QZSS"); break;
      default: strcpy(name, "Unknown"); 
   }
}


uint8_t str2num(char* str) {
   if (strlen(str) == 0)
      return 0;
   return atoi(str);
}




static void do_gsv(uint8_t argc, char** argv)
{
   if (argc < 8)     
       return;
   uint8_t nmsg = atoi(argv[1]);
   uint8_t msgno = atoi(argv[2]);
   uint8_t nsats = atoi(argv[3]);
   char system = argv[0][2];
   for (int i=0; i<nsats; i++) {
      if (argc < 4+4*i+4)
         break;
      char sysname[16]; 
      get_sysname(sysname, system); 
      uint8_t prn = str2num(argv[4+4*i]);
      uint8_t elev = str2num(argv[4+4*i+1]);
      uint8_t asimuth = str2num(argv[4+4*i+2]);
      uint8_t signal = str2num(argv[4+4*i+3]);
      if (signal > 0) 
         ESP_LOGI(TAG, "Satellite: prn=%2d, elevation=%3d, asimuth=%4d, signal=%3d -- %s", 
            prn, elev, asimuth, signal, sysname);
   }
}       




/****************************************************************
 * Handle RMC line
 ****************************************************************/

static void do_rmc(uint8_t argc, char** argv)
{
    static uint8_t lock_cnt = 4;    
    char tbuf[9];
    if (argc < 13 || argc > 14)      /* Ignore if wrong format */
       return;
    
    /* get timestamp */
    nmea2time(&gps_current_time, argv[1], argv[9]);
    
    if (*argv[2] != 'A') { 
       notify_fix(false);            /* Ignore if receiver not in fix */
       lock_cnt = 4;
       return;
    }
    else
      if (lock_cnt > 0) {
         lock_cnt--;
         return;
      }
    
    ESP_LOGD(TAG, "GNSS FIX detected");
    lock_cnt = 1;
    notify_fix(true);
   
    gps_current_pos.timestamp = gps_current_time; 
    
    /* get latitude [ddmm.mmmmm] */
    str2coord(2, argv[3], &gps_current_pos.latitude);  
    if (*argv[4] == 'S')
        gps_current_pos.latitude = -gps_current_pos.latitude;
        
     /* get longitude [dddmm.mmmmm] */
    str2coord(3, argv[5], &gps_current_pos.longitude);  
    if (*argv[6] == 'W')
        gps_current_pos.longitude = -gps_current_pos.longitude;
    
    /* get speed [nnn.nn] */
    if (*argv[7] != '\0')
       sscanf(argv[7], "%f", &gps_current_pos.speed);
    else
       gps_current_pos.speed = 0;
       
    /* get course [nnn.nn] */
    if (*argv[8] != '\0') {
       float x;
       sscanf(argv[8], "%f", &x);
       gps_current_pos.course = (uint16_t) x+0.5;
    }
    else
       gps_current_pos.course = 0;
    gps_current_pos.altitude = altitude;
           
    /* If requested, show position on screen */    
    if (monitor_pos) {
      printf("TIME: %s, POS: lat=%f, long=%f, SPEED: %f km/h, COURSE: %u deg\r\n",  
          time2str(tbuf, gps_current_pos.timestamp, false), 
          gps_current_pos.latitude, gps_current_pos.longitude, 
          gps_current_pos.speed*KNOTS2KMH, gps_current_pos.course);
    }
}



/******************************************* 
 * Get altitude from GGA line
 *******************************************/

static void do_gga(uint8_t argc, char** argv)
{
   if (argc == 15 && *argv[6] > '0')
      sscanf(argv[9], "%f", &altitude);
   else
      altitude = -1; 
}



/******************************************* 
 * Get accuracy from GSA line
 *******************************************/

static void do_gsa(uint8_t argc, char** argv)
{
   if ((argc == 18 || argc==19) && *argv[2] > '1') 
      sscanf(argv[15], "%f", &pdop);
   else
      pdop = -1; 
}

