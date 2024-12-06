
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
#include "tracker.h"
#include "trackstore.h"
#include "tracklogger.h"
#include "radio.h"
#include "aprs.h"

#define NSCREENS 10

#if DISPLAY_HEIGHT >= 64

#define LINE1 14
#define LINE2 24
#define LINE3 34
#define LINE4 44
#define LINE5 54

#else
#define LINE1  3
#define LINE2 13
#define LINE3 23
#define LINE4 33
#define LINE5 43

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
static void status_screen7(void);
static void status_screen8(void);
static void status_screen9(void);

void status_init() {
    gui_mutex = xSemaphoreCreateMutex();
}


/****************************************************************
 * Show current status screen 
 ****************************************************************/

void status_show() {
    mutex_lock(gui_mutex);  
    gui_setPause(1000); 
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
        case 7:  status_screen7(); 
                 break;       
        case 8:  status_screen8(); 
                 break;  
        case 9:  status_screen9(); 
                 break;  
    }
    mutex_unlock(gui_mutex);
}


/****************************************************************
 * Cycle to next or previous status screen 
 ****************************************************************/

void status_next() { 
    current = (current + 1) % NSCREENS; 
    status_show();
}

void status_prev() { 
    current = (current - 1); 
    if (current < 0) 
        current = NSCREENS-1;
    status_show();
}


/****************************************************************
 * Display heading. 
 ****************************************************************/

static void status_heading(char* label) {
#if DISPLAY_HEIGHT >= 64
    disp_label(0,0, label);
    disp_flag(32,0, "i", wifi_isConnected() );
    disp_flag(44,0, "g", wifi_isConnected() && GET_BYTE_PARAM("IGATE.on")); 
    disp_flag(56,0, "d", GET_BYTE_PARAM("DIGIPEATER.on"));
    disp_flag(68,0, "p", batt_charge());
    disp_flag(80,0, "F", gps_is_fixed());
#endif
    
    uint16_t batt = batt_percent();
    uint8_t bi; 
    if (batt > 80 -5)      bi = 4;
    else if (batt > 60 -5) bi = 3; 
    else if (batt > 40 -5) bi = 2;  
    else if (batt > 20 -5) bi = 1;  // Low
    else bi = 0;
        
    disp_battery(110,2,bi);
#if DISPLAY_HEIGHT >= 64
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
    disp_setHighFont(true, false);
    disp_writeText(0, LINE1, call);
    disp_setHighFont(false, false);
        
#if defined(ARCTIC4_UHF)
    int32_t f = get_i32_param("FREQ", DFL_FREQ);
    sprintf(buf, "%03ld.%03ld MHz%c", f/1000000, (f/1000)%1000, '\0');
#else
    int32_t f = get_i32_param("TXFREQ", DFL_TXFREQ);
    sprintf(buf, "%03ld.%03ld MHz%c", f/10000, (f/10)%1000, '\0');
#endif
    disp_writeText(0, LINE3, buf);
    disp_setBoldFont(false);
        
    /* Get and convert digipeater path */
    addr_t digis[7];
    char buf[70];
    GET_STR_PARAM("DIGIPATH", buf, 70);
    uint8_t ndigis = str2digis(digis, buf);
    digis2str(buf, ndigis, digis, true);
    disp_writeText(0, LINE4, buf);  
    
    /* Number of pos reports */
    sprintf(buf, "Pos reports: %ld", tracker_posReports());
    disp_writeText(0, LINE5, buf); 
    disp_flush();
}


/****************************************************************
 * 2. Radio monitor
 ****************************************************************/

#if defined(ARCTIC4_UHF)

static void status_screen2() {
    char buf[32];   
    char buf2[10];
    disp_clear();
    status_heading("RXTX");
    
    if (radio_is_on() && loraprs_tx_is_on()) {
        gui_setPause(250); 
        disp_setBoldFont(true);
        disp_setHighFont(true, false);
        sprintf(buf, "Transmitting..");
        disp_writeText(0, LINE2, buf);
        disp_setHighFont(false, false);
        disp_setBoldFont(false);
    }
    else if (radio_is_on()) {
        gui_setPause(300); 
        
        int rssi=radio_getRssi();
        uint8_t sf = get_byte_param("LORA_SF", DFL_LORA_SF);
        uint8_t cr = get_byte_param("LORA_CR", DFL_LORA_CR);
        uint32_t f = get_i32_param("FREQ", DFL_FREQ);
        
        disp_setBoldFont(true);
        disp_setHighFont(true, false);
        sprintf(buf, "LoRa SF%d/CR%d",sf, cr);
        disp_writeText(0, LINE1, buf);
        disp_setBoldFont(false);
        disp_setHighFont(false, false);
        sprintf(buf, "%03ld.%03ld MHz%c", f/1000000, (f/1000)%1000, '\0');
        disp_writeText(0, LINE3, buf);
        
        loraprs_last_heard(buf2);
        if (strlen(buf2) > 0) {
            char tbuf[9];
            time2str(tbuf, loraprs_last_time(), true);
            tbuf[5] = '\0';
            sprintf(buf, "RX %s: %s", tbuf, buf2);
            disp_writeText(0, LINE4, buf);
            sprintf(buf, "%d dBm, SNR=%d dB", loraprs_last_rssi(), loraprs_last_snr());
            disp_writeText(0, LINE5, buf);
        }

    }
    else {
        gui_setPause(2000); 
        disp_setBoldFont(true);
        sprintf(buf, "Radio is OFF");
        disp_writeText(0, LINE2, buf);
        disp_setBoldFont(false);
    }
    disp_flush();
}
    
    

#else

static void status_screen2() {
    char buf[32];   
    disp_clear();
    status_heading("RXTX");
    
    if (radio_is_on() && radio_tx_is_on()) {
        gui_setPause(250); 
        disp_setBoldFont(true);
        disp_setHighFont(true, false);
        sprintf(buf, "Transmitting..");
        disp_writeText(0, LINE2, buf);
        disp_setHighFont(false, false);
        disp_setBoldFont(false);
    }
    else if (radio_is_on()) {
        gui_setPause(250); 
        int rssi=radio_getRssi();
        int nrssi = (rssi-16) / 8;
        if (nrssi > 14)
            nrssi = 14;
        disp_setBoldFont(true);
        disp_setHighFont(true, false);
        int i = 0;
        memset(buf, 0, 24);
        for (i=0; i<nrssi; i++)
            buf[i] = '}'+1;
        disp_writeText(0, LINE1, buf);
        disp_setBoldFont(false);
        disp_setHighFont(false, false);
    
        sprintf(buf, "RSSI: %3d  %s", rssi, (radio_getSquelch() ? "[SQ]" : "")); 
        disp_writeText(0, LINE3, buf);
        int32_t f = get_i32_param("RXFREQ", DFL_RXFREQ);
        sprintf(buf, "RX: %03ld.%03ld MHz%c", f/10000, (f/10)%1000, '\0');
        disp_writeText(0, LINE4, buf);
    }
    else {
        gui_setPause(2000); 
        disp_setBoldFont(true);
        sprintf(buf, "Radio is OFF");
        disp_writeText(0, LINE2, buf);
        disp_setBoldFont(false);
    }
    disp_flush();
}
#endif


/****************************************************************
 * 3. Time
 ****************************************************************/

static void status_screen3() {
    disp_clear();
    status_heading("TIME");
    char buf[24];
    disp_setBoldFont(true);
    disp_setHighFont(true, true); 
    sprintf(buf, "%s", time2str(buf, getTime(), true));   
    disp_writeText(10, LINE2, buf); 
    disp_setHighFont(false, false); 
    sprintf(buf, "%s", date2str(buf, getTime(), true));   
    disp_writeText(10, LINE4, buf); 
    disp_setBoldFont(false);
    disp_flush();
}


/****************************************************************
 * 4. GPS position status
 ****************************************************************/

static void status_screen4() {
    char buf[24];
    disp_clear();
    status_heading("GNSS");
    if (gps_is_fixed()) {
        disp_setBoldFont(true);
        disp_writeText(0, LINE1, pos2str_lat(buf, gps_get_pos()));
        disp_writeText(0, LINE2, pos2str_long(buf, gps_get_pos()));
        disp_setBoldFont(false);
        disp_writeText(0, LINE3, datetime2str(buf, gps_get_time(), false));
       
        if (gps_get_pdop() > -1) {
            sprintf(buf, "pdop: %1.02f", (float) gps_get_pdop());
            disp_writeText(0, LINE4, buf);
        }
    }		     
    else {
       disp_setBoldFont(true);
       disp_setHighFont(true, false);
       disp_writeText(0, LINE1, "Searching...");
       disp_setBoldFont(false);
       disp_setHighFont(false, false);
    }
    disp_flush();
}



/****************************************************************
 * 5. WIFI status
 ****************************************************************/

static void status_screen5() {
    char buf[24]; 
    
    disp_clear();
    status_heading("WIFI");
    disp_writeText(0, LINE1, wifi_getStatus());
    
    disp_writeText(0, LINE2, wifi_getConnectedAp(buf));
    disp_writeText(0, LINE3, mdns_hostname(buf)); 

    if (wifi_isConnected()) {
        disp_setBoldFont(true);
        disp_setHighFont(true, false); 
        disp_writeText(0, LINE4, wifi_getIpAddr(buf)); 
        disp_setHighFont(false, false); 
        disp_setBoldFont(false);
    }
    else
        disp_writeText(0, LINE4, "-");
    disp_flush();
}


/****************************************************************
 * 6. WIFI softap status
 ****************************************************************/

static void status_screen6() {
    char buf[24]; 
    
    disp_clear();
    status_heading("W-AP");
    disp_writeText(0, LINE1, (wifi_isEnabled() && wifi_softAp_isEnabled() ? "Enabled" : "Disabled"));
    disp_writeText(0, LINE2, wifi_getApSsid(buf));
    disp_writeText(0, LINE3, wifi_getApIp(buf));   
    
    sprintf(buf, "Clients: %d", wifi_softAp_clients());
    disp_writeText(0, LINE4, buf); 
    disp_flush();
}



/****************************************************************
 * 7. Battery status
 ****************************************************************/

int chg_cnt = 0;

static void status_screen7() {
    char b1[16], b2[16];
    b2[0] = '\0';
    disp_clear();
    status_heading("BATT");
 
    uint16_t batt = batt_status(b1, b2);
    uint8_t bp = batt_percent();
    if (bp==255)
        bp = 0;
    
    sprintf(buf, "%1.02f V  %d %c%c", ((float)batt)/1000, bp, '%', '\0');

    disp_writeText(0, LINE1, buf);
    disp_writeText(0, LINE2, b1);
    
    disp_setBoldFont(true);
    disp_setHighFont(true, false); 
    
    if (batt_voltage() == 0 ) {
        disp_writeText(0, LINE4, "No battery!");
    }
    else if (batt_charge() ) {
        disp_writeText(0, LINE4, "Charging...");
        chg_cnt = 50;
    }
    else if (bp >= 99 && chg_cnt > 0) {
        disp_writeText(0, LINE4, "Charge complete!");
        chg_cnt--; 
    }
    else if (strlen(b2) > 1)
        disp_writeText(0, LINE4, b2);
    
    disp_setHighFont(false, false);
    disp_setBoldFont(false);
    disp_flush();
}


/****************************************************************
 * 8. Firmware status
 ****************************************************************/

static void status_screen8() {
    disp_clear();
    status_heading("SYST");
 
    disp_writeText(0, LINE1, FW_NAME);
    disp_writeText(0, LINE2, VERSION_STRING);
    disp_writeText(0, LINE3, FW_DATE);   
    disp_writeText(0, LINE4, DEVICE_STRING);   
    disp_flush();
}


/****************************************************************
 * 9. Track log upload status
 ****************************************************************/

static void status_screen9() {
    disp_clear();
    status_heading("TRKL");
    char buf[24];
    
    sprintf(buf, "Reports stored: %d", trackstore_nEntries());
    disp_writeText(0, LINE1, (GET_BYTE_PARAM("TRKLOG.on") ? "Enabled" : "Disabled"));
    disp_writeText(0, LINE2, buf); 
    disp_writeText(0, LINE3, tracklog_status());
    disp_flush();
}


