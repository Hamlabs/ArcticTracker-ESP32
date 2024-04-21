
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
    cJSON_AddNumberToObject(root, "minpause", get_byte_param("MINPAUSE", DFL_MINPAUSE) );
    cJSON_AddNumberToObject(root, "mindist",   get_byte_param("MINDIST", DFL_MINDIST));
    cJSON_AddNumberToObject(root, "repeat",    get_byte_param("REPEAT", DFL_REPEAT));
    cJSON_AddNumberToObject(root, "turnlimit", get_u16_param("TURNLIMIT", DFL_TURNLIMIT));
    cJSON_AddNumberToObject(root, "txfreq",    get_i32_param("TXFREQ", DFL_TXFREQ));
    cJSON_AddNumberToObject(root, "rxfreq",    get_i32_param("RXFREQ", DFL_RXFREQ));
   
    cJSON_AddBoolToObject(root, "timestamp", get_byte_param("TIMESTAMP.on", 0));
    cJSON_AddBoolToObject(root, "compress",  get_byte_param("COMPRESS.on", 0));
    cJSON_AddBoolToObject(root, "altitude", get_byte_param("ALTITUDE.on", 0));
    cJSON_AddBoolToObject(root, "extraturn", get_byte_param("EXTRATURN.on", 0));
    
    return rest_JSON_send(req, root);
}


/******************************************************************
 *  PUT handler for setting related to APRS tracking
 ******************************************************************/

static esp_err_t aprs_put_handler(httpd_req_t *req) 
{
    
    ESP_LOGI(TAG, "aprs_put_handler");
    cJSON *root;   
    rest_cors_enable(req); 
    CHECK_JSON_INPUT(req, root);
      
    set_str_param("MYCALL",   JSON_STR(root, "mycall"));
    set_str_param("SYMBOL",   JSON_STR(root, "symbol"));
    set_str_param("DIGIPATH", JSON_STR(root, "path"));
    set_str_param("REP.COMMENT",  JSON_STR(root, "comment"));
    
    set_byte_param("MAXPAUSE", JSON_BYTE(root, "maxpause"));
    set_byte_param("MINPAUSE", JSON_BYTE(root, "minpause"));
    set_byte_param("MINDIST",  JSON_BYTE(root, "mindist"));
    set_byte_param("REPEAT",   JSON_BYTE(root, "repeat"));
    
    set_u16_param("TURNLIMIT", JSON_U16(root, "turnlimit"));
    set_i32_param("TXFREQ",    JSON_INT(root, "txfreq"));
    set_i32_param("RXFREQ",    JSON_INT(root, "rxfreq"));
    
    set_byte_param("TIMESTAMP.on", JSON_BOOL(root, "timestamp"));
    set_byte_param("COMPRESS.on",  JSON_BOOL(root, "compress"));
    set_byte_param("ALTITUDE.on",  JSON_BOOL(root, "altitude"));
    set_byte_param("EXTRATURN.on", JSON_BOOL(root, "extraturn"));
    
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
    cJSON_AddBoolToObject(root, "digiOn", get_byte_param("DIGIPEATER.on", 0));
    cJSON_AddBoolToObject(root, "wide1", get_byte_param("DIGI.WIDE1.on", 0));
    cJSON_AddBoolToObject(root, "sar", get_byte_param("DIGI.SAR.on", 0));
    cJSON_AddBoolToObject(root, "igateOn", get_byte_param("IGATE.on", 0));
    cJSON_AddNumberToObject(root, "port", get_u16_param("IGATE.PORT", DFL_IGATE_PORT));
    cJSON_AddNumberToObject(root, "passcode", get_u16_param("IGATE.PASS", 0));
     
    get_str_param("IGATE.HOST", buf, 64, DFL_IGATE_HOST);
    cJSON_AddStringToObject(root, "server", buf);
    
    get_str_param("IGATE.USER", buf, 32, DFL_IGATE_USER);
    cJSON_AddStringToObject(root, "user", buf);
    
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
    
    get_str_param("WIFIAP.AUTH", buf, 64, AP_DEFAULT_PASSWD);
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
    cJSON_AddBoolToObject(root, "trklog_on", get_byte_param("TRKLOG.on", 0));
    cJSON_AddBoolToObject(root, "trkpost_on", get_byte_param("TRKLOG.POST.on", 0));
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
    if (get_byte_param("TRKLOG.on", 0))
        tracklog_on();
    
    set_byte_param("TRKLOG.POST.on", JSON_BOOL(root, "trkpost_on"));
    if (get_byte_param("TRKLOG.POST.on", 0))
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
