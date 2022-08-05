
#include "defines.h"
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "espfs_image.h"
#include "cJSON.h"
#include "system.h"
#include "networking.h"
#include "config.h"
#include "sdkconfig.h"
#include "esp_spi_flash.h"
#include "restapi.h"
#include "digipeater.h"
#include "igate.h"



/******************************************************************
 *  GET handler for system status info 
 ******************************************************************/

static esp_err_t system_info_handler(httpd_req_t *req)
{
    char buf[32];
    rest_cors_enable(req); 
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "flash", spi_flash_get_chip_size());
    cJSON_AddStringToObject(root, "ap", wifi_getConnectedAp(buf));
    cJSON_AddStringToObject(root, "ipaddr", wifi_getIpAddr(buf));
    cJSON_AddStringToObject(root, "mdns", mdns_hostname(buf));
    
    cJSON_AddBoolToObject(root, "softap", wifi_softAp_isEnabled());
    
    sprintf(buf, "%1.02f", ((double) adc_batt()) / 1000);
    cJSON_AddStringToObject(root, "vbatt",  buf);       
    
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    sprintf(buf, "%s", mac2str(mac));
    cJSON_AddStringToObject(root, "macaddr", buf);
    
    char st1[16], st2[16];
    adc_batt_status(st1, st2);
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
    cJSON *root; 
    CHECK_JSON_INPUT(req, root);
    rest_cors_enable(req); 
    
    char* mycall = cJSON_GetObjectItem(root, "mycall")->valuestring;
    char* symbol = cJSON_GetObjectItem(root, "symbol")->valuestring;
    char* path = cJSON_GetObjectItem(root, "path")->valuestring;
    char* comment = cJSON_GetObjectItem(root, "comment")->valuestring;
    
    uint8_t maxpause = (uint8_t) cJSON_GetObjectItem(root, "maxpause")->valueint;
    uint8_t minpause = (uint8_t) cJSON_GetObjectItem(root, "minpause")->valueint;
    uint8_t mindist = (uint8_t) cJSON_GetObjectItem(root, "mindist")->valueint;
    uint8_t repeat = (uint8_t) cJSON_GetObjectItem(root, "repeat")->valueint;
    uint16_t turnlimit = (uint16_t) cJSON_GetObjectItem(root, "turnlimit")->valueint;
    int32_t txfreq = (int32_t) cJSON_GetObjectItem(root, "txfreq")->valueint;
    int32_t rxfreq = (int32_t) cJSON_GetObjectItem(root, "rxfreq")->valueint;
    
    bool timestamp = cJSON_GetObjectItem(root, "timestamp")->valueint;
    bool compress = cJSON_GetObjectItem(root, "compress")->valueint;
    bool altitude = cJSON_GetObjectItem(root, "altitude")->valueint;
    bool extraturn = cJSON_GetObjectItem(root, "extraturn")->valueint;
    
    set_str_param("MYCALL", mycall);
    set_str_param("SYMBOL", symbol);
    set_str_param("PATH",   path);
    set_str_param("COMMENT", comment);
    
    set_byte_param("MAXPAUSE", maxpause);
    set_byte_param("MINPAUSE", minpause);
    set_byte_param("MINDIST", mindist);
    set_byte_param("REPEAT", repeat);
    
    set_u16_param("TURNLIMIT", turnlimit);
    set_i32_param("TXFREQ", txfreq);
    set_i32_param("RXFREQ", rxfreq);
    
    set_byte_param("TIMESTAMP.on", timestamp);
    set_byte_param("COMPRESS.on", compress);
    set_byte_param("ALTITUDE.on", altitude);
    set_byte_param("EXTRATURN.on", extraturn);
    
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
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "digiOn", get_byte_param("DIGIPEATER.on", 0));
    cJSON_AddBoolToObject(root, "wide1", get_byte_param("DIGI.WIDE1.on", 0));
    cJSON_AddBoolToObject(root, "sar", get_byte_param("DIGI.SAR.on", 0));
    cJSON_AddBoolToObject(root, "igateOn", get_byte_param("IGATE.on", 0));
    cJSON_AddNumberToObject(root, "port", get_u16_param("IGATE.PORT", DFL_IGATE_PORT));
    
    get_str_param("IGATE.HOST", buf, 64, DFL_IGATE_HOST);
    cJSON_AddStringToObject(root, "server", buf);
    
    get_str_param("IGATE.USER", buf, 32, DFL_IGATE_USER);
    cJSON_AddStringToObject(root, "user", buf);
    
    get_str_param("IGATE.PASS", buf, 6, "");
    cJSON_AddStringToObject(root, "passcode", buf);
    
    return rest_JSON_send(req, root);
}



/******************************************************************
 *  PUT handler for setting related to digipeater/igate 
 ******************************************************************/

static esp_err_t digi_put_handler(httpd_req_t *req) 
{
    cJSON *root; 
    CHECK_JSON_INPUT(req, root);
    rest_cors_enable(req); 
    
    bool digiOn = cJSON_GetObjectItem(root, "digiOn")->valueint;
    bool wide1 = cJSON_GetObjectItem(root, "wide1")->valueint;
    bool sar = cJSON_GetObjectItem(root, "sar")->valueint;
    bool igateOn = cJSON_GetObjectItem(root, "igateOn")->valueint;
    uint16_t port = (uint16_t) cJSON_GetObjectItem(root, "port")->valueint;
    char* server = cJSON_GetObjectItem(root, "server")->valuestring;
    char* user = cJSON_GetObjectItem(root, "user")->valuestring;
    uint16_t passcode = (uint16_t) cJSON_GetObjectItem(root, "passcode")->valueint;
    
    set_byte_param("DIGI.WIDE1.on", wide1);
    set_byte_param("DIGI.SAR.on", sar);
    set_u16_param("IGATE.PORT", port);
    set_u16_param("IGATE.PASS", passcode);
    set_str_param("IGATE.HOST", server);
    set_str_param("IGATE.USER", user);
    
    set_byte_param("DIGIPEATER.on", digiOn);
    digipeater_activate(digiOn);

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
    cJSON *root = cJSON_CreateObject();

    get_str_param("WIFIAP.SSID", buf, 32, default_ssid);
    cJSON_AddStringToObject(root, "apssid", buf);
    
    get_str_param("WIFIAP.AUTH", buf, 64, AP_DEFAULT_PASSWD);
    cJSON_AddStringToObject(root, "appass", buf);
    
    get_str_param("HTTPD.USR", buf, 32, HTTPD_DEFAULT_USR);
    cJSON_AddStringToObject(root, "htuser", buf);
    
    get_str_param("HTTPD.PWD", buf, 64, HTTPD_DEFAULT_PWD);
    cJSON_AddStringToObject(root, "htpass", buf);
    
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
    CHECK_JSON_INPUT(req, root);
    rest_cors_enable(req);
 
    char* apssid = cJSON_GetObjectItem(root, "apssid")->valuestring;
    char* appass = cJSON_GetObjectItem(root, "appass")->valuestring;
    char* htuser = cJSON_GetObjectItem(root, "htuser")->valuestring;
    char* htpass = cJSON_GetObjectItem(root, "htpass")->valuestring;
    set_str_param("WIFIAP.SSID", apssid);
    set_str_param("WIFIAP.AUTH", appass);
    set_str_param("HTTPD.USR", htuser);
    set_str_param("HTTPD.PWD", htpass);
    
    for (int i=0; i<6; i++) {
        char ssid[32];
        char pw[64];
        wifiAp_t ap; 
        sprintf(ssid, "ap_%d_ssid", i);
        sprintf(pw, "ap_%d_pw", i);
        strcpy (ap.ssid, cJSON_GetObjectItem(root, ssid)->valuestring);
        strcpy (ap.passwd, cJSON_GetObjectItem(root, pw)->valuestring);
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
    CHECK_JSON_INPUT(req, root);
    rest_cors_enable(req); 
     
    bool trklog = cJSON_GetObjectItem(root, "trklog_on")->valueint;
    bool trkpost = cJSON_GetObjectItem(root, "trkpost_on")->valueint;
    uint8_t interv = (uint8_t) cJSON_GetObjectItem(root, "interv")->valueint;
    uint8_t ttl = (uint8_t) cJSON_GetObjectItem(root, "ttl")->valueint;
    char* url = cJSON_GetObjectItem(root, "url")->valuestring;
    char* key = cJSON_GetObjectItem(root, "key")->valuestring;
    
    set_byte_param("TRKLOG.on", trklog);
    set_byte_param("TRKLOG.POST.on", trkpost);
    set_byte_param("TRKLOG.INT", interv);
    set_byte_param("TRKLOG.TTL", ttl);
    set_str_param ("TRKLOG.URL", url);
    set_str_param ("TRKLOG.KEY", key);
    
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "PUT WIFI settings successful");
    return ESP_OK;
} 
    
    
    
/******************************************************************
 *  Register handlers for uri/methods
 ******************************************************************/

void register_api_test() 
{    
    REGISTER_GET("/api/info",     system_info_handler); 

    REGISTER_GET("/api/digi",     digi_get_handler);
    REGISTER_PUT("/api/digi",     digi_put_handler);
    REGISTER_OPTIONS("/api/digi", rest_options_handler);

    REGISTER_GET("/api/aprs",     aprs_get_handler);
    REGISTER_PUT("/api/aprs",     aprs_put_handler);
    REGISTER_OPTIONS("/api/aprs", rest_options_handler);    
    
    REGISTER_GET("/api/wifi",     wifi_get_handler);
    REGISTER_PUT("/api/wifi",     wifi_put_handler);
    REGISTER_OPTIONS("/api/wifi", rest_options_handler);
    
    REGISTER_GET("/api/trklog",    trklog_get_handler);
    REGISTER_PUT("/api/trklog",    trklog_put_handler);
    REGISTER_OPTIONS("/api/trklog",rest_options_handler);
}
