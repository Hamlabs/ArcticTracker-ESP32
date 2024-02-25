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
#include "tracklogger.h"

#define AP_BEACON_INTERVAL 5000 // in milliseconds
#define AP_SSID_HIDDEN 0

#define DEFAULT_SSID "NO-SSID"
#define DEFAULT_PWD "NO-PASSWORD"

#define TAG "wifix"

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
static cond_t scanDone;
char default_ssid[32];


/* esp netif object representing the WIFI station */
esp_netif_t *sta_netif = NULL;
esp_netif_t *ap_netif = NULL;


static esp_err_t wifi_event_handler(void* arg, esp_event_base_t ebase, int32_t eid, void* event_data);

static void task_autoConnect( void * pvParms ); 





/********************************************************************************
 * start dhcp server
 ********************************************************************************/

static bool dhcp_started = false; 
static void dhcp_enable(bool on)
{
/*
    if (on && !dhcp_started) {
        char ip[16]; 
        // stop DHCP server
        esp_netif_dhcps_stop(ap_netif);
        // assign a static IP to the network interface
        esp_netif_ip_info_t info;
        memset(&info, 0, sizeof(info));
        get_str_param("WIFIAP.IP", ip, 16, AP_DEFAULT_IP);
        str2ip(&info.ip, ip);
        str2ip(&info.gw, ip); //ESP acts as router, so gw addr will be its own addr
        str2ip(&info.netmask, "255.255.255.0");
        ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &info));
        // start the DHCP server   
        ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
        ESP_LOGD(TAG, "DHCP server started on %s", ip);
        dhcp_started = true;
    }
    if (!on && dhcp_started) {
        ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
        dhcp_started = false; 
    }
*/
}

    
    
    
    
/********************************************************************************
 * Event handler
 ********************************************************************************/

static esp_err_t wifi_event_handler(void* arg, esp_event_base_t ebase,
                                    int32_t eid, void* event_data)
{
    if (eid == WIFI_EVENT_STA_STOP) {
        status = "Idle";
    }
    else if (eid == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        xEventGroupClearBits(wifi_event_group, DISCONNECTED_BIT);
        status = "Connected";
        connected = true; 
        time_init(); 
    }
    else if (eid == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGD(TAG, "STA Connect event");
        status = "Waiting for IP";
    }
    else if (eid == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGD(TAG, "STA Disconnect event");
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        xEventGroupSetBits(wifi_event_group, DISCONNECTED_BIT);
        status = "Disconnected";
        connected = false;
    }
    else if (eid == WIFI_EVENT_SCAN_DONE) { 
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
        sprintf(ident,"%lX", chipId());
    
    wifi_event_group = xEventGroupCreate();
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);        // DO WE NEED THESE? 
    xEventGroupSetBits(wifi_event_group, DISCONNECTED_BIT);
    
    esp_netif_init();
    sprintf(default_ssid, "Arctic_%s", ident);
    
    
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    
//    wifi_enable_softAp(GET_BYTE_PARAM("SOFTAP.on")); NEED TO BE FIXED
    
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    scanDone = cond_create();
    
    /* Register event handlers */
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));
    
    // Now we need to set mode, the config and start it
    
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
}


int wifi_softAp_clients(void) {
    return 0;
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

esp_netif_ip_info_t wifi_getIpInfo(void) { 
    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(sta_netif, &ip_info));
    return ip_info;
}


char* wifi_getIpAddr(char* buf) {
    esp_netif_ip_info_t ipinfo;
    ipinfo = wifi_getIpInfo(); 
    esp_ip4addr_ntoa(&ipinfo.ip, buf, 16);
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
bool wifi_startScan(void)
{
    wifi_scan_config_t scanConf = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = 1
    };
    if (wifi_isEnabled()) {
        esp_err_t res = esp_wifi_scan_start(&scanConf, 0);
        if (res == ESP_OK)
            return true;
        else
            ESP_LOGE(TAG, "Network scan initialization failed: %x", res);
    }
    return false;
}


/********************************************************************************
 * AUTOCONNECT THREAD
 ********************************************************************************/

static void task_autoConnect( void * pvParms ) 
{
    int i;
    int n=0;
    wifiAp_t alt;
    sleepMs(4000);
    while(true) {
        /* Wait until disconnected */
        ESP_LOGI(TAG, "Waiting for DISCONNECTED_BIT");
        if (connected) 
            xEventGroupWaitBits(wifi_event_group, DISCONNECTED_BIT, 1, 1, portMAX_DELAY );
        sleepMs(6000); 
        ESP_LOGI(TAG, "Starting scan");
        if (!wifi_startScan()) {
            sleepMs(20000);
            continue; 
        }
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
            case WIFI_AUTH_WEP:
                return "WEP";
            case WIFI_AUTH_WPA_PSK:
                return "WPA_PSK";
            case WIFI_AUTH_WPA2_PSK:
                return "WPA2_PSK";
            case WIFI_AUTH_WPA_WPA2_PSK:
                return "WPA_WPA2_PSK";
            case WIFI_AUTH_WPA2_ENTERPRISE:
                return "WPA2_ENTERPRISE";
            case WIFI_AUTH_WPA3_PSK:
                return "WPA3_PSK";
            case WIFI_AUTH_WPA2_WPA3_PSK:
                return "WPA2_WPA3_PSK";
            case WIFI_AUTH_OWE:
                return "OWE";
            default:
                return "Unknown";
            break;
        }
   }


   
   
/********************************************************************************
 * Convert string to IP address
 ********************************************************************************/

void str2ip(esp_ip4_addr_t *ip, char* str) 
{
    unsigned int d1=0, d2=0, d3=0, d4=0;
    if (sscanf(str, "%u.%u.%u.%u", &d1, &d2, &d3, &d4) < 4)
        ESP_LOGE("str2ip", "Error in IP address format");
    esp_netif_set_ip4_addr(ip, d1, d2, d3, d4);
}



/********************************************************************************
 * Convert mac address to string
 ********************************************************************************/

char* mac2str(uint8_t *x) {
    static char buf[19];
    sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",x[0],x[1],x[2],x[3],x[4],x[5]);
    return buf; 
}

