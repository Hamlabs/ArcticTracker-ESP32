
#include "defines.h"
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"
#include "system.h"
#include "networking.h"
#include "config.h"
#include "esp_flash.h"
#include "restapi.h"
#include "digipeater.h"
#include "igate.h"
#include "tracklogger.h"


#define TAG "rest"


/******************************************************************
 *  GET handler for system status info 
 ******************************************************************/

static esp_err_t system_info_handler(httpd_req_t *req)
{
    char buf[32];
    rest_cors_enable(req); 
    httpd_resp_set_type(req, "application/json");
    CHECK_AUTH(req);
     
    uint32_t size_flash;
    esp_flash_get_size(NULL, &size_flash);
   
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "flash", size_flash );
    cJSON_AddNumberToObject(root, "sizefs", fatfs_size() );
    cJSON_AddNumberToObject(root, "freefs", fatfs_free() );
    cJSON_AddStringToObject(root, "ap", wifi_getConnectedAp(buf));
    cJSON_AddStringToObject(root, "ipaddr", wifi_getIpAddr(buf));
    cJSON_AddStringToObject(root, "mdns", mdns_hostname(buf));
    cJSON_AddBoolToObject(root, "softap", wifi_softAp_isEnabled());
    
    int16_t vbatt = batt_voltage();
    int16_t pbatt = batt_percent();
    sprintf(buf, "%1.02f", ((double) vbatt) / 1000);
    cJSON_AddStringToObject(root, "vbatt",  buf);       
    cJSON_AddNumberToObject(root, "vpercent", pbatt);    
    
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    sprintf(buf, "%s", mac2str(mac));
    cJSON_AddStringToObject(root, "macaddr", buf);
    
    char st1[16]; 
    char st2[16];
    batt_status(st1, st2);
    sprintf(buf, "%s %s", st1, st2);
    cJSON_AddStringToObject(root, "battstatus", buf);
    cJSON_AddStringToObject(root, "device", DEVICE_STRING);
    cJSON_AddStringToObject(root, "version", VERSION_STRING);
    return rest_JSON_send(req, root);
}


/******************************************************************
 *  GET handler for setting related to APRS tracking
 ******************************************************************/

static esp_err_t aprs_get_handler(httpd_req_t *req)
{
    char buf[64];
    rest_cors_enable(req); 
    httpd_resp_set_type(req, "application/json");
    CHECK_AUTH(req);
    
    cJSON *root = cJSON_CreateObject();
    get_str_param("MYCALL", buf, 10, DFL_MYCALL);
    cJSON_AddStringToObject(root, "mycall", buf );
    
    get_str_param("SYMBOL", buf, 64, DFL_SYMBOL);
    cJSON_AddStringToObject(root, "symbol", buf);
    
    get_str_param("DIGIPATH", buf, 64, DFL_DIGIPATH);
    cJSON_AddStringToObject(root, "path", buf );
    
    get_str_param("REP.COMMENT", buf, 64, DFL_REP_COMMENT);
    cJSON_AddStringToObject(root, "comment", buf);
    
    cJSON_AddNumberToObject(root, "maxpause",  get_byte_param("MAXPAUSE", DFL_MAXPAUSE) );
    cJSON_AddNumberToObject(root, "minpause",  get_byte_param("MINPAUSE", DFL_MINPAUSE) );
    cJSON_AddNumberToObject(root, "mindist",   get_byte_param("MINDIST", DFL_MINDIST));
    cJSON_AddNumberToObject(root, "repeat",    get_byte_param("REPEAT", DFL_REPEAT));
    cJSON_AddNumberToObject(root, "turnlimit", get_u16_param("TURNLIMIT", DFL_TURNLIMIT));
   
    cJSON_AddBoolToObject(root, "timestamp", GET_BOOL_PARAM("TIMESTAMP.on", DFL_TIMESTAMP_ON));
    cJSON_AddBoolToObject(root, "compress",  GET_BOOL_PARAM("COMPRESS.on", DFL_COMPRESS_ON));
    cJSON_AddBoolToObject(root, "altitude",  GET_BOOL_PARAM("ALTITUDE.on", DFL_ALTITUDE_ON));
    cJSON_AddBoolToObject(root, "extraturn", GET_BOOL_PARAM("EXTRATURN.on", DFL_EXTRATURN_ON));
   
#if defined(ARCTIC4_UHF)
    cJSON_AddNumberToObject(root, "lora_sf",     get_byte_param("LORA_SF", DFL_LORA_SF));
    cJSON_AddNumberToObject(root, "lora_cr",     get_byte_param("LORA_CR", DFL_LORA_CR));
    cJSON_AddNumberToObject(root, "lora_alt_sf", get_byte_param("LORA_ALT_SF", DFL_LORA_ALT_SF));
    cJSON_AddNumberToObject(root, "lora_alt_cr", get_byte_param("LORA_ALT_CR", DFL_LORA_ALT_CR));
    
    cJSON_AddNumberToObject(root, "txpower",   get_byte_param("TXPOWER", DFL_TXPOWER));
    cJSON_AddNumberToObject(root, "freq",      get_i32_param("FREQ", DFL_FREQ));
#else
    cJSON_AddNumberToObject(root, "txfreq",    get_i32_param("TXFREQ", DFL_TXFREQ));
    cJSON_AddNumberToObject(root, "rxfreq",    get_i32_param("RXFREQ", DFL_RXFREQ));
#endif
    
    return rest_JSON_send(req, root);
}


/******************************************************************
 *  PUT handler for setting related to APRS tracking
 ******************************************************************/

static esp_err_t aprs_put_handler(httpd_req_t *req) 
{
    char buf[32];
    ESP_LOGI(TAG, "aprs_put_handler");
    cJSON *root;   
    rest_cors_enable(req); 
    CHECK_JSON_INPUT(req, root);
    
    strcpy(buf, JSON_STR(root, "mycall"));
    strupr(buf);
    set_str_param("MYCALL",  buf);
    
    strcpy(buf, JSON_STR(root, "path"));
    strupr(buf);
    set_str_param("DIGIPATH", buf);
    
    set_str_param("SYMBOL",   JSON_STR(root, "symbol"));
    set_str_param("REP.COMMENT",  JSON_STR(root, "comment"));
    set_byte_param("MAXPAUSE", JSON_BYTE(root, "maxpause"));
    set_byte_param("MINPAUSE", JSON_BYTE(root, "minpause"));
    set_byte_param("MINDIST",  JSON_BYTE(root, "mindist"));
    set_byte_param("REPEAT",   JSON_BYTE(root, "repeat"));
    
    set_byte_param("TIMESTAMP.on", JSON_BOOL(root, "timestamp"));
    set_byte_param("COMPRESS.on",  JSON_BOOL(root, "compress"));
    set_byte_param("ALTITUDE.on",  JSON_BOOL(root, "altitude"));
    set_byte_param("EXTRATURN.on", JSON_BOOL(root, "extraturn"));
    set_u16_param ("TURNLIMIT",    JSON_U16(root,  "turnlimit"));
    
#if defined(ARCTIC4_UHF)
    set_byte_param("LORA_SF",     JSON_BYTE(root, "lora_sf"));
    set_byte_param("LORA_CR",     JSON_BYTE(root, "lora_cr"));  
    set_byte_param("LORA_ALT_SF", JSON_BYTE(root, "lora_alt_sf"));
    set_byte_param("LORA_ALT_CR", JSON_BYTE(root, "lora_alt_cr"));  
    
    set_byte_param("TXPOWER", JSON_BYTE(root, "txpower")); 
    set_i32_param("FREQ",     JSON_INT(root, "freq"));
#else
    set_i32_param("TXFREQ",    JSON_INT(root, "txfreq"));
    set_i32_param("RXFREQ",    JSON_INT(root, "rxfreq"));
#endif
    
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "PUT digi/igate settings successful");
    return ESP_OK;
}



/******************************************************************
 *  GET handler for setting related to digipeater/igate 
 ******************************************************************/

static esp_err_t digi_get_handler(httpd_req_t *req)
{
    char buf[64];
    rest_cors_enable(req); 
    httpd_resp_set_type(req, "application/json");
    CHECK_AUTH(req);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "digiOn", GET_BOOL_PARAM("DIGIPEATER.on", DFL_DIGIPEATER_ON));
    cJSON_AddBoolToObject(root, "wide1", GET_BOOL_PARAM("DIGI.WIDE1.on", DFL_DIGI_WIDE1_ON));
    cJSON_AddBoolToObject(root, "sar", GET_BOOL_PARAM("DIGI.SAR.on", DFL_DIGI_SAR_ON));
    cJSON_AddBoolToObject(root, "igateOn", GET_BOOL_PARAM("IGATE.on", DFL_IGATE_ON));
    cJSON_AddNumberToObject(root, "port", get_u16_param("IGATE.PORT", DFL_IGATE_PORT));
    cJSON_AddNumberToObject(root, "passcode", get_u16_param("IGATE.PASS", 0));
     
    get_str_param("IGATE.HOST", buf, 64, DFL_IGATE_HOST);
    cJSON_AddStringToObject(root, "server", buf);
    
    get_str_param("IGATE.USER", buf, 32, DFL_IGATE_USER);
    cJSON_AddStringToObject(root, "user", buf);
    
    get_str_param("IGATE.FILTER", buf, 32, DFL_IGATE_FILTER);
    cJSON_AddStringToObject(root, "filter", buf);

#if defined(ARCTIC4_UHF)
    cJSON_AddBoolToObject(root, "dualOn", GET_BOOL_PARAM("LORA_ALT.on", DFL_LORA_ALT_ON));
#endif
    return rest_JSON_send(req, root);
}



/******************************************************************
 *  PUT handler for setting related to digipeater/igate 
 ******************************************************************/

static esp_err_t digi_put_handler(httpd_req_t *req) 
{
    cJSON *root;    
    rest_cors_enable(req); 
    CHECK_JSON_INPUT(req, root);

    set_byte_param("DIGI.WIDE1.on", JSON_BOOL(root, "wide1"));
    set_byte_param("DIGI.SAR.on",   JSON_BOOL(root, "sar"));
    set_u16_param("IGATE.PORT",     JSON_U16(root, "port"));
    set_u16_param("IGATE.PASS",     JSON_U16(root, "passcode"));
    set_str_param("IGATE.HOST",     JSON_STR(root, "server"));
    set_str_param("IGATE.USER",     JSON_STR(root, "user"));
    set_str_param("IGATE.FILTER",   JSON_STR(root, "filter"));
    
#if defined(ARCTIC4_UHF)   
    bool dualOn = JSON_BOOL(root, "dualOn");
    set_byte_param("LORA_ALT.on", dualOn);
#endif
    
    bool digiOn = JSON_BOOL(root, "digiOn");
    set_byte_param("DIGIPEATER.on", digiOn);
    digipeater_activate(digiOn);
    
    bool igateOn = JSON_BOOL(root, "igateOn");
    set_byte_param("IGATE.on", igateOn);
    igate_activate(igateOn); 
    
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "PUT digi/igate settings successful");
    return ESP_OK;
}



/******************************************************************
 *  GET handler for setting related to WIFI 
 ******************************************************************/

static esp_err_t wifi_get_handler(httpd_req_t *req) 
{
    char buf[64];
    rest_cors_enable(req); 
    httpd_resp_set_type(req, "application/json");
    CHECK_AUTH(req);
    
    cJSON *root = cJSON_CreateObject();
    get_str_param("WIFIAP.SSID", buf, 32, default_ssid);
    cJSON_AddStringToObject(root, "apssid", buf);
    
    get_str_param("WIFIAP.AUTH", buf, 64, DFL_SOFTAP_PASSWD);
    cJSON_AddStringToObject(root, "appass", buf);

    get_str_param("FW.URL", buf, 64, DFL_TRKLOG_URL);
    cJSON_AddStringToObject(root, "fwurl", buf);
    
    /* Don't send the API key */
    cJSON_AddStringToObject(root, "apikey", "");
    
    
    for (int i=0; i<6; i++) {
        wifiAp_t res;
        if (wifi_getApAlt(i, &res)) {
            char ssid[32];
            char pw[64];
            sprintf(ssid, "ap_%d_ssid", i);
            sprintf(pw, "ap_%d_pw", i);
            cJSON_AddStringToObject(root, ssid, res.ssid);
            cJSON_AddStringToObject(root, pw, res.passwd);
        }
    }
    
    return rest_JSON_send(req, root);
}



/******************************************************************
 *  PUT handler for setting related to WIFI
 ******************************************************************/

static esp_err_t wifi_put_handler(httpd_req_t *req) 
{
    cJSON *root;  
    rest_cors_enable(req);
    CHECK_JSON_INPUT(req, root);
 
    set_str_param("WIFIAP.SSID", JSON_STR(root, "apssid"));
    set_str_param("WIFIAP.AUTH", JSON_STR(root, "appass"));
    set_str_param("FW.URL",      JSON_STR(root, "fwurl"));
    
    /* API key is updated if it is non-empty */
    if (strlen(JSON_STR(root, "apikey"))>3)
        set_str_param("API.KEY", JSON_STR(root, "apikey"));
    
    for (int i=0; i<6; i++) {
        char ssid[32];
        char pw[64];
        wifiAp_t ap; 
        sprintf(ssid, "ap_%d_ssid", i);
        sprintf(pw, "ap_%d_pw", i);
        strcpy (ap.ssid, JSON_STR(root, ssid));
        strcpy (ap.passwd, JSON_STR(root, pw));
        wifi_setApAlt(i, &ap);
    }
    
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "PUT WIFI settings successful");
    return ESP_OK;
}    
    
    
/******************************************************************
 *  GET handler for setting related to track logging
 ******************************************************************/

static esp_err_t trklog_get_handler(httpd_req_t *req)
{
    char buf[128];
    rest_cors_enable(req); 
    httpd_resp_set_type(req, "application/json");
    CHECK_AUTH(req);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "trklog_on", GET_BOOL_PARAM("TRKLOG.on", DFL_TRKLOG_ON));
    cJSON_AddBoolToObject(root, "trkpost_on", GET_BOOL_PARAM("TRKLOG.POST.on", DFL_TRKLOG_POST_ON));
    cJSON_AddNumberToObject(root, "interv", get_byte_param("TRKLOG.INT", DFL_TRKLOG_INT));
    cJSON_AddNumberToObject(root, "ttl", get_byte_param("TRKLOG.TTL", DFL_TRKLOG_TTL));

    get_str_param("TRKLOG.URL", buf, 64, DFL_TRKLOG_URL);
    cJSON_AddStringToObject(root, "url", buf);

    get_str_param("TRKLOG.KEY", buf, 128, "");
    cJSON_AddStringToObject(root, "key", buf);
    
    return rest_JSON_send(req, root);
}


/******************************************************************
 *   PUT handler for setting related to track logging
 ******************************************************************/

static esp_err_t trklog_put_handler(httpd_req_t *req)
{
    cJSON *root;    
    rest_cors_enable(req); 
    CHECK_JSON_INPUT(req, root);

    
    set_byte_param("TRKLOG.on", JSON_BOOL(root, "trklog_on"));
    if (GET_BOOL_PARAM("TRKLOG.on", DFL_TRKLOG_ON))
        tracklog_on();
    
    set_byte_param("TRKLOG.POST.on", JSON_BOOL(root, "trkpost_on"));
    if (GET_BOOL_PARAM("TRKLOG.POST.on", DFL_TRKLOG_ON))
        tracklog_post_start();
        
    set_byte_param("TRKLOG.INT", JSON_BYTE(root, "interv"));
    set_byte_param("TRKLOG.TTL", JSON_BYTE(root, "ttl"));
    set_str_param ("TRKLOG.URL", JSON_STR(root, "url"));
    set_str_param ("TRKLOG.KEY", JSON_STR(root, "key"));
    
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "PUT WIFI settings successful");
    return ESP_OK;
} 



/******************************************************************
 *   GET handler for mDNS info about trackers on LAN
 ******************************************************************/

static esp_err_t trackers_handler(httpd_req_t *req) {   
    rest_cors_enable(req);
    cJSON *root = cJSON_CreateArray();
    mdns_result_t * res = mdns_find_service("_https", "_tcp");
    while(res) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "name", res->instance_name);
        cJSON_AddStringToObject(obj, "host", res->hostname);
        cJSON_AddNumberToObject(obj, "port", res->port); 
        cJSON_AddItemToArray(root, obj);
        res = res->next;
    }
    return rest_JSON_send(req, root);
}




extern httpd_handle_t http_server;
extern esp_err_t register_file_server(httpd_handle_t *server, const char *path);


/******************************************************************
 *  Register handlers for uri/methods
 ******************************************************************/

void register_api_rest() 
{    
    REGISTER_GET("/api/info",      system_info_handler);
    REGISTER_OPTIONS("/api/info",  rest_options_handler);
    
    REGISTER_GET("/api/trackers",     trackers_handler);
    REGISTER_OPTIONS("/api/trackers", rest_options_handler);
    
    REGISTER_GET("/api/digi",      digi_get_handler);
    REGISTER_PUT("/api/digi",      digi_put_handler);
    REGISTER_OPTIONS("/api/digi",  rest_options_handler);

    REGISTER_GET("/api/aprs",      aprs_get_handler);
    REGISTER_PUT("/api/aprs",      aprs_put_handler);
    REGISTER_OPTIONS("/api/aprs",  rest_options_handler);    
    
    REGISTER_GET("/api/wifi",      wifi_get_handler);
    REGISTER_PUT("/api/wifi",      wifi_put_handler);
    REGISTER_OPTIONS("/api/wifi",  rest_options_handler);
    
    REGISTER_GET("/api/trklog",    trklog_get_handler);
    REGISTER_PUT("/api/trklog",    trklog_put_handler);
    REGISTER_OPTIONS("/api/trklog",rest_options_handler);
    
    /* Static file access */
    register_file_server(http_server, "/webapp");
}
