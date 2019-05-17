/*
 * Misc. System related stuff
 * By LA7ECA, ohanssen@acm.org
 */

#include <time.h>
#include <sys/time.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "defines.h" 
#include "esp_log.h"
#include "esp_wifi.h"
#include "networking.h"
#include "config.h"
#include "system.h"
#include "esp_sntp.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "ui.h"

static void initialize_sntp(void);


#define TAG "system"


/******************************************************************************
 * Upgrade firmware over HTTPS
 ******************************************************************************/

esp_err_t firmware_upgrade()
{
    char* fwurl = malloc(64);
    char* fwcert = malloc(BBUF_SIZE);
    GET_STR_PARAM("FW.URL", fwurl, 64);
    GET_STR_PARAM("FW.CERT", fwcert, BBUF_SIZE);
    
    esp_http_client_config_t config = {
        .url = fwurl,
        .cert_pem = fwcert,
    };
    printf("*** URL=%s\n", config.url);
    BLINK_FWUPGRADE;
    esp_err_t ret = esp_https_ota(&config);
    
    free(fwcert);
    free(fwurl);
    if (ret == ESP_OK) {
        esp_restart();
    } else {
        return ESP_FAIL;
    }
    return ESP_OK;
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
    struct tm timeinfo;
    if (!time_getUTC(&timeinfo)) {
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
 * Get UTC time. Return true if time is set
 *******************************************************************************/

bool time_getUTC(struct tm *timeinfo)
{
    time_t now;
    time(&now);
    localtime_r(&now, timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo->tm_year < (2016 - 1900)) 
        return false; 
    return true; 
}



/********************************************************************************
 * Set loglevels from flash config
 ********************************************************************************/

void set_logLevels() {
    esp_log_level_t default_level = get_byte_param("LOGLV.ALL", ESP_LOG_WARN) ; 
    esp_log_level_set("*", default_level);
    esp_log_level_set("wifi", get_byte_param("LGLV.wifi", default_level));
    esp_log_level_set("wifix", get_byte_param("LGLV.wifix", default_level));
    esp_log_level_set("config", get_byte_param("LGLV.config", default_level));
    esp_log_level_set("httpd", get_byte_param("LGLV.httpd", default_level));
    esp_log_level_set("shell", get_byte_param("LGLV.shell", default_level));
    esp_log_level_set("system", get_byte_param("LGLV.system", default_level));
    esp_log_level_set("tracker", get_byte_param("LGLV.tracker", default_level));
    esp_log_level_set("esp-tls", get_byte_param("LGLV.esp-tls", default_level));
    esp_log_level_set("radio", get_byte_param("LGLV.radio", default_level));
    esp_log_level_set("ui", get_byte_param("LGLV.ui", default_level));
    esp_log_level_set("hdlc-enc", get_byte_param("LGLV.hdlc-enc", default_level));
    esp_log_level_set("hdlc-dec", get_byte_param("LGLV.hdlc-dec", default_level));
    esp_log_level_set("gps", get_byte_param("LGLV.gps", default_level));
    esp_log_level_set("uart", get_byte_param("LGLV.uart", default_level));
}


bool hasTag(char*tag) {
    return strcmp(tag, "wifi")==0 || strcmp(tag, "wifix")==0 ||
           strcmp(tag, "config")==0 || strcmp(tag, "httpd")==0 ||
           strcmp(tag, "shell")==0 || strcmp(tag, "system")==0 ||
           strcmp(tag, "tracker")==0 || strcmp(tag, "esp-tls")==0 ||
           strcmp(tag, "radio")==0 || strcmp(tag, "ui")==0 ||
           strcmp(tag, "hdlc-enc")==0 || strcmp(tag, "gps")==0 ||
           strcmp(tag, "hdlc-dec")==0 || strcmp(tag, "uart")==0;
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

bool readline(uart_port_t cbp, char* buf, const uint16_t max) {
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
     register uint8_t ntokens = 0;
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
