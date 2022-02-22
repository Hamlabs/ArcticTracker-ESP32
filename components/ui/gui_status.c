
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "config.h"
#include "defines.h"
#include "ax25.h"
#include "gps.h"
#include "ui.h"
#include "gui.h"
#include "networking.h"
#include "system.h"


#define NSCREENS 7

#if DISPLAY_TYPE == 0

#define LINE1 15
#define LINE2 26
#define LINE3 37
#define LINE4 48

#else

#define LINE1 14
#define LINE2 24
#define LINE3 34
#define LINE4 44
#define LINE5 54

#endif

mutex_t gui_mutex;

static int current = 0;
static char buf[30];

static void status_heading(char* label);
static void status_screen1(void);
static void status_screen2(void);
static void status_screen3(void);
static void status_screen4(void);
static void status_screen5(void);
static void status_screen6(void);



void status_init() {
    gui_mutex = xSemaphoreCreateMutex();
}


/****************************************************************
 * Show current status screen 
 ****************************************************************/

void status_show() {
    mutex_lock(gui_mutex);
    switch (current) {
        case 0:  gui_welcome();
                 break;
        case 1:  status_screen1(); 
                 break;
        case 2:  status_screen2(); 
                 break;       
        case 3:  status_screen3(); 
                 break;        
	    case 4:  status_screen4(); 
                 break;    	
        case 5:  status_screen5(); 
                 break;  
        case 6:  status_screen6(); 
                 break;            
    }
    mutex_unlock(gui_mutex);
}


/****************************************************************
 * Cycle to next status screen 
 ****************************************************************/

void status_next() { 
    current = (current + 1) % NSCREENS; 
    status_show();
}



/****************************************************************
 * Display heading. 
 ****************************************************************/

static void status_heading(char* label) {
    disp_label(0,0, label);
#if DISPLAY_TYPE == 0
    disp_flag(32,0, "i", wifi_isEnabled() );
    disp_flag(41,0, "g", GET_BYTE_PARAM("IGATE.on")); 
    disp_flag(50,0, "d", GET_BYTE_PARAM("DIGIPEATER.on"));
#else
    disp_flag(32,0, "i", wifi_isEnabled() );
    disp_flag(44,0, "g", GET_BYTE_PARAM("IGATE.on")); 
    disp_flag(56,0, "d", GET_BYTE_PARAM("DIGIPEATER.on"));
#endif
    
    uint16_t batt = adc_batt(); 
    uint8_t bi; 
    if (batt > 8000)      bi = 4;
    else if (batt > 7800) bi = 3; 
    else if (batt > 7400) bi = 2;  
    else if (batt > 7100) bi = 1;  // Low
    else bi = 0;
             
#if DISPLAY_TYPE == 0
    disp_battery(70,3,bi);
    disp_hLine(0,10,66);
#else
    disp_battery(110,2,bi);
    disp_hLine(0,10,106);
#endif
}



/****************************************************************
 * 1. APRS status
 ****************************************************************/
  
static void status_screen1() {
    char call[10]; 

    disp_clear();
    status_heading("APRS");

    GET_STR_PARAM("MYCALL", call, 10);
    disp_setBoldFont(true);
    disp_writeText(0, LINE1, call);
    disp_setBoldFont(false);
    
    int32_t f = GET_I32_PARAM("TXFREQ");
    sprintf(buf, "%03d.%03d MHz%c", f/10000, (f/10)%1000, '\0');
    disp_writeText(0, LINE2, buf);
    
    /* Get and convert digipeater path */
    addr_t digis[7];
    char buf[70];
    GET_STR_PARAM("DIGIPATH", buf, 70);
    uint8_t ndigis = str2digis(digis, buf);
    digis2str(buf, ndigis, digis, true);
    
    disp_writeText(0, LINE3, buf);  
    disp_flush();
}



/****************************************************************
 * 2. GPS position status
 ****************************************************************/

static void status_screen2() {
    disp_clear();
    status_heading("GPS");
    if (gps_is_fixed()) {
       disp_writeText(0, LINE1, pos2str_lat(buf, gps_get_pos()));
       disp_writeText(0, LINE2, pos2str_long(buf, gps_get_pos()));
       disp_writeText(0, LINE3, datetime2str(buf, gps_get_time()));
    }		     
    else
       disp_writeText(0, LINE1, "Searching...");
    disp_flush();
}



/****************************************************************
 * 3. WIFI status
 ****************************************************************/

static void status_screen3() {
    char buf[24]; 
    
    disp_clear();
    status_heading("WIFI");
    
    disp_writeText(0, LINE1, wifi_getStatus());
    disp_writeText(0, LINE2, wifi_getConnectedAp(buf));
    disp_setBoldFont(true);
    disp_writeText(0, LINE3, wifi_getIpAddr(buf)); 
    disp_setBoldFont(false);
    disp_writeText(0, LINE4, mdns_hostname(buf)); 
    disp_flush();
}


/****************************************************************
 * 4. WIFI softap status
 ****************************************************************/

static void status_screen4() {
    disp_clear();
    status_heading("W-AP");
    disp_writeText(0, LINE1, (wifi_isEnabled() ? "Enabled" : "Disabled"));
    disp_writeText(0, LINE2, wifi_getApSsid(buf));
    disp_writeText(0, LINE3, wifi_getApIp(buf));
    disp_flush();
}



/****************************************************************
 * 5. Battery status
 ****************************************************************/

static void status_screen5() {
    char b1[16], b2[16];
    b2[0] = '\0';
    disp_clear();
    status_heading("BATT");
 
    uint16_t batt = adc_batt_status(b1, b2);
    sprintf(buf, "%1.02f V%c", ((float)batt)/1000, '\0');
    disp_writeText(0, LINE1, buf);
    disp_writeText(0, LINE2, b1);
    disp_setBoldFont(true);
    disp_writeText(0, LINE3, b2);
    disp_setBoldFont(false);
    disp_flush();
}


/****************************************************************
 * 6. Firmware status
 ****************************************************************/

static void status_screen6() {
    disp_clear();
    status_heading("SYST");
 
    disp_writeText(0, LINE1, FW_NAME);
    disp_writeText(0, LINE2, VERSION_STRING);
    disp_writeText(0, LINE3, FW_DATE);   
    disp_flush();
}
