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


typedef void (*menucmd_t)(void*);

typedef struct {
  const char     *mc_name;  
  menucmd_t       mc_func;
  void           *mc_arg;
} MenuCommand;


/* Handler functions (defined below) */
static void mhandle_send(void*);
static void mhandle_igate(void*); 
static void mhandle_digi(void*); 
static void mhandle_wifi(void*); 
static void mhandle_fwupgrade(void*); 
static void mhandle_shutdown(void*);

static MenuCommand items[] = 
{
    { "Send report",    mhandle_send,      NULL },
    { "WIFI +|-",       mhandle_wifi,      NULL },
    { "Igate +|-",      mhandle_igate,     NULL },
    { "Digipeater +|-", mhandle_digi,      NULL },
    { "Firmware upgr.", mhandle_fwupgrade, NULL },
    { "Shut down..",    mhandle_shutdown,  NULL },
};
static int nitems = 6; 



/*********************************************************
 * Show a menu with one item highlighted
 * items is an array of strings of max length 4. 
 *********************************************************/

#define MIN(x,y) (x<y ? x : y)
#define MAX_VISIBLE 4

static uint8_t offset = 0;
static uint8_t selection = 0;
static bool menu_active;

 


static void menu_show(int st, int sel) 
{     
   gui_clear();
   gui_frame(); 

   gui_box(0, sel*11, 83, 12, true);
   int i;
   for (i=0; i < MIN(nitems,MAX_VISIBLE); i++) 
     gui_writeText(4, 2+i*11, items[st+i].mc_name); 
   gui_flush();
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
    gui_clear();
    status_show();
}



/****************************************************
 * Thread that periodically update display and turn
 * off/on igate or digipeater.
 ****************************************************/
 // FIXME Move to ui.h and rename to service Thread
 
 static void gui_thread (void* arg) 
 {
     while (true) {
         sleepMs(5000);
         
         if (!menu_is_active() && !gui_popupActive())
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



/**********************************************************
 * Menu commands
 **********************************************************/

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

static void mhandle_wifi(void* x) {
    bool isOn = wifi_isEnabled(); 
    wifi_enable( !isOn ); 
}

static void mhandle_fwupgrade(void* x) {
    firmware_upgrade(); 
}

static void mhandle_shutdown(void* x) {
    systemShutdown(); 
}
