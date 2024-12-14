#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "system.h"
#include "defines.h"
#include "config.h"
#include "digipeater.h"
#include "igate.h"
#include "ui.h"
#include "gui.h"
#include "fbuf.h"
#include "tracker.h"
#include "networking.h"
#include "tracklogger.h"

typedef void (*menucmd_t)(void*);

typedef struct {
  const char     *mc_name;  
  menucmd_t       mc_func;
  void           *mc_arg;
  char           *mc_param;
  bool            mc_dfl;
} MenuCommand;


/* Handler functions (defined below) */
static void mhandle_send(void*);
static void mhandle_igate(void*); 
static void mhandle_digi(void*); 
static void mhandle_wifi(void*); 
static void mhandle_softAp(void* x);
static void mhandle_fwupgrade(void*); 
static void mhandle_shutdown(void*);
static void mhandle_restart(void*);
static void mhandle_tracklog(void* x);

static const MenuCommand items[] = 
{
    { "Send pos report",  mhandle_send,      NULL, NULL, false },
    { "Restart..",        mhandle_restart,   NULL, NULL, false },
    { "Soft Ap",          mhandle_softAp,    NULL, "SOFTAP.on",     DFL_SOFTAP_ON },
    { "WIFI",             mhandle_wifi,      NULL, "WIFI.on",       DFL_WIFI_ON },
    { "Igate",            mhandle_igate,     NULL, "IGATE.on",      DFL_IGATE_ON },
    { "Digipeater",       mhandle_digi,      NULL, "DIGIPEATER.on", DFL_DIGIPEATER_ON },
    { "Track log",        mhandle_tracklog,  NULL, "TRKLOG.on",     DFL_TRKLOG_ON },
    { "Firmware upgrade", mhandle_fwupgrade, NULL, NULL, false }
};
static int nitems = 8; 



/*********************************************************
 * Show a menu with one item highlighted
 * items is an array of strings of max length 4. 
 *********************************************************/

#define MIN(x,y) (x<y ? x : y)
#define MAX_VISIBLE 5

static uint8_t offset = 0;
static uint8_t selection = 0;
static bool menu_active;

 


static void menu_show(int st, int sel) 
{     
    disp_clear();
    disp_frame(); 
    disp_box(2, sel*11+2, 97+10, 12, true);
    
    int i;
    char item[32];
    bool on = false; 
    
    for (i=0; i < MIN(nitems, MAX_VISIBLE); i++) {
        if (items[st+i].mc_param != NULL) 
           on = (bool) GET_BOOL_PARAM(items[st+i].mc_param, items[st+i].mc_dfl);
        
        sprintf(item, "%s  %s", items[st+i].mc_name, 
            (items[st+i].mc_param==NULL ? "" : (!on ? "On" : "Off")));
        
         int ypos = 4+i*11;
         disp_writeText(5, ypos, item); 
    }
    disp_flush();
}



bool menu_is_active()
   { return menu_active; }


   
void menu_activate()
{
    selection = 0;
    offset = 0;
    menu_active = true;
    menu_show(offset, selection); 
}


void menu_increment()
{
    if (offset+selection >= nitems-1) {
        menu_end();
        return; 
    }
    if (selection >= MAX_VISIBLE-1)
       offset++;
    else
       selection++;
    menu_show(offset, selection);
}



void menu_decrement()
{
    if (offset+selection == 0) {
        return; 
    }
    if (selection <= 0)
       offset--;
    else
       selection--;
    menu_show(offset, selection);
}



void menu_select()
{
    menu_end();
    if (items[offset+selection].mc_func != NULL)
        items[offset+selection].mc_func(items[offset+selection].mc_arg);
}


void menu_end()
{
    menu_active = false; 
    disp_clear();
    status_show();
}



/****************************************************
 * Thread that periodically update display and turn
 * off/on igate or digipeater.
 ****************************************************/
 // FIXME Move to ui.c and rename to service Thread
 
bool charging = false;
int gui_pause = 1000;

static void gui_thread (void* arg) 
{
    while (true) {
        sleepMs(gui_pause);
             
        if (batt_charge() && !charging)
            { beeps("-.-.  "); blipUp(); }
        if (!batt_charge() && charging)
            { beeps("-.-.  "); blipDown(); }
        charging = batt_charge();
    
        if (!menu_is_active() && !disp_popupActive())
            status_show();
    }
}


void gui_setPause(int n) {
    gui_pause = n;
}


 
void menu_init()
{
    xTaskCreatePinnedToCore(&gui_thread, "GUI/Menu Thd", 
        STACK_GUI, NULL, NORMALPRIO, NULL, CORE_GUI);
}

 
/* FIXME: use a function instead */
extern bool _popup; 
 


/*************************************************
 * Firmware upgrade
 *************************************************/

void gui_fwupgrade()
{
    _popup = true; 
    disp_clear(); 
    disp_frame(); 
    disp_setBoldFont(true);
    disp_writeText(10,10, "Firmware");
    disp_writeText(10,19, "Upgrade...");
    disp_setBoldFont(false);
    disp_flush();
}
    
    

/*************************************************
 * Welcome message
 *************************************************/

void gui_welcome() 
{
  disp_clear();
  
#if DISPLAY_HEIGHT >= 64
  disp_circle(60,30,10);
  disp_line(60,4,60,56);
  disp_line(35,30,84,30);
  disp_setBoldFont(true); 
  disp_writeText(6,12,"Arctic");
  disp_writeText(70,42, "Tracker");
  disp_setBoldFont(false);
#else
  disp_circle(60,16,10);
  disp_line(60,0,60,32);
  disp_line(35,16,84,16);
  disp_setBoldFont(true); 
  disp_writeText(3,5, "Arctic");
  disp_writeText(74,21, "Tracker");
  disp_setBoldFont(false);
#endif
  
  disp_flush();
}




/**********************************************************
 * Menu command handlers
 **********************************************************/

static void mhandle_send(void* x) {
    tracker_posReport(); 
}

static void mhandle_igate(void* x) {
    bool isOn = GET_BOOL_PARAM("IGATE.on", DFL_IGATE_ON);
    set_byte_param("IGATE.on", !isOn);
    igate_activate( !isOn ); 
}

static void mhandle_digi(void* x) {
    bool isOn = GET_BOOL_PARAM("DIGIPEATER.on", DFL_DIGIPEATER_ON); 
    set_byte_param("DIGIPEATER.on", !isOn); 
    digipeater_activate( !isOn ); 
}

static void mhandle_softAp(void* x) {
    bool isOn = GET_BOOL_PARAM("SOFTAP.on", DFL_SOFTAP_ON);
    set_byte_param("SOFTAP.on", !isOn);
    wifi_enable_softAp( !isOn ); 
}

static void mhandle_tracklog(void* x) {
    bool isOn = GET_BOOL_PARAM("TRKLOG.on", DFL_TRKLOG_ON) && GET_BOOL_PARAM("TRKLOG.POST.on", DFL_TRKLOG_POST_ON) ;
    if (isOn) {
        tracklog_off();
        set_byte_param("TRKLOG.POST.on", 0);
    }
    else {
        tracklog_on();
        set_byte_param("TRKLOG.POST.on", 1);
        tracklog_post_start(); 
    }    
}

static void mhandle_wifi(void* x) {
    bool isOn = GET_BOOL_PARAM("WIFI.on", DFL_WIFI_ON);
    set_byte_param("WIFI.on", !isOn);
    wifi_enable( !isOn ); 
}

static void mhandle_fwupgrade(void* x) {
    firmware_upgrade(); 
}

static void mhandle_restart(void* x) {
    esp_restart();
}
    
static void mhandle_shutdown(void* x) {
    disp_clear(); 
    disp_flush();
    systemShutdown(); 
}
