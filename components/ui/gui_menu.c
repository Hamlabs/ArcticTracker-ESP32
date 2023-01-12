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
} MenuCommand;


/* Handler functions (defined below) */
static void mhandle_dispBl(void*);
static void mhandle_send(void*);
static void mhandle_igate(void*); 
static void mhandle_digi(void*); 
static void mhandle_wifi(void*); 
static void mhandle_softAp(void* x);
static void mhandle_fwupgrade(void*); 
static void mhandle_shutdown(void*);
static void mhandle_tracklog(void* x);

static const MenuCommand items[] = 
{
#if DISPLAY_TYPE == 0
    { "Send report",     mhandle_send,      NULL },
    { "SoftAp +|-",      mhandle_softAp,    NULL },
    { "WIFI +|-",        mhandle_wifi,      NULL },
    { "Igate +|-",       mhandle_igate,     NULL },
    { "Digipeater +|-",  mhandle_digi,      NULL },
    { "Firmware upgr.",  mhandle_fwupgrade, NULL },
    { "Shut down..",     mhandle_shutdown,  NULL }
};
static int nitems = 7;
#else
    { "Backlight (+|-)",   mhandle_dispBl,    NULL },
    { "SoftAp (+|-)",      mhandle_softAp,    NULL },
    { "WIFI (+|-)",        mhandle_wifi,      NULL },
    { "Track log (+|-)",   mhandle_tracklog,  NULL },
    { "Igate (+|-)",       mhandle_igate,     NULL },
    { "Digipeater (+|-)",  mhandle_digi,      NULL },
    { "Send pos report",   mhandle_send,      NULL },
    { "Firmware upgrade",  mhandle_fwupgrade, NULL },
    { "Shut down..",       mhandle_shutdown,  NULL },
};
static int nitems = 9; 
#endif



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
   
#if DISPLAY_TYPE == 0
    disp_box(0, sel*11, 83, 12, true);
    int i;
    for (i=0; i < MIN(nitems,MAX_VISIBLE); i++) 
        disp_writeText(4, 2+i*11, items[st+i].mc_name); 
#else
    disp_box(2, sel*11+2, 97, 12, true);
    int i;
    for (i=0; i < MIN(nitems,MAX_VISIBLE); i++) 
        disp_writeText(5, 4+i*11, items[st+i].mc_name); 
#endif
    
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

static void gui_thread (void* arg) 
{
    while (true) {
        sleepMs(5000);
             
        if (batt_charge() && !charging)
            { beeps("-.-.  "); blipUp(); }
        if (!batt_charge() && charging)
            { beeps("-.-.  "); blipDown(); }
        charging = batt_charge();
    
        if (!menu_is_active() && !disp_popupActive())
            status_show();
         
 //        igate_on(GET_BYTE_PARAM("IGATE.on"));
 //        digipeater_on(GET_BYTE_PARAM("DIGIPEATER.on"));
    }
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
  
#if DISPLAY_TYPE == 0
  disp_circle(40,24,10);
  disp_line(40,2,40,55);
  disp_line(14,24,66,24);
  disp_writeText(2,7,"Arctic");
  disp_writeText(43,36, "Tracker");
#elif DISPLAY_HEIGHT >= 64
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


static void mhandle_dispBl(void* x) {
    disp_toggleBacklight(); 
}

static void mhandle_send(void* x) {
    tracker_posReport(); 
}

static void mhandle_igate(void* x) {
    bool isOn = get_byte_param("IGATE.on", 0);
    set_byte_param("IGATE.on", !isOn);
    igate_activate( !isOn ); 
}

static void mhandle_digi(void* x) {
    bool isOn = get_byte_param("DIGIPEATER.on", 0); 
    set_byte_param("DIGIPEATER.on", !isOn); 
    digipeater_activate( !isOn ); 
}

static void mhandle_softAp(void* x) {
    bool isOn = get_byte_param("SOFTAP.on", 0);
    set_byte_param("SOFTAP.on", !isOn);
    wifi_enable_softAp( !isOn ); 
}

static void mhandle_tracklog(void* x) {
    bool isOn = get_byte_param("TRKLOG.on", 0) && get_byte_param("TRKLOG.POST.on", 0) ;
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
    bool isOn = get_byte_param("WIFI.on", 0);
    set_byte_param("WIFI.on", !isOn);
    wifi_enable( !isOn ); 
}

static void mhandle_fwupgrade(void* x) {
    firmware_upgrade(); 
}

static void mhandle_shutdown(void* x) {
    disp_clear(); 
    disp_flush();
    systemShutdown(); 
}
