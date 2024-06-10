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

#define TAG "ui"

extern void neopixel_init(); 
extern void buzzer_init();
static void bphandler(struct tmrTimerControl * p);
static void holdhandler(struct tmrTimerControl * p);
static void click_handler(void* p, bool up); 
static void push_handler(void* p);
  


/*********************************************************************
 * UI service thread to handle button events 
 *********************************************************************/

static cond_t buttCond;  
 
 
#define BUTT_EV_SHORT   BIT0
#define BUTT_EV_LONG    BIT1
#define BUTT_EV_UP      BIT2
#define BUTT_EV_DOWN    BIT3
#define BUTT_EV_CONFIRM BIT4
 


 static void ui_service_thread (void* arg) 
 {
    (void)arg;
   
    while (true) {
        cond_waitBits(buttCond, BUTT_EV_SHORT|BUTT_EV_LONG|BUTT_EV_UP|BUTT_EV_DOWN|BUTT_EV_CONFIRM);
       
        if ( cond_testBits(buttCond, BUTT_EV_UP) ) {
            ESP_LOGI(TAG, "Butt ev UP");
            click_handler(NULL, true);
        }  
        else if ( cond_testBits(buttCond, BUTT_EV_DOWN) ) {
            ESP_LOGI(TAG, "Butt ev DOWN");
            click_handler(NULL, false);
        }
        else if ( cond_testBits(buttCond, BUTT_EV_SHORT) ) {
            ESP_LOGI(TAG, "Butt ev short");
            click_handler(NULL, true);
        }
#if DEVICE==T_TWR
        else if ( cond_testBits(buttCond, BUTT_EV_CONFIRM) ) {
            ESP_LOGI(TAG, "Butt ev confirm (wheel push)");
            if (menu_is_active())
                push_handler(NULL);
            else
                click_handler(NULL, true);
        }
#endif
        else {
            ESP_LOGI(TAG, "Butt ev long");
            push_handler(NULL);
        }
        cond_clearBits(buttCond, BUTT_EV_LONG|BUTT_EV_SHORT|BUTT_EV_UP|BUTT_EV_DOWN|BUTT_EV_CONFIRM);
    }
 }  

 
 
 
/**************************************
 * Rotary encoder for T-TWR device
 **************************************/
  
#if DEVICE == T_TWR


/*
 * State machine (simplified). Same code again keeps it in the same state. 
 * Unexpected code returns it to INITIAL:
 * 
 * State      AB    Action/next state
 * ----------------------------------
 * INITIAL    01 -> START_UP
 *            10 -> START_DOWN
 * 
 * START_UP   11 -> MID_UP
 * MID_UP     10 -> END_UP
 *            01 -> START_UP (back)
 * END_UP     00 -> INITIAL; send event: BUTT_EV_UP
 *            11 -> MID_UP   (back)
 * 
 * START_DOWN 11 -> MID_DOWN
 * MID_DOWN   01 -> END_DOWN
 *            10 -> START_DOWN
 * END_DOWN   00 -> INITIAL; send event: BUTT_EV_DOWN
 *            11 -> MID_DOWN
 * 
 */

#define INITIAL     0
#define START_UP    1
#define MID_UP      2
#define END_UP      3
#define START_DOWN  4
#define MID_DOWN    5
#define END_DOWN    6

static int state = INITIAL;

    

static int next_state(bool p1, bool p2) 
{
    switch (state) 
    {
        case INITIAL:    if (p1 && !p2) return START_UP;
                         if (!p1 && p2) return START_DOWN;
                         return INITIAL;
        
        case START_UP:   if (p1 && p2) return MID_UP; 
                         if (p1 && !p2) return START_UP;
                         return INITIAL;
        
        case MID_UP:     if (!p1 && p2) return END_UP;
                         if (p1 && !p2) return START_UP; // back
                         if (p1 && p2) return MID_UP;
                         return INITIAL;
        
        case END_UP:     if (!p1 && !p2) cond_setBits(buttCond, BUTT_EV_UP);
                         if (p1 && p2) return MID_UP; // back
                         if (!p1 && p2) return END_UP;
                         return INITIAL;
                  
        case START_DOWN: if (p1 && p2) return MID_DOWN; 
                         if (!p1 && p2) return START_DOWN;
                         return INITIAL; 
        
        case MID_DOWN:   if (p1 && !p2) return END_DOWN;
                         if (!p1 && p2) return START_DOWN; // back
                         if (p1 && p2) return MID_DOWN; 
                         return INITIAL;
        
        case END_DOWN:   if (!p1 && !p2) cond_setBits(buttCond, BUTT_EV_DOWN);
                         if (p1 && p2) return MID_DOWN;
                         if (p1 && !p2) return END_DOWN;
                         return INITIAL;           
    } 
    return INITIAL;
}



static void enc_handler(void* x) {
    bool pin1 = gpio_get_level(ENC_A_PIN);
    bool pin2 = gpio_get_level(ENC_B_PIN);
    state = next_state(!pin1, !pin2);
}


#endif




 
/**************************************
 * Pushbutton handler. 
 **************************************/
 
static bool buttval = 1;
static bool buttval2 = 1;
static bool pressed; 
static bool butt2press; 

static TimerHandle_t bptimer, bhtimer;


/* This is a ISR for handling button */

static void button_handler(void* x) 
{
    BaseType_t hpw = pdFALSE;
    buttval = gpio_get_level(BUTTON_PIN);
#if DEVICE == T_TWR
    buttval2 = gpio_get_level(ENC_PUSH_PIN);
    if (buttval2==0) 
        butt2press = true;
#endif
    xTimerStopFromISR(bptimer, &hpw);
    xTimerStopFromISR(bhtimer, &hpw);

    if (buttval==0 || buttval2==0) {
        xTimerStartFromISR(bptimer, &hpw);
        xTimerStartFromISR(bhtimer, &hpw);
    }
    else {
        if (pressed) {
            cond_setBits(buttCond, (butt2press ? BUTT_EV_CONFIRM : BUTT_EV_SHORT));
            pressed = false; 
            butt2press = false;
        }
    }
}
 

 
/* This function is called by a timer (bptimer) */

static void bphandler(struct tmrTimerControl* p)
{
    (void) p;
    /* If the button has been pressed down for 10ms */
#if DEVICE == T_TWR
    if ((gpio_get_level(BUTTON_PIN) == 0 && buttval==0) ||
        (gpio_get_level(ENC_PUSH_PIN) == 0 && buttval2==0) )
       pressed = true;
#else
    if ((gpio_get_level(BUTTON_PIN) == 0) && buttval==0)
       pressed = true;
#endif

}
 

/* This function is called by a timer (bhtimer) */

static void holdhandler(struct tmrTimerControl *p) {
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
        "Button hold", pdMS_TO_TICKS(1000),  pdFALSE,
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

#if DEVICE == T_TWR
    gpio_set_intr_type(ENC_PUSH_PIN, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(ENC_PUSH_PIN, button_handler, NULL);
    gpio_set_direction(ENC_PUSH_PIN,  GPIO_MODE_INPUT);
    gpio_intr_enable(ENC_PUSH_PIN);
    
    gpio_set_intr_type(ENC_A_PIN, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(ENC_A_PIN, enc_handler, NULL);
    gpio_set_direction(ENC_A_PIN,  GPIO_MODE_INPUT);
    gpio_set_pull_mode(ENC_A_PIN, GPIO_PULLUP_ONLY);
    gpio_intr_enable(ENC_A_PIN);
    
    gpio_set_intr_type(ENC_B_PIN, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(ENC_B_PIN, enc_handler, NULL);
    gpio_set_direction(ENC_B_PIN,  GPIO_MODE_INPUT);
    gpio_set_pull_mode(ENC_B_PIN, GPIO_PULLUP_ONLY);
    gpio_intr_enable(ENC_B_PIN);
    
#endif
    
 }
 

 
 /******************************************
  * Button event handlers. 
  ******************************************/
 
 static void push_handler(void* x) {
    disp_backlight();
    beeps("-");
    if (menu_is_active())
        menu_select();
    else
        menu_activate();
 }

 
 
 static void click_handler(void* p, bool up) {
    bool dim = disp_isDimmed();
    disp_backlight();  
    if (dim) {
        ESP_LOGI(TAG, "Display is dimmed. Just turn it on");
        return;
    }
    beep(20); 
    if (menu_is_active()) {
        if (up) menu_increment();
        else menu_decrement();
    }
    else {
        if (up) status_next(); 
        else status_prev();
    }
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

    /* LEDs */
    led_init();
    
    /* Display */  
    disp_init();
    sleepMs(100);
    disp_backlight();
    gui_welcome(); 
    status_init(); 
    menu_init();
 }
 
