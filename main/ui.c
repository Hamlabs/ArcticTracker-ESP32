/*
 * User interface setup. 
 * By LA7ECA, ohanssen@acm.org
 */


#include "defines.h"
#include "esp_log.h"
#include "system.h"
#include "ui.h"
#include "lcd.h"
#include "gui.h"


static void bphandler(void* p);
static void holdhandler(void* p);
static void clickhandler(void* p);

#define TAG "ui"

static void blipUp() {} // Dummy, TBD

 
 /*********************************************************************
  * Main UI thread. LED blinking to indicate that it is alive
  *********************************************************************/
 
 uint16_t blink_length=500, blink_interval=500;

 
 static void ui_thread(void* arg)
 {
   (void)arg;
   int cnt = 0;
   blipUp();
   sleepMs(300);
   
   /* Blink LED */
   BLINK_NORMAL;
   while (1) {
     gpio_set_level(LED_STATUS_PIN, 1);
     sleepMs(blink_length);
     gpio_set_level(LED_STATUS_PIN, 0);
     sleepMs(blink_interval);
   }
 }
 
 
 
 /*****************************************
  * UI init
  *****************************************/
 
 void ui_init()
 {   
   lcd_init();
   sleepMs(100);
   lcd_backlight();
   gui_welcome();
   
    gpio_set_direction(LED_STATUS_PIN,  GPIO_MODE_OUTPUT);
    xTaskCreate(&ui_thread, "LED blinker", 
        STACK_LEDBLINKER, NULL, NORMALPRIO, NULL);
 }
 
