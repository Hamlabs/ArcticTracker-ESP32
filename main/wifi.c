/*
 * WIFI code.
 * By LA7ECA, ohanssen@acm.org
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "tcpip_adapter.h"
#include "esp_event_loop.h"

#include "freertos/semphr.h" 
#include "defines.h"
#include "config.h"
#include "commands.h"
#include "networking.h"
#include "system.h"

#define AP_BEACON_INTERVAL 1000 // in milliseconds
#define AP_SSID_HIDDEN 0


static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT           = BIT0;
const int DISCONNECTED_BIT        = BIT1;
const int CLIENT_CONNECTED_BIT    = BIT2;     
const int CLIENT_DISCONNECTED_BIT = BIT3;
const int AP_STARTED_BIT          = BIT4;

static uint16_t apCount = 0;
static wifi_ap_record_t *apList = NULL;

static bool initialized = false;
static bool enabled = false;
static bool connected = false;
static char *status = "Off"; 
static cond_t scanDone;
char default_ssid[32];

static esp_err_t event_handler(void *ctx, system_event_t *event);
static void task_autoConnect( void * pvParms ); 

#define TAG "wifix"




/********************************************************************************
 * start dhcp server
 ********************************************************************************/

static bool dhcp_started = false; 
static void dhcp_enable(bool on)
{
    if (on && !dhcp_started) {
        char ip[16]; 
        // stop DHCP server
        tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
        // assign a static IP to the network interface
        tcpip_adapter_ip_info_t info;
        memset(&info, 0, sizeof(info));
        get_str_param("WIFIAP.IP", ip, 16, AP_DEFAULT_IP);
        str2ip(&info.ip, ip);
        str2ip(&info.gw, ip); //ESP acts as router, so gw addr will be its own addr
        IP4_ADDR(&info.netmask, 255, 255, 255, 0);
        ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
        // start the DHCP server   
        ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
        ESP_LOGD(TAG, "DHCP server started on %s", ip);
        dhcp_started = true;
    }
    if (!on && dhcp_started) {
        ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
        dhcp_started = false; 
    }
}



/********************************************************************************
 * Event handler
 ********************************************************************************/

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
        
    case SYSTEM_EVENT_STA_START:
        status = "Idle";
        break;
    
    case SYSTEM_EVENT_STA_STOP:
        status = "Off";
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        xEventGroupClearBits(wifi_event_group, DISCONNECTED_BIT);
        status = "Connected";
        connected = true; 
        time_init(); 
        break;
        
    case SYSTEM_EVENT_STA_CONNECTED:
        ESP_LOGD(TAG, "STA Connect event");
        status = "Waiting for IP";
        break;
        
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGD(TAG, "STA Disconnect event");
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        xEventGroupSetBits(wifi_event_group, DISCONNECTED_BIT);
        status = "Disconnected";
        connected = false;
        break;
        
    case SYSTEM_EVENT_SCAN_DONE: 
        ESP_LOGD(TAG, "Scan done event"); 
        if (enabled) {
            ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&apCount));
            free(apList);
            apList = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
            ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, apList));
        }
        cond_set(scanDone);
        break;
        
    default:
        break;
    }
    return ESP_OK;
}




/********************************************************************************
 * Initialize
 ********************************************************************************/

void wifi_init(void)
{
    if (initialized)
        return;
    wifi_event_group = xEventGroupCreate();
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);        
    xEventGroupSetBits(wifi_event_group, DISCONNECTED_BIT);
    
    tcpip_adapter_init();
    sprintf(default_ssid, "Arctic_%X", chipId());
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
//    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t apcnf = {
        .ap = {
            .max_connection = AP_MAX_CLIENTS,
            .ssid_hidden = AP_SSID_HIDDEN, 
            .beacon_interval = AP_BEACON_INTERVAL,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    
    get_str_param("WIFIAP.SSID", (char*) apcnf.ap.ssid, 32, default_ssid);
    apcnf.ap.ssid_len = strlen((char*) apcnf.ap.ssid);
    
    get_str_param("WIFIAP.AUTH", (char*) apcnf.ap.password, 64, AP_DEFAULT_PASSWD);
    if (strlen((char*) apcnf.ap.password) == 0) 
        apcnf.ap.authmode = WIFI_AUTH_OPEN;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    esp_err_t err = esp_wifi_set_config(ESP_IF_WIFI_AP, &apcnf);
    if (err == ESP_ERR_WIFI_PASSWORD)
        ESP_LOGW(TAG, "Invalid password for softAP");
    else if (err == ESP_ERR_WIFI_SSID)
        ESP_LOGW(TAG, "Invalid SSID for softAP");
    else 
        ESP_ERROR_CHECK(err);

    scanDone = cond_create();

    if (GET_BYTE_PARAM("WIFI.on"))
        wifi_enable(true);
    initialized = true;
}

   
/********************************************************************************
 * Turn on or off WIFI
 ********************************************************************************/
TaskHandle_t task_autocon = NULL;

void wifi_enable(bool en)
{
    if (en && !enabled) {
        ESP_ERROR_CHECK( esp_wifi_start() );
        dhcp_enable(true);
        if (GET_BYTE_PARAM("HTTPD.on"))
            httpd_enable(true);
        
        /* Start autoconnect task */
        if ( xTaskCreate( task_autoConnect, "wifi-autocon", STACK_AUTOCON, 
                NULL, tskIDLE_PRIORITY, &task_autocon ) == pdPASS)
            ESP_LOGD(TAG, "WIFI autoconnect task created");
    }
    if (!en && enabled) {
        /* Kill autoconnect task */
        vTaskDelete( task_autocon );
        ESP_LOGD(TAG, "WIFI autoconnect task killed");
        
        /* Disable http and dhcp server and shut down wifi */
        httpd_enable(false);
        dhcp_enable(false);
        ESP_ERROR_CHECK( esp_wifi_stop() );
    }
    enabled = en;
}



/********************************************************************************
 * Set ap alternative
 ********************************************************************************/

void wifi_setApAlt(int n, wifiAp_t* ap) {
    if (n<0 || n>= AP_MAX_ALTERNATIVES) {
        ESP_LOGE(TAG, "Index out of range");
        return;
    }
    char key[12];
    sprintf(key, "AP.ALT.%d", n);
    set_bin_param(key, (void*) ap, sizeof(wifiAp_t)); 
}



/********************************************************************************
 * Get ap alternative
 ********************************************************************************/

bool wifi_getApAlt(int n, wifiAp_t* res) {
    if (n<0 || n>= AP_MAX_ALTERNATIVES) {
        ESP_LOGW(TAG, "Index out of range");
        return false;
    }
    char key[12];
    sprintf(key, "AP.ALT.%d", n);
    return ( GET_BIN_PARAM(key, (void*) res, sizeof(wifiAp_t)) > 0); 
}



/********************************************************************************
 * Delete ap alternative
 ********************************************************************************/

void wifi_deleteApAlt(int n) {
    if (n<0 || n>= AP_MAX_ALTERNATIVES) {
        ESP_LOGE(TAG, "Index out of range");
        return;
    }
    char key[12];
    sprintf(key, "AP.ALT.%d", n);
    delete_param(key);
}



/********************************************************************************
 * Get ip address info
 ********************************************************************************/

tcpip_adapter_ip_info_t wifi_getIpInfo(void) { 
    tcpip_adapter_ip_info_t ip_info;
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
    return ip_info;
}


char* wifi_getIpAddr(char* buf) {
    tcpip_adapter_ip_info_t ipinfo;
    ipinfo = wifi_getIpInfo(); 
    sprintf(buf, "%s", ip4addr_ntoa(&ipinfo.ip));
    return buf;
}



/********************************************************************************
 * Get name (ssid) of the connected ap
 ********************************************************************************/

char* wifi_getConnectedAp(char* buf) {
    wifi_ap_record_t ap;
    if (wifi_isConnected() && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) 
        sprintf(buf, "%s", (char*) ap.ssid);
    else
        sprintf(buf, "-"); 
    return buf;
}
        
        
/********************************************************************************
 * Get name (ssid) or IP addr of softAP
 ********************************************************************************/

char* wifi_getApSsid(char* buf) {
    get_str_param("WIFIAP.SSID", buf, 32, default_ssid);
    return buf; 
}

char* wifi_getApIp(char* buf) {
    get_str_param("WIFIAP.IP", buf, 16, AP_DEFAULT_IP);
    return buf; 
}
 
 
/********************************************************************************
 * Wait for scan to finish
 ********************************************************************************/

void wifi_waitScan(void)
   { cond_wait(scanDone); cond_clear(scanDone); }
 
 
 
/********************************************************************************
 * Return true if wifi is enabled
 ********************************************************************************/
 
bool wifi_isEnabled(void)
   { return enabled; }

   
   
/********************************************************************************
 * Return true if connected to an ap
 * Wait until connected
 ********************************************************************************/
bool wifi_isConnected(void)
   { return connected; }
   
void wifi_waitConnected(void)
{
       xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
}


/********************************************************************************
 * Get a textual description of status
 ********************************************************************************/

char* wifi_getStatus(void)   
   { return status; }

   
   
/********************************************************************************
 * Get number of ap's in range
 ********************************************************************************/

int wifi_getApCount(void)
   { return apCount; }
   

   
/********************************************************************************
 * Get a list of ap's in range
 ********************************************************************************/

wifi_ap_record_t * wifi_getApList(void)
   { return apList; }

   
   

/********************************************************************************
 * Join an access point
 ********************************************************************************/

bool wifi_join(const char* ssid, const char* pass, int timeout_ms)
{
    wifi_config_t wifi_config = { 0 };
    strncpy((char*) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (pass) {
        strncpy((char*) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    } 
    ESP_LOGI(TAG, "Connecting to '%s'", ssid);
    
    status = "Connecting...";
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_connect() );

    int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
            1, 1, timeout_ms / portTICK_PERIOD_MS);
    connected = ((bits & CONNECTED_BIT) != 0);
    if (!connected) 
        status = "Connection failed";
    return connected;
}



/********************************************************************************
 * Start scanning for APs
 ********************************************************************************/

void wifi_startScan(void)
{
    wifi_scan_config_t scanConf = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = 1
    };
    if (wifi_isEnabled())
        ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, 0));
}


/********************************************************************************
 * AUTOCONNECT THREAD
 ********************************************************************************/

static void task_autoConnect( void * pvParms ) 
{
    int i;
    wifiAp_t alt;
    
    while(true) {
        /* Wait until disconnected */
        xEventGroupWaitBits(wifi_event_group, DISCONNECTED_BIT, 1, 1, portMAX_DELAY  );
        sleep(6); 
        wifi_startScan(); 
        wifi_waitScan(); 
        for (i=0; i<AP_MAX_ALTERNATIVES; i++) {
            if (wifi_getApAlt(i, &alt)) 
                if (wifi_inScanList(alt.ssid)) 
                    if (wifi_join(alt.ssid, alt.passwd, 6000))
                        break;
        }
        if (!connected)
            sleep(AUTOCONNECT_PERIOD);
    }
}

// FIXME: when to invalidate scan-results? Interference with others that initiates scans. 
// FIXME: RSSI threshold. 




/********************************************************************************
 * Return true if ssid if found in scan list
 ********************************************************************************/

bool wifi_inScanList(char* ssid) 
{
    int i;
    
    for (i=0; i<wifi_getApCount(); i++) {
        wifi_ap_record_t ap = wifi_getApList()[i]; 
        if (strcmp(ssid, (char*) ap.ssid)==0)
            return true;
    }
    return false;
}

   
   
/********************************************************************************
 * Convert auth mode to string
 ********************************************************************************/   
   
char* wifi_authMode(int mode) 
   {
        switch(mode) {
            case WIFI_AUTH_OPEN:
                return "OPEN";
                break;
            case WIFI_AUTH_WEP:
                return "WEP";
                break;
            case WIFI_AUTH_WPA_PSK:
                return "WPA_PSK";
                break;
            case WIFI_AUTH_WPA2_PSK:
                return "WPA2_PSK";
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                return "WPA_WPA2_PSK";
                break;
            default:
                return "Unknown";
            break;
        }
   }


   
   
/********************************************************************************
 * Convert string to IP address
 ********************************************************************************/

void str2ip(ip4_addr_t *ip, char* str) 
{
    unsigned int d1=0, d2=0, d3=0, d4=0;
    if (sscanf(str, "%u.%u.%u.%u", &d1, &d2, &d3, &d4) < 4)
        ESP_LOGE("str2ip", "Error in IP address format");
    IP4_ADDR(ip, d1, d2, d3, d4);
}



/********************************************************************************
 * Convert mac address to string
 ********************************************************************************/

char* mac2str(uint8_t *x) {
    static char buf[19];
    sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",x[0],x[1],x[2],x[3],x[4],x[5]);
    return buf; 
}

