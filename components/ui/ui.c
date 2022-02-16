/*
 * User interface setup. 
 * By LA7ECA, ohanssen@acm.org
 */


#include "defines.h"
#include "system.h"
#include "ui.h"
#include "disp_nokia.h"
#include "gui.h"
#include "driver/gpio.h"

extern void buzzer_init();
static void bphandler(void* p);
static void holdhandler(void* p);
static void clickhandler(void* p);

#define TAG "ui"

static void click_handler(void* p); 
static void push_handler(void* p);
  
/*********************************************************************
 * UI service thread to handle button events 
 *********************************************************************/

static cond_t buttCond;  
 
 
#define BUTT_EV_SHORT BIT0
#define BUTT_EV_LONG  BIT1
 
 
 
 static void ui_service_thread (void* arg) 
 {
    (void)arg;
   
    while (true) {
       cond_waitBits(buttCond, BUTT_EV_SHORT|BUTT_EV_LONG);
       if ( cond_testBits(buttCond, BUTT_EV_SHORT) ) {
          ESP_LOGI(TAG, "Butt ev short");
          beep(20); 
          click_handler(NULL);
          cond_clearBits(buttCond, BUTT_EV_LONG|BUTT_EV_SHORT);
       }
       else {
          ESP_LOGI(TAG, "Butt ev long");
          beeps("-");
          push_handler(NULL);
          cond_clearBits(buttCond, BUTT_EV_LONG|BUTT_EV_SHORT);
       }
    }
 }  

 
 
/**************************************
 * Pushbutton handler. 
 **************************************/
 
static bool buttval = 0;
static bool pressed; 
static TimerHandle_t bptimer, bhtimer;


static void button_handler(void* x) 
{
    BaseType_t hpw = pdFALSE;
    buttval = gpio_get_level(BUTTON_PIN); 
    xTimerStopFromISR(bptimer, &hpw);
    xTimerStopFromISR(bhtimer, &hpw);

    if (buttval==0) {
        xTimerStartFromISR(bptimer, &hpw);
        xTimerStartFromISR(bhtimer, &hpw);
    }
    else {
        if (pressed) 
            clickhandler(NULL);
        pressed = false; 
    }
}
 
 
static void bphandler(void* p)
{
    (void) p;
    /* If the button has been pressed down for 10ms */
    if ((gpio_get_level(BUTTON_PIN) == 0) && buttval==0)
       pressed = true;
}
 

static void clickhandler(void* p) {
    (void) p;  
    cond_setBits(buttCond, BUTT_EV_SHORT);
}


static void holdhandler(void* p) {
    (void)p;
    pressed = false;
    cond_setBits(buttCond, BUTT_EV_LONG);
}



static void button_init() {
         
    /* Software timers */
    bptimer = xTimerCreate ( 
        "Button press", pdMS_TO_TICKS(15),  pdFALSE,
        ( void * ) 0, bphandler
    );  
    bhtimer = xTimerCreate ( 
        "Button hold", pdMS_TO_TICKS(800),  pdFALSE,
        ( void * ) 0, holdhandler
    );
    
    buttCond = cond_create();
    xTaskCreatePinnedToCore(&ui_service_thread, "UI Service thd", 
        STACK_UI_SRV, NULL, NORMALPRIO, NULL, CORE_UI_SRV);
    
    /* Button pin. Pin interrupt */
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(BUTTON_PIN, button_handler, NULL);
    gpio_set_direction(BUTTON_PIN,  GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);
    gpio_pullup_en(BUTTON_PIN);
    gpio_intr_enable(BUTTON_PIN);
    
 }
 

    
 /*********************************************************************
  * Main UI thread. LED blinking to indicate that it is alive
  *********************************************************************/
 
 uint16_t blink_length=500, blink_interval=500;
 bool blink_both=false;

 
 static void ui_thread(void* arg)
 {
   (void)arg;
   blipUp();
   sleepMs(300);
   /* Blink LED */
   BLINK_NORMAL;
   while (1) {
     gpio_set_level(LED_STATUS_PIN, 1);
     if (blink_both)
         gpio_set_level(LED_TX_PIN, 0);
     sleepMs(blink_length);
     gpio_set_level(LED_STATUS_PIN, 0);
     if (blink_both)
        gpio_set_level(LED_TX_PIN, 1);
     sleepMs(blink_interval);
   }
 }
 
 
 
 /******************************************
  * Button event handlers. 
  ******************************************/
 
 static void push_handler(void* x) {
    lcd_backlight();
    if (menu_is_active())
        menu_select();
    else
        menu_activate(); 
 }

 
 
 static void click_handler(void* p) {
    lcd_backlight();
    if (menu_is_active())
        menu_increment();
    else
        status_next();
 }

 
 /*****************************************
  * UI init
  *****************************************/
 
 void ui_init()
 {   
    /* Button */
    button_init();
        
    /* Buzzer */
    buzzer_init(); 
    
    /* LCD display */  
    lcd_init();
    sleepMs(100);
    lcd_backlight();
    gui_welcome(); 
    status_init(); 
        
    
   /* LED blinker thread */
    gpio_set_direction(LED_STATUS_PIN,  GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_TX_PIN,  GPIO_MODE_OUTPUT);
    gpio_set_level(LED_STATUS_PIN, 0);
    gpio_set_level(LED_TX_PIN, 0);
    
    xTaskCreatePinnedToCore(&ui_thread, "LED blinker", 
        STACK_LEDBLINKER, NULL, NORMALPRIO, NULL, CORE_LEDBLINKER);
    
    menu_init();
 }
 
