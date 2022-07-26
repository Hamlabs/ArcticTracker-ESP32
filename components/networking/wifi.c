/*
 * WIFI code.
 * By LA7ECA, ohanssen@acm.org
 */

#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "system.h"
#include "defines.h"
#include "config.h"
#include "commands.h"
#include "networking.h"
#include "mdns.h"
#include "tracklogger.h"

#define AP_BEACON_INTERVAL 5000 // in milliseconds
#define AP_SSID_HIDDEN 0

#define DEFAULT_SSID "NO-SSID"
#define DEFAULT_PWD "NO-PASSWORD"


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
static bool softApEnabled = false; 
static bool connected = false;
static char *status = "Off"; 
static char hostname[32]; 
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



/**********************************************************************************
 * start mdns service 
 **********************************************************************************/

void mdns_start(char* ident) {
    //initialize mDNS service
    esp_err_t err = mdns_init();
    if (err) {
        printf("MDNS Init failed: %d\n", err);
        return;
    }

    char buffer[32]; 
    
    /* Set hostname */
    sprintf(hostname, "arctic-%s", ident);
    mdns_hostname_set(hostname);
    
    /* Set default instance */
    sprintf(buffer, "Arctic Tracker: %s", ident);
    mdns_instance_name_set(buffer);
    
    /* Announce services */
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    mdns_service_instance_name_set("_http", "_tcp", "Arctic Tracker HTTP Server");
    
    mdns_txt_item_t txtData[1] = {
        {"ident", ident}
    };
    /* Set txt data for service (will free and replace current data) */
    mdns_service_txt_set("_http", "_tcp", txtData, 1);
}


char* mdns_hostname(char* buf) {
    sprintf(buf, "%s.local", hostname);
    return buf;
    
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
            if (apCount > 0) {
                free(apList);
                apList = (wifi_ap_record_t *) malloc(sizeof(wifi_ap_record_t) * apCount);
                ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, apList));
            }
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
    char ident[16];
    get_str_param("MYCALL", ident, 16, DFL_MYCALL);
    if (strcmp(ident, "") == 0 || strcmp(ident, "NOCALL") == 0)
        sprintf(ident,"%X", chipId());
    
    wifi_event_group = xEventGroupCreate();
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);        
    xEventGroupSetBits(wifi_event_group, DISCONNECTED_BIT);
    
    tcpip_adapter_init();
    sprintf(default_ssid, "Arctic_%s", ident);
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    wifi_enable_softAp(GET_BYTE_PARAM("SOFTAP.on"));
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    scanDone = cond_create();

    if (GET_BYTE_PARAM("WIFI.on"))
        wifi_enable(true);
    
    mdns_start(ident);
    initialized = true;
}



/********************************************************************************
 * Turn on or off softAP mode
 ********************************************************************************/
bool wifi_softAp_isEnabled() {
    return softApEnabled;
}



void wifi_enable_softAp(bool en)
{    
    wifi_config_t apcnf = {
        .ap = {
            .max_connection = AP_MAX_CLIENTS,
            .ssid_hidden = AP_SSID_HIDDEN, 
            .beacon_interval = AP_BEACON_INTERVAL,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    wifi_config_t stacnf = {        
        .sta = {
            .ssid = DEFAULT_SSID,
            .password = DEFAULT_PWD,
        },
    };
    
    
    if (!en) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &stacnf));
        softApEnabled = false;
        wifi_enable(false);
        wifi_enable(true);
        return;
    }

    
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
    softApEnabled = true;
}


int wifi_softAp_clients(void){    
    wifi_sta_list_t  stations;
    esp_err_t err = esp_wifi_ap_get_sta_list(&stations);
    if (err==ESP_ERR_WIFI_MODE) 
         return 0;
    
    tcpip_adapter_sta_list_t infoList;
    ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&stations, &infoList));
    return infoList.num;
}


   
/********************************************************************************
 * Turn on or off WIFI
 ********************************************************************************/
TaskHandle_t task_autocon = NULL;

void wifi_enable(bool en)
{
    /* Enable */
    if (en && !enabled) {
        ESP_ERROR_CHECK( esp_wifi_start() );
        dhcp_enable(true);
        if (GET_BYTE_PARAM("HTTPD.on"))
            httpd_enable(true);
        
        /* Start autoconnect task */
        if ( xTaskCreatePinnedToCore( task_autoConnect, "wifi-autocon", STACK_AUTOCON, 
                NULL, tskIDLE_PRIORITY, &task_autocon, CORE_AUTOCON ) == pdPASS)
            ESP_LOGD(TAG, "WIFI autoconnect task created");
    }
    /* Disable */
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
    connected = false;
    esp_err_t res = esp_wifi_connect();
    if (res == ESP_ERR_WIFI_SSID)
        ESP_LOGW(TAG, "Invalid SSID '%s'", ssid);
    else {
        ESP_ERROR_CHECK(res);
        int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
            1, 1, timeout_ms / portTICK_PERIOD_MS);
        connected = ((bits & CONNECTED_BIT) != 0);
    }
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
    int n=0;
    wifiAp_t alt;
    sleepMs(3000);
    while(true) {
        /* Wait until disconnected */
        ESP_LOGI(TAG, "Waiting for DISCONNECTED_BIT");
        if (connected) 
            xEventGroupWaitBits(wifi_event_group, DISCONNECTED_BIT, 1, 1, portMAX_DELAY );
        sleepMs(6000); 
        ESP_LOGI(TAG, "Starting scan");
        wifi_startScan(); 
        ESP_LOGI(TAG, "Waiting for scan");
        wifi_waitScan();
        sleepMs(100);
        /* Go through list of alternatives and if in scan-list, try to connect */
        ESP_LOGI(TAG, "Going through scan result");
        for (i=0; i<AP_MAX_ALTERNATIVES; i++) {
            if (wifi_getApAlt(i, &alt)) 
                if (wifi_inScanList(alt.ssid)) {
                    if (wifi_join(alt.ssid, alt.passwd, 6000))
                        break;
                    sleepMs(2000);
                }
        }
        if (connected) {
            ESP_LOGI(TAG, "Connected");
            tracklog_post_start();  
            n=0;
        }
        else { 
            if (n>0) { 
                // Turn off wifi to save power?
                ESP_LOGI(TAG, "Waiting - to save power");
                if (!wifi_softAp_isEnabled())
                    wifi_enable(false);
                sleepMs(AUTOCONNECT_PERIOD*1000);
                wifi_enable(true);
            }
            n++;
        }
    }
}




/********************************************************************************
 * Return true if ssid if found in scan list
 ********************************************************************************/

bool wifi_inScanList(char* ssid) 
{
    int i;
    if (strlen(ssid) < 3)
        return false;
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

