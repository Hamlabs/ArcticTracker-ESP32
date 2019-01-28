/*
 * Definitions for wifi and internetworking. 
 * By LA7ECA, ohanssen@acm.org
 */ 

#ifndef _NETWORKING_H_
#define _NETWORKING_H_

#include "esp_wifi.h"
#include "tcpip_adapter.h"

#define AP_MAX_PASSWD_LEN 64
#define AP_MAX_ALTERNATIVES 6


typedef struct {
    char ssid[32];
    char passwd[AP_MAX_PASSWD_LEN];
} wifiAp_t; 

extern char default_ssid[];


/* WIFI functions */
bool   wifi_isEnabled(void);
bool   wifi_isConnected(void);
void   wifi_waitConnected(void);
bool   wifi_join(const char* ssid, const char* pass, int timeout_ms);
void   wifi_init(void);
void   wifi_enable(bool);
void   wifi_startScan(void);
char*  wifi_getStatus(void);
char*  wifi_authMode(int); 
int    wifi_getApCount(void);
void   wifi_waitScan(void);
char*  wifi_getConnectedAp(char*);
bool   wifi_getApAlt(int n, wifiAp_t* res);
void   wifi_setApAlt(int n, wifiAp_t* ap);
void   wifi_deleteApAlt(int n);
bool   wifi_inScanList(char* ssid);
tcpip_adapter_ip_info_t wifi_getIpInfo(void);
wifi_ap_record_t * wifi_getApList(void);

/* Web server */
void   httpd_enable(bool);

/* Utilities */
char*  mac2str(uint8_t *x);
void str2ip(ip4_addr_t *ip, char* str);

#endif
