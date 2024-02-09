/* 
 * Generate beeps, etc, using the buzzer
 */

#include "defines.h"
#include <stdint.h>
#include "driver/gpio.h"
#include "system.h" 
#include "ui.h"


#define BUZZ_RESOLUTION 1000000
#define BUZZ_CNT 100

#define FREQ2CNT(f) BUZZ_RESOLUTION/((f)*2) 


static void buzzer_start(uint16_t freq);
static void buzzer_stop(void);


/********************************************
 * Buzzer stuff
 ********************************************/

volatile int buzz_pin = 0; 

static bool buzzer_isr(struct gptimer_t * t, const gptimer_alarm_event_data_t * a, void * arg) 
{
    buzz_pin = (buzz_pin==1 ? 0 : 1);
    gpio_set_level(BUZZER_PIN, buzz_pin);    
    return true;
}



void _beep(uint16_t freq, uint16_t time) {
    buzzer_start(freq);
    sleepMs(time);
    buzzer_stop();
}


/**************************************************
 *  short double beep using two frequencies
 *   up (sucess or on) or down (failure or off)
 **************************************************/

void blipUp() {
    beep(60);
    hbeep(60);
}

void blipDown() {
    hbeep(60);
    beep(60);
}



/**************************************
 *  Morse code 
 **************************************/

void beeps(char* s)
{
    while (*s != 0) {
        if (*s == '.')
            beep(50);
        else if (*s == '-')
            beep(150);
        else
            sleepMs(100);
        sleepMs(50);  
        s++;
    }
}



/**************************************
 * Init buzzer
 **************************************/
static clock_t buzzer;

void buzzer_init() {
#if BUZZER_PIN != -1
    gpio_set_direction(BUZZER_PIN,  GPIO_MODE_OUTPUT);
    clock_init(&buzzer, BUZZ_RESOLUTION, BUZZ_CNT, buzzer_isr, NULL);
#endif
}



/**************************************************************************
 * Start generating a tone .
 **************************************************************************/

static void buzzer_start(uint16_t freq) {
#if BUZZER_PIN != -1
    clock_set_interval(buzzer, FREQ2CNT(freq));
    clock_start(buzzer); 
#endif
}



/**************************************************************************
 * Stop generating a tone. 
 **************************************************************************/

static void buzzer_stop() {
#if BUZZER_PIN != -1
    clock_stop(buzzer);
    gpio_set_level(BUZZER_PIN, 0);
#endif
}


