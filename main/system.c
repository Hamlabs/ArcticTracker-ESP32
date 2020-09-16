/*
 * Misc. System related stuff
 * By LA7ECA, ohanssen@acm.org
 */

#include <time.h>
#include <sys/time.h>
#include <string.h>
#include "defines.h" 
#include "esp_wifi.h"
#include "system.h"
#include "networking.h"
#include "config.h"
#include "esp_sntp.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_sleep.h"
#include "ui.h"
#include "lcd.h"
#include "gui.h"
#include "afsk.h"
#include "radio.h"
#include "gps.h"

static void initialize_sntp(void);


#define TAG "system"



 BaseType_t _cond_setBitsI(cond_t cond, BaseType_t bits) {
     BaseType_t hptw = pdFALSE;
     return xEventGroupSetBitsFromISR(cond, bits, &hptw);
 }

 
 
 

/******************************************************************************
 * Upgrade firmware over HTTPS
 ******************************************************************************/

esp_err_t firmware_upgrade()
{ 
    if (!wifi_isConnected()) {
        ESP_LOGW(TAG, "Wifi not connected - cannot update");
        return ESP_OK; 
    }
    afsk_rx_stop(); 
    afsk_tx_stop();
    radio_on(false);
    lcd_backlight();
    gui_fwupgrade();
    sleepMs(500);
    beeps("..-. .--");
    sleepMs(500);
    
    char* fwurl = malloc(80);
    char* fwcert = malloc(BBUF_SIZE+1);
    if (fwurl==NULL || fwcert==NULL) {
        ESP_LOGW(TAG, "Cannot allocate buffer for certificate or url");
        return ESP_FAIL;
    }
    
    GET_STR_PARAM("FW.URL", fwurl, 79);
    GET_STR_PARAM("FW.CERT", fwcert, BBUF_SIZE);
    
    esp_http_client_config_t config = {
        .url = fwurl,
        .cert_pem = fwcert,
    };
    BLINK_FWUPGRADE;    
    esp_err_t ret = esp_https_ota(&config);
    ESP_LOGW(TAG, "Upgrade ok. Rebooting..\n");
    if (ret == ESP_OK) {
        esp_restart();
    } else {
        return ESP_FAIL;
    }
    return ESP_OK;
}             




/******************************************************************************
 * Shutdown - go to deep sleep mode and turn off radio, etc.
 * to use as little battery as possible. 
 ******************************************************************************/

void systemShutdown(void)
{   
    sleepMs(500);
    gui_sleepmode();
    sleepMs(1500);
    blipDown();
    sleepMs(200);
    radio_on(false); 
    esp_deep_sleep_start();
}




/******************************************************************************
 * Get time from NTP
 ******************************************************************************/

static void initialize_sntp(void)
{
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}



/*******************************************************************************
 * Set the real time clock if wifi is connected. 
 *******************************************************************************/

void time_init()
{
    if (getTime() <= 30000000) {
        if (wifi_isConnected()) {
            ESP_LOGI(TAG, "Time is not set yet. Getting time over SNTP.");
            initialize_sntp();
        }
        // FIXME: Get time from GPS if available? 
    } 
    else
        ESP_LOGI(TAG, "Time is already set.");
}



/*******************************************************************************
 * Get time. Return -1 if time is not set
 *******************************************************************************/

time_t getTime() {
    time_t now;
    if (gps_is_fixed())
        now = gps_get_time();
    else
        time(&now);
    return now;
}


/*******************************************************************************
 * Get UTC time. Return true if time is set
 *******************************************************************************/

bool getUTC(struct tm *timeinfo)
{
    time_t now = getTime();
    if (now < 30000000)
        return false; 
    localtime_r(&now, timeinfo);
    return true; 
}



/********************************************************************************
 * Time formatting
 ********************************************************************************/

char* datetime2str(char* buf, time_t time)
{
    struct tm *tm = gmtime(&time);
    switch (tm->tm_mon+1) {
        case  1: sprintf(buf, "Jan"); break;
        case  2: sprintf(buf, "Feb"); break;
        case  3: sprintf(buf, "Mar"); break;
        case  4: sprintf(buf, "Apr"); break;
        case  5: sprintf(buf, "May"); break;
        case  6: sprintf(buf, "Jun"); break;
        case  7: sprintf(buf, "Jul"); break;
        case  8: sprintf(buf, "Aug"); break;
        case  9: sprintf(buf, "Sep"); break;
        case 10: sprintf(buf, "Oct"); break;
        case 11: sprintf(buf, "Nov"); break;
        case 12: sprintf(buf, "Dec"); break;
        default:  sprintf(buf, "???"); ;
    }
    sprintf(buf+3, " %02u %02u:%02u UT", tm->tm_mday, 
        (uint8_t) tm->tm_hour, (uint8_t) tm->tm_min);
    return buf;
}


char* time2str(char* buf, time_t time)
{
    struct tm *tm = gmtime(&time);
    sprintf(buf, "%02u:%02u:%02u", 
      (uint8_t) tm->tm_hour, (uint8_t) tm->tm_min, (uint8_t) tm->tm_sec );
    return buf;
}
 
 
char* date2str(char* buf, time_t time)
{
    struct tm *tm = gmtime(&time);
    sprintf(buf, "%02hu-%02hu-%4hu", (uint8_t) tm->tm_mday, (uint8_t) tm->tm_mon+1, (uint16_t) tm->tm_year+1900);
    return buf;
}




/********************************************************************************
 * Set loglevels from flash config
 ********************************************************************************/

void set_logLevel(char* comp, char* param, esp_log_level_t dfl) {
    esp_log_level_t lvl = get_byte_param(param, dfl);
    ESP_LOGD(TAG, "Set log level for %s = %s", comp, loglevel2str(lvl));
    
    esp_log_level_set(comp, lvl);
}


void set_logLevels() {
    esp_log_level_t dfl = get_byte_param("LGLV.ALL", ESP_LOG_WARN) ; 
    esp_log_level_set("*", dfl);
        
    set_logLevel("system", "LGLV.system", dfl);
    set_logLevel("main", "LGLV.main", dfl);
    set_logLevel("wifi", "LGLV.wifi", dfl);
    set_logLevel("wifix", "LGLV.wifix", dfl);
    set_logLevel("config", "LGLV.config", dfl);
    set_logLevel("httpd", "LGLV.httpd", dfl);
    set_logLevel("shell", "LGLV.shell", dfl);
    set_logLevel("tracker", "LGLV.tracker", dfl);
    set_logLevel("esp-tls", "LGLV.esp-tls", dfl);
    set_logLevel("radio", "LGLV.radio", dfl);
    set_logLevel("ui", "LGLV.ui", dfl);
    set_logLevel("hdlc-enc", "LGLV.hdlc-enc", dfl);
    set_logLevel("hdlc-dec", "LGLV.hdlc-dec", dfl);
    set_logLevel("gps", "LGLV.gps", dfl);
    set_logLevel("uart", "LGLV.uart", dfl);
    set_logLevel("digi", "LGLV.digi", dfl);
    set_logLevel("igate", "LGLV.igate", dfl);
    set_logLevel("tcp-cli", "LGLV.tcp-cli", dfl);
}


bool hasTag(char*tag) {
    return strcmp(tag, "wifi")==0     || strcmp(tag, "wifix")==0   ||
           strcmp(tag, "config")==0   || strcmp(tag, "httpd")==0   ||
           strcmp(tag, "shell")==0    || strcmp(tag, "system")==0  ||
           strcmp(tag, "tracker")==0  || strcmp(tag, "esp-tls")==0 ||
           strcmp(tag, "radio")==0    || strcmp(tag, "ui")==0      ||
           strcmp(tag, "hdlc-enc")==0 || strcmp(tag, "gps")==0     ||
           strcmp(tag, "hdlc-dec")==0 || strcmp(tag, "uart")==0    ||
           strcmp(tag, "digi")==0     || strcmp(tag, "igate")==0   || 
           strcmp(tag, "tcp-cli")==0  || strcmp(tag, "main")==0    ||
           strcmp(tag, "*")==0;
}




/********************************************************************************
 * Convert log level to string
 *******************************************************************************/

char* loglevel2str(esp_log_level_t lvl) {
    switch (lvl) {
        case ESP_LOG_NONE: return "NONE"; 
        case ESP_LOG_ERROR: return "ERROR";     
        case ESP_LOG_WARN: return "WARN";   
        case ESP_LOG_INFO: return "INFO";     
        case ESP_LOG_DEBUG: return "DEBUG";     
        case ESP_LOG_VERBOSE: return "VERBOSE";
    }
    return "UNKNOWN";
}



/********************************************************************************
 * Convert string to log level
 ********************************************************************************/

esp_log_level_t str2loglevel(char* str) 
{
    if (strcasecmp(str, "NONE")==0) return ESP_LOG_NONE; 
    else if (strcasecmp(str, "ERROR")==0) return ESP_LOG_ERROR;
    else if (strcasecmp(str, "WARN")==0) return ESP_LOG_WARN;
    else if (strcasecmp(str, "INFO")==0) return ESP_LOG_INFO;
    else if (strcasecmp(str, "DEBUG")==0) return ESP_LOG_DEBUG;
    else if (strcasecmp(str, "VERBOSE")==0) return ESP_LOG_VERBOSE;    
    ESP_LOGW(TAG, "No corresponding log level for string '%s'", str);
    return ESP_LOG_NONE;
}



/****************************************************************************
 * read line from serial input 
 * Typing ctrl-C will immediately return false
 ****************************************************************************/

bool readline(uart_port_t cbp, char* buf, const uint16_t max) 
{
  char x, xx;
  uint16_t i=0; 
  
  for (i=0; i<max; i++) {
    uart_read_bytes(cbp, (uint8_t*) &x, 1, portMAX_DELAY);     
    if (x == 0x03)     /* CTRL-C */
      return false;
    if (x == '\r') {
      /* Get LINEFEED */
      uart_read_bytes(cbp, (uint8_t*) &xx, 1, portMAX_DELAY);
      break; 
    }
    if (x == '\n')
      break;
    buf[i]=x;
  }
  buf[i] = '\0';
  return true;
}



bool freadline(FILE* f, char* buf, const uint16_t max) 
{
  char x;
  uint16_t i=0; 
  
  for (i=0; i<max; i++) {
    x = fgetc(f);     
    if (x == 0x03)     /* CTRL-C */
      return false;
    if (x == '\r') {
      /* Get LINEFEED */
      fgetc(f);
      break; 
    }
    if (x == '\n')
      break;
    buf[i]=x;
  }
  buf[i] = '\0';
  return true;
}




/****************************************************************************
 * split input string into tokens - returns number of tokens found
 *
 * ARGUMENTS: 
 *   buf       - text buffer to tokenize
 *   tokens    - array in which to store pointers to tokens
 *   maxtokens - maximum number of tokens to scan for
 *   delim     - characters which can be used as delimiters between tokens
 *   merge     - if true, merge empty tokens
 ****************************************************************************/
 
uint8_t tokenize(char* buf, char* tokens[], uint8_t maxtokens, char *delim, bool merge)
{ 
     uint8_t ntokens = 0;
     while (ntokens<maxtokens)
     {    
        tokens[ntokens] = strsep(&buf, delim);
        if ( buf == NULL)
            break;
        if (!merge || *tokens[ntokens] != '\0') 
            ntokens++;
     }
     return (merge && *tokens[ntokens] == '\0' ? ntokens : ntokens+1);
}
