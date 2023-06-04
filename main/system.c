

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
#include "esp_flash.h"
#include "esp_spiffs.h"
#include "ui.h"
#include "gui.h"
#include "afsk.h"
#include "radio.h"
#include "gps.h"
#include "esp_crt_bundle.h"
#include "tracker.h"


static void initialize_sntp(void);


#define TAG "system"



 BaseType_t _cond_setBitsI(cond_t cond, BaseType_t bits) {
     BaseType_t hptw = pdFALSE;
     return xEventGroupSetBitsFromISR(cond, bits, &hptw);
 }

 
 
static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
    }
    return ESP_OK;
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
    disp_backlight();
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
    ESP_LOGI(TAG, "Fw upgrade: URL=%s", fwurl);
    
    esp_http_client_config_t config = {
        .url = fwurl,
        .cert_pem = fwcert,
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
        .skip_cert_common_name_check = false
    };   
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    
    if (fwcert == NULL || strlen(fwcert) < 10) {
        config.cert_pem = NULL;
        config.crt_bundle_attach = esp_crt_bundle_attach;
    }
        
        
    BLINK_FWUPGRADE;    
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGW(TAG, "Fw upgrade ok. Rebooting..");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Fw upgrade failed!");
        return ESP_FAIL;
    }
    return ESP_OK;
}             




/******************************************************************************
 * Battery charger  
 ******************************************************************************/

void batt_init(void)
{
    gpio_set_direction(BATT_CHG_TEST, GPIO_MODE_INPUT);    
    gpio_set_pull_mode(BATT_CHG_TEST, GPIO_FLOATING);
}


static bool charging = false;
bool batt_charge(void)
{
    bool chg = (gpio_get_level(BATT_CHG_TEST) == 1);
    if (chg == charging)
        return chg;
    
    if (chg) {
        tracker_off();
        gps_off();
    }
    else if (GET_BYTE_PARAM("TRACKER.on")) {
        tracker_on();
        gps_on();
    }
    return chg;
}



/******************************************************************************
 * Shutdown - go to deep sleep mode and turn off radio, etc.
 * to use as little battery as possible. 
 ******************************************************************************/

void systemShutdown(void)
{   
    sleepMs(500);
    disp_sleepmode();
    sleepMs(1000);
    blipDown();
    sleepMs(50);
    radio_on(false); 
    esp_deep_sleep_start();
}


/**************************************************
 *  SPIFFS FILESYSTEM
 **************************************************/


#define SPIFFS_LABEL "storage"

esp_vfs_spiffs_conf_t spconf = {
      .base_path = "/files",
      .partition_label = SPIFFS_LABEL,
      .max_files = 10,
      .format_if_mount_failed = true
    };

    
void spiffs_init() {
    /* Register and mount */
    esp_err_t ret = esp_vfs_spiffs_register(&spconf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) 
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        else if (ret == ESP_ERR_NOT_FOUND) 
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        else 
            ESP_LOGE(TAG, "ERROR in mounting filesystem: %d", ret);
    }

    /* Check if SPIFFS fs is mounted */
    if (esp_spiffs_mounted(spconf.partition_label)) 
         ESP_LOGI(TAG, "SPIFFS partition mounted on %s", spconf.base_path);
    
    /* Get and log info */
    size_t size, used;
    ret = esp_spiffs_info(spconf.partition_label, &size, &used);
    if (ret == ESP_OK)
        ESP_LOGI(TAG, "SPIFFS fs: '%s', %d bytes, %d used", spconf.partition_label, size, used);
}


void spiffs_format() {
    if ( esp_spiffs_format(SPIFFS_LABEL) != ESP_OK)
        ESP_LOGW(TAG, "SPIFFS format failed\n");
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
    set_logLevel("trackstore", "LGLV.trackstore", dfl); 
    set_logLevel("tracklog", "LGLV.tracklog", dfl);
    set_logLevel("mbedtls", "LGLV.mbedtls", dfl);
    set_logLevel("rest", "LGLV.rest", dfl);
}


bool hasTag(char*tag) {
    return strcmp(tag, "wifi")==0       || strcmp(tag, "wifix")==0    || strcmp(tag, "uart")==0    || 
           strcmp(tag, "config")==0     || strcmp(tag, "httpd")==0    ||
           strcmp(tag, "shell")==0      || strcmp(tag, "system")==0   ||
           strcmp(tag, "tracker")==0    || strcmp(tag, "esp-tls")==0  ||
           strcmp(tag, "radio")==0      || strcmp(tag, "ui")==0       ||
           strcmp(tag, "hdlc-enc")==0   || strcmp(tag, "gps")==0      ||
           strcmp(tag, "hdlc-dec")==0   || strcmp(tag, "uart")==0     ||
           strcmp(tag, "digi")==0       || strcmp(tag, "igate")==0    || 
           strcmp(tag, "tcp-cli")==0    || strcmp(tag, "main")==0     ||
           strcmp(tag, "trackstore")==0 || strcmp(tag, "tracklog")==0 ||
           strcmp(tag, "mbedtls")==0    || strcmp(tag, "rest")==0  ||
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
