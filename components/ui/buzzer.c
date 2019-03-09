/* 
 * Generate beeps, etc, using the buzzer
 */

#include "defines.h"
#include <stdint.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "system.h" 
#include "ui.h"


#define CLOCK_DIVIDER 16
#define CLOCK_FREQ (TIMER_BASE_CLK / CLOCK_DIVIDER)
#define FREQ(f)    (CLOCK_FREQ/(f*2))


static void buzzer_start(uint16_t freq);
static void buzzer_stop(void);


/********************************************
 * Buzzer stuff
 ********************************************/

volatile int buzz_pin = 0; 

static void IRAM_ATTR buzzer_isr(void* arg) 
{
    clock_clear_intr(BUZZER_TIMERGRP, BUZZER_TIMERIDX);
    buzz_pin = (buzz_pin==1 ? 0 : 1);
    gpio_set_level(BUZZER_PIN, buzz_pin);    
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

void buzzer_init() {
    gpio_set_direction(BUZZER_PIN,  GPIO_MODE_OUTPUT);
    clock_init(BUZZER_TIMERGRP, BUZZER_TIMERIDX, CLOCK_DIVIDER, buzzer_isr, true);
}



/**************************************************************************
 * Start generating a tone .
 **************************************************************************/

static void buzzer_start(uint16_t freq) {
    clock_start(BUZZER_TIMERGRP, BUZZER_TIMERIDX, FREQ(freq)); 
}



/**************************************************************************
 * Stop generating a tone. 
 **************************************************************************/

static void buzzer_stop() {
    clock_stop(BUZZER_TIMERGRP, BUZZER_TIMERIDX);
    gpio_set_level(BUZZER_PIN, 0);
}


