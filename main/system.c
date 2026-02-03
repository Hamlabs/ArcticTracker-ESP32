
/*
 * Misc. System related stuff
 * (c) By LA7ECA, ohanssen@acm.org
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
#include "gui.h"
#include "afsk.h"
#include "radio.h"
#include "gps.h"
#include "esp_crt_bundle.h"
#include "tracker.h"
#if defined USE_PMU
#include "pmu.h"
#endif


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
#if !defined(ARCTIC4_UHF)
    afsk_rx_stop(); 
    afsk_tx_stop();
    radio_on(false);
#endif
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
    
    get_str_param("FW.URL", fwurl, 79, DFL_FW_URL);
    get_str_param("FW.CERT", fwcert, BBUF_SIZE, NULL);
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
 * Battery management
 * FIXME: Consider moving this part to the pmu component
 ******************************************************************************/

void batt_init(void)
{
#if defined USE_PMU
    pmu_init();
    pmu_power_setup();
    pmu_batt_setup();
    ESP_LOGI(TAG, "Power/battery management enabled - Arctic-4 or T-TWR board used");
#else
    ESP_LOGI(TAG, "No power managment. Old board used");
    gpio_set_direction(BATT_CHG_TEST, GPIO_MODE_INPUT);    
    gpio_set_pull_mode(BATT_CHG_TEST, GPIO_FLOATING);
#endif
}


bool batt_charge(void)
{
#if defined USE_PMU
    return pmu_isCharging();
#else
    return (gpio_get_level(BATT_CHG_TEST) == 1);
#endif
}



/* Battery status */

int16_t batt_voltage(void) 
{
#if defined USE_PMU
    return pmu_getBattVoltage();
#else
    return 0;
    // FIXME: ADC reading required - see earlier versions
#endif
}



#if !defined USE_PMU
/* Charging profile for 2S LiPo battery */
static const uint16_t volt2s[] = 
{ 8400, 8300, 8220, 8160, 8050, 7970, 7910, 7830, 7750, 7710, 7670, 
  7630, 7590, 7570, 7530, 7490, 7450, 7410, 7370, 7220, 6550 };
#endif
  
  
int16_t batt_percent(void) 
{
#if defined USE_PMU
    return pmu_getBattPercent();
#else
    uint8_t p = 100;
    uint8_t chg = (batt_charge() ? 90 : 0);
    int16_t vbatt = batt_voltage();
    
    for (int i=0; i<21; i++)
        if (vbatt < volt2s[i]+chg)
            p -= 5;
        else
            break;
    return p;
#endif
}



/*************************************************************************
 * Textual description of battery status
 *************************************************************************/

int16_t batt_status(char* line1, char* line2)
{    
    int16_t pbatt = batt_percent();
    int16_t vbatt = batt_voltage();
    if (line2)
        sprintf(line2, " ");
    
    if (pbatt > 90) {
        if (line1) sprintf(line1, "Max (charged)");
    }
    else if (pbatt > 70) { 
        if (line1) sprintf(line1, "Full.");
    }
    else if (pbatt > 30) {
        if (line1) sprintf(line1, "Ok.");
    }
    else if (pbatt > 10) {
        if (line1) sprintf(line1, "Low.");  
        if (line2 && !batt_charge()) sprintf(line2, "Need charging");
    }
    else {
        if (line1) sprintf(line1, "Empty.");
        if (line2 && !batt_charge()) sprintf(line2, "Charge ASAP!");
    } 
    return vbatt;
}




/******************************************************************************
 * Shutdown - go to deep sleep mode and turn off radio, etc.
 * to use as little battery as possible. 
 ******************************************************************************/

void systemShutdown(void)
{   
    sleepMs(500);
    disp_sleepmode(true);
    sleepMs(1000);
    blipDown();
    sleepMs(50);
    radio_on(false); 
#if defined USE_PMU
    pmu_shutdown();
#else
    esp_deep_sleep_start();
#endif
}




/*******************************************************************************
 * TIME MANAGEMENT
 *******************************************************************************/

static bool sntp_initialized = false; 
static mutex_t time_mutex;



/******************************************************************************
 * Get time from NTP
 ******************************************************************************/

static void initialize_sntp(void)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}



/*******************************************************************************
 * Set the real time clock from GPS or from SNTP (if WIFI is connected)
 *******************************************************************************/


static void set_time(time_t t) {
    struct timeval tv; 
    tv.tv_sec = t; 
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
}


void time_init() 
{
    time_mutex = mutex_create();
}


void time_update()
{    
    mutex_lock(time_mutex);
    char tz[64];
    get_str_param("TIMEZONE", tz, 64, DFL_TIMEZONE);
    setenv("TZ", tz, 1);
    tzset();
    
    
    if (gps_is_fixed()) {
        ESP_LOGI(TAG, "Getting time from fixed GNSS.");
        set_time(gps_get_time());
    }
    else if (wifi_isConnected() && !sntp_initialized) {
        ESP_LOGI(TAG, "Getting time over SNTP.");
        initialize_sntp(); 
        sntp_initialized = true;
    }
    else {
        /* if time not set by other means, use GNSS even if not in fix */
        ESP_LOGI(TAG, "Getting time from GNSS.");
        time_t t = gps_get_time();
        if (getTime() <= 1000000000 && t != 0) 
            set_time(t);
    }
    mutex_unlock(time_mutex);
}


/*************************************************************
 * Convert time-struct to calendar time.
 * Make sure we interpret it as UTC (GMT) time by setting the 
 * time zone. 
 *************************************************************/

time_t timegm(struct tm *tm)
{
    mutex_lock(time_mutex);
    setenv("TZ", "GMT0", 1);
    tzset();
    
    time_t t = mktime(tm);
        
    char tz[64];
    get_str_param("TIMEZONE", tz, 64, DFL_TIMEZONE);
    setenv("TZ", tz, 1);
    tzset();
    mutex_unlock(time_mutex);
    return t;
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
    if (now < 3000000)
        return false; 
    mutex_lock(time_mutex);
    gmtime_r(&now, timeinfo);
    mutex_unlock(time_mutex);
    return true; 
}

bool getLocaltime(struct tm *timeinfo)
{
    time_t now = getTime();
    if (now < 3000000)
        return false; 
    mutex_lock(time_mutex);
    localtime_r(&now, timeinfo);
    mutex_unlock(time_mutex);
    return true; 
}




/********************************************************************************
 * Time formatting
 ********************************************************************************/

char* datetime2str(char* buf, time_t time, bool local)
{
    mutex_lock(time_mutex);
    struct tm *tm = (local? localtime(&time) : gmtime(&time));
    mutex_unlock(time_mutex);
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
    sprintf(buf+3, " %02u %02u:%02u %s", tm->tm_mday, 
        (uint8_t) tm->tm_hour, (uint8_t) tm->tm_min, (local? "" : " UTC"));
    return buf;
}


char* time2str(char* buf, time_t time, bool local)
{
    mutex_lock(time_mutex);
    struct tm *tm = (local? localtime(&time) : gmtime(&time));
    mutex_unlock(time_mutex);
    sprintf(buf, "%02u:%02u:%02u", 
      (uint8_t) tm->tm_hour, (uint8_t) tm->tm_min, (uint8_t) tm->tm_sec );
    return buf;
}
 
 
char* date2str(char* buf, time_t time, bool local)
{
    mutex_lock(time_mutex);
    struct tm *tm = (local? localtime(&time) : gmtime(&time));
    mutex_unlock(time_mutex);
    sprintf(buf, "%02hu-%02hu-%4hu", (uint8_t) tm->tm_mday, (uint8_t) tm->tm_mon+1, (uint16_t) tm->tm_year+1900);
    return buf;
}




/********************************************************************************
 * Set loglevels from flash config
 ********************************************************************************/

/* 
 * Managed log-tags. If you want to control logging of a component, add
 * its tag(s) to this list.
 */

static char* logtags[] = {
#if defined(ARCTIC4_UHF)   
    "system", "main", "wifi", "wifix", "config", "httpd", "shell", "tracker",
    "esp-tls", "radio", "ui", "gps", "uart", "digi", "igate", "tcp-cli", "trackstore", 
    "tracklog", "mbedtls", "rest", "adc", "httpd_txrx", "httpd_uri", "httpd_parse", "mdns", 
    "gptimer", "lora", "lora-aprs", "spi"
#define NLOGTAGS 29
#else
    "system", "main", "wifi", "wifix", "config", "httpd", "shell", "tracker",
    "esp-tls", "radio", "ui", "afsk-rx", "hdlc-enc", "hdlc-dec", "gps", "uart", "digi", "igate",
    "tcp-cli", "trackstore", "tracklog", "mbedtls", "rest", "adc", "httpd_txrx", 
    "httpd_uri", "httpd_parse", "mdns", "gptimer"
#define NLOGTAGS 29
#endif
};

    

uint8_t lglv_dfl = ESP_LOG_NONE;



static char* paramName(char* buf, char* param, int max) {
    snprintf(buf, max, "LGLV.%s", param);
    buf[max-1] = '\0';
    return buf;
}


/* 
 * Restore loglevel for tag, from NVS
 */
void logLevel_restore(char* param, esp_log_level_t dfl) {
    char buf[16];
    paramName(buf, param, 16); 
    esp_log_level_t lvl = get_byte_param(buf, dfl);
    ESP_LOGD(TAG, "Restore log level for %s = %s", param, loglevel2str(lvl));
    esp_log_level_set(param, lvl);
}


/*
 * Set loglevel for tag and store it in NVS
 */
void logLevel_set(char* param, uint8_t level) {
    char buf[16];   
    paramName(buf, param, 16); 
    set_byte_param(buf, level); 
    esp_log_level_set(param, level);
}


/*
 * Get loglevel setting for tag, from NVS. 
 * If not set, return default.
 */
int logLevel_get(char* param) {
    char buf[16];  
    paramName(buf, param, 16); 
    return get_byte_param(buf, lglv_dfl);
}


/*
 * Remove loglevel setting for tag, in NVS. 
 */
void logLevel_delete(char* param) {
    char buf[16];  
    paramName(buf, param,16); 
    delete_param(buf); 
    esp_log_level_set(param, lglv_dfl);
}



void logLevel_init() {
    lglv_dfl = get_byte_param("LGLV.ALL", ESP_LOG_WARN) ; 
    esp_log_level_set("*", lglv_dfl);
    
    for (int i=0; i<NLOGTAGS; i++)
        logLevel_restore(logtags[i], lglv_dfl);
}


bool logLevel_hasTag(char* tag) {
    if (strcmp(tag, "*") == 0)
        return true;
    for (int i=0; i<NLOGTAGS; i++)
        if (strcmp(tag, logtags[i]) == 0)
            return true;
    return false; 
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
        case ESP_LOG_MAX: return "MAX"; 
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
    else if (strcasecmp(str, "MAX")==0) return ESP_LOG_MAX;
    ESP_LOGW(TAG, "No corresponding log level for string '%s'", str);
    return ESP_LOG_NONE;
}


int __characters = 0;

/****************************************************************************
 * read line from serial input 
 * Typing ctrl-C will immediately return false
 ****************************************************************************/

bool readline(uart_port_t cbp, char* buf, const uint16_t max) 
{
  char x, xx;
  uint16_t i=0; 
  __characters = 0;
  
  for (i=0; i<max-1; i++) {
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
  __characters = i;
  return true;
}



bool freadline(FILE* f, char* buf, const uint16_t max) 
{
  char x;
  uint16_t i=0; 
  
  for (i=0; i<max-1; i++) {
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
