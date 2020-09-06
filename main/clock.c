 /*
  * Hardware timers - used as clocks with periodic ticks. 
  * By LA7ECA, ohanssen@acm.org
  */

#include "system.h"

/************************************************************************
 * Timer setup
 ************************************************************************/

void clock_init(int group, int idx, uint16_t divider,  void (*isr)(void *), bool iram)
{
    /* Select and initialize basic parameters of the timer */
    static timer_config_t config;
    config.divider = divider;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = 1;
    timer_init(group, idx, &config);
    timer_set_counter_value(group, idx, 0x00000000ULL);
    timer_disable_intr(group, idx);
    timer_isr_register(group, idx, isr, (void *) idx, (iram? ESP_INTR_FLAG_IRAM : 0), NULL);
    timer_enable_intr(group, idx);
}


/************************************************************************
 * Start a timer with a specific interval
 ************************************************************************/

void clock_start(int group, int idx, double interval) 
{
    timer_set_counter_value(group, idx, 0x00000000ULL);
    timer_set_alarm_value(group, idx, interval);
    timer_start(group, idx);
}


/************************************************************************
 * Stop (pause) a timer
 ************************************************************************/

void clock_stop(int group, int idx) {
    timer_pause(group, idx);
}


/************************************************************************
 * Change interval of the timer without stopping it. 
 * If the current count is greater than the new alarm value,
 * interrupt will be triggered. 
 ************************************************************************/

// FIXME: This may be called from a timer ISR !!!
void clock_changeInterval(int group, int idx, double interval)
{
    uint64_t counter;  
    timer_get_counter_value(group, idx, &counter);
    if (counter >= interval)
        timer_set_counter_value(group, idx, interval-1);
    timer_set_alarm_value(group, idx, interval);
}



/*************************************************************************
 * To be used in ISR: Clear interrupt and re-enable alarm 
 *************************************************************************/

void IRAM_ATTR clock_clear_intr(int group, int index)
{
    if (group==0) {
        TIMERG0.hw_timer[index].update = 1;
        if (index==0) 
            TIMERG0.int_clr_timers.t0 = 1;
        else 
            TIMERG0.int_clr_timers.t1 = 1;
        TIMERG0.hw_timer[index].config.alarm_en = TIMER_ALARM_EN;
    }
    else {
        TIMERG1.hw_timer[index].update = 1;
        if (index==0) 
            TIMERG1.int_clr_timers.t0 = 1;
        else 
            TIMERG1.int_clr_timers.t1 = 1;
        TIMERG1.hw_timer[index].config.alarm_en = TIMER_ALARM_EN;
    }
}

