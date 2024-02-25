/*
 * Settings and shell commands related to wifi and internet
 * By LA7ECA, ohanssen@acm.org
 */

#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "system.h" 
#include "defines.h"
#include "config.h"
#include "commands.h"
#include "esp_wifi.h"
#include "networking.h"
#include "system.h"
#include "linenoise/linenoise.h"
#include "restapi.h"


static void   showScan(void);

int    do_connect(int argc, char** argv);
int    do_scan(int argc, char** argv); 
void   register_wifi(void);



/********************************************************************************
 * Join an access point
 ********************************************************************************/

/** Arguments used by 'connect' function */
static struct {
    struct arg_int *timeout;
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
} join_args;


int do_join(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &join_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, join_args.end, argv[0]);
        return 1;
    }

    bool connected = wifi_join(
        join_args.ssid->sval[0],
        join_args.password->sval[0],
        join_args.timeout->ival[0]);
    
    if (!connected) 
        printf("Connection failed\n");
    else
        printf("Ok\n");
    return 0;
}


/********************************************************************************
 * Connect to an internet server using tcp (like telnet command)
 ********************************************************************************/

static char buf[200];
static bool trunning = true; 


void tcprec(void* arg)
{
    while (trunning) {
        int n = inet_read(buf, 199);
        if (n>0)
            printf("%s", buf);
    }
    vTaskDelete(NULL);
}



int do_connect(int argc, char** argv)
{
    if (argc<=2) {
        printf("Connect command needs arguments\n");
        return 0;
    }
    char* host = argv[1]; 
    int port = atoi(argv[2]);
    printf("Connecting..\n");
    int err = inet_open(host, port);
    if (err != 0) {
        printf("Connection failed. errno=%d\n", err);
        return 0;
    }
    printf("Connected to %s:%d. Ctrl-D to disconnect\n", host, port);
    /* Start receiver thread */ 
    trunning = true;
    xTaskCreatePinnedToCore(&tcprec, "Data receiver", 
        STACK_TCP_REC, NULL, NORMALPRIO, NULL, CORE_TCP_REC);

    /* Loop reading text from console. Ctrl-D to disconnect */
    char* line;  
    while ((line = linenoise("")) != NULL) { 
        inet_write(line, 64);
        inet_write("\r\n", 3);
        free(line);
    }
    trunning=false;
    inet_close();
    
    return 0;
}

/********************************************************************************
 * Scan command handler
 ********************************************************************************/

int do_scan(int argc, char** argv) 
{
    if (!wifi_startScan()) {
        printf("Couldn't start scan of Wifi\n");
        return 0;
    }
    wifi_waitScan(); 
    showScan();
    return 0;
}

/********************************************************************************
 * Scan command handler
 ********************************************************************************/
void test_mdns();
int do_mdns(int argc, char** argv) 
{
    char buf[20];
    for (int i=0; i<argc; i++) {
        sprintf(buf, "_%s", argv[i]); 
        mdns_result_t * res = mdns_find_service(buf, "_tcp");
        mdns_print_results(res);
        mdns_free_results(res);
    }
    return 0;
}



/********************************************************************************
 * Show result of scan
 ********************************************************************************/

static void showScan(void)
{
    printf("Number of access points found: %d\n", wifi_getApCount());
    if (wifi_getApCount() == 0) {
        return;
    }
    int i;
           
    printf("\nBSSID              AUTH               RSSI  SSID\n");
    
    for (i=0; i<wifi_getApCount(); i++) {
        wifi_ap_record_t ap = wifi_getApList()[i]; 
        char *authmode = wifi_authMode(ap.authmode);
        printf("%s  %-16s  %4d   %s\n",
            mac2str(ap.bssid), authmode, ap.rssi, ap.ssid);
    }
    /* FIXME: Should we free list memory? */
}



/********************************************************************************
 * List connected APs
 ********************************************************************************/

int do_apSta(int argc, char** argv)
{
    if (!wifi_isEnabled())
        printf("Wifi is not enabled\n");

    wifi_sta_list_t  stations;
    esp_err_t err = esp_wifi_ap_get_sta_list(&stations);
    if (err==ESP_ERR_WIFI_MODE) {
         printf("Wifi adapter is not in AP mode\n");
         return 0;
    }
    else
        ESP_ERROR_CHECK(err);
    
    // FIXME: find IP of each station
    for(int i = 0; i < stations.num; i++) {
        wifi_sta_info_t st = stations.sta[i];
        printf("MAC: %s\n",
            mac2str(st.mac));
      }
    return 0;
}



/********************************************************************************
 * Info command handler
 ********************************************************************************/

int do_info(int argc, char** argv)
{
    char buf[32];
    if (wifi_isEnabled()) {
        // FIXME: Any heap resources that should be freed? ? ? ? 
        // FIXME: Find IP address
        
        printf("    Stn status: %s\n",  wifi_getStatus());
        if (wifi_isConnected()) {
       //     tcpip_adapter_ip_info_t ipinfo = wifi_getIpInfo();
            char buf[40];
            printf("  Connected to: %s\n", (char*) wifi_getConnectedAp(buf));
       //     printf("    IP address: %s\n", ip4addr_ntoa(&ipinfo.ip) );
            printf(" mDNS hostname: %s\n", mdns_hostname(buf));
        }
        uint8_t mac[6];
        ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
        printf("   MAC address: %s\n", mac2str(mac) );
      
        printf("\n");
        printf("        SoftAP: %s\n", (wifi_softAp_isEnabled() ? "On" : "Off"));
        get_str_param("WIFIAP.SSID", buf, 32, default_ssid);
        printf("       AP SSID: %s\n",  buf);     
        get_str_param("WIFIAP.IP", buf, 16, AP_DEFAULT_IP);
        printf(" AP IP address: %s\n", buf); 
    }
    else
        printf(" WIFI is off\n");
    return 0;
}



/********************************************************************************
 * Show or change list of ap alternatives
 ********************************************************************************/

int do_apAlt(int argc, char** argv)
{
    int i;            
    wifiAp_t alt; 
    if (argc == 1) {
        for (i=0; i<AP_MAX_ALTERNATIVES; i++) {
            if (wifi_getApAlt(i, &alt))
                printf("%3d: %s %s\n", i, (strlen(alt.passwd)>0 ? "P" : " "), alt.ssid); 
            else
                printf("%3d: -\n", i);
        }
    }
    else {
        i = atoi(argv[1]); 
        if (i<0 || i>6) 
            printf("Index out of range: %s\n", argv[1]);
        else if (argc == 2) {
            wifi_getApAlt(i, &alt);
            printf("ssid='%s' passwd='%s'\n", alt.ssid, alt.passwd);
        }
        else if (argc == 3 && strcasecmp(argv[2], "delete")==0) {
            wifi_deleteApAlt(i);
            printf("Ok\n");
        }
        else {
            strcpy(alt.ssid, argv[2]); 
            alt.passwd[0] = '\0';
            if (argc >= 4)
                strcpy(alt.passwd, argv[3]);
            wifi_setApAlt(i, &alt);
        }
    }
    return 0;
}


inline static void _param_wifi_handler(bool x)
   { wifi_enable(x); }
   
inline static void _param_softap_handler(bool x)
   { wifi_enable_softAp(x); }
   
inline static void _param_httpd_handler(bool x) {
    if (wifi_isEnabled())
        httpd_enable(x);
}
   
CMD_BOOL_SETTING(_param_wifi,      "WIFI.on",     &_param_wifi_handler);
CMD_BOOL_SETTING(_param_softap,    "SOFTAP.on",   &_param_softap_handler);
CMD_STR_SETTING (_param_apikey,    "API.KEY",     128, "", NULL);
CMD_STR_SETTING (_param_apiorig,   "API.ORIGINS", 64, "", NULL);
CMD_STR_SETTING (_param_ap_ssid,   "WIFIAP.SSID", 32, default_ssid, NULL); 
CMD_STR_SETTING (_param_ap_auth,   "WIFIAP.AUTH", 64, AP_DEFAULT_PASSWD, NULL);
CMD_STR_SETTING (_param_ap_ip,     "WIFIAP.IP",   17, AP_DEFAULT_IP, REGEX_IPADDR);
CMD_STR_SETTING (_param_fwurl,     "FW.URL",      64, "", NULL);
CMD_STR_SETTING (_param_fwcert,    "FW.CERT",     BBUF_SIZE, "", NULL);


/********************************************************************************
 * Register command handlers
 ********************************************************************************/

void register_wifi()
{
    join_args.timeout = arg_int0(NULL, "timeout", "<t>", "Connection timeout, ms");
    join_args.timeout->ival[0] = 8000; // set default value
    join_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    join_args.password = arg_str0(NULL, NULL, "<pass>", "PSK of AP");
    join_args.end = arg_end(2);
    
    const esp_console_cmd_t join_cmd = {
        .command = "wifi-join",
        .help = "Join WiFi AP as a station",
        .hint = NULL,
        .func = &do_join,
        .argtable = &join_args
    };

    ADD_CMD_X(&join_cmd);
    
    ADD_CMD("mdns",       &do_mdns,          "Scan for MDNS services", NULL);  
    ADD_CMD("wifi-scan",  &do_scan,          "Scan for wifi access points", NULL);  
    ADD_CMD("wifi-info",  &do_info,          "Info about WIFI connection", NULL);
    ADD_CMD("wifi",       &_param_wifi,      "WIFI On/Off setting", "[on|off]");
    ADD_CMD("softap",     &_param_softap,    "Soft AP On/Off setting", "[on|off]");
    ADD_CMD("ap",         &do_apAlt,         "List or change AP alternatives", "[<index> [delete | <ssid> [<password>]]]");
    ADD_CMD("ap-ssid",    &_param_ap_ssid,   "WIFI SoftAP SSID setting", "[<ssid>]");
    ADD_CMD("ap-auth",    &_param_ap_auth,   "WIFI SoftAP password", "[<password>]");
    ADD_CMD("ap-ip",      &_param_ap_ip,     "WIFI_SoftAP IP address", "[<ip>]");
    ADD_CMD("ap-sta",     &do_apSta,         "WIFI SoftAP Connected stations", NULL);
    ADD_CMD("api-key",    &_param_apikey,    "REST API Key", "[<key>]");    
    ADD_CMD("api-origins",&_param_apiorig,   "Allowed origins for REST API webclients", "[<regex>]");
    ADD_CMD("fw-url",     &_param_fwurl,     "URL for firmware update", "<url>");
    ADD_CMD("fw-cert",    &_param_fwcert,    "Certificate for firmware update", "");
    ADD_CMD("connect",    &do_connect,       "Connect to internet server", "<host> <port>");
}


