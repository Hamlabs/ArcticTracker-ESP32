 /*
  * Hardware timers - used as clocks with periodic ticks. 
  * By LA7ECA, ohanssen@acm.org
  */

#include "system.h"
#include "driver/timer.h"
#include "hal/timer_hal.h"



/************************************************************************
 * Timer setup
 ************************************************************************/

void clock_init(int group, int idx, uint16_t divider, timer_isr_t isr, bool iram)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = divider,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = true,
    }; // default clock source is APB
    timer_init(group, idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(group, idx, 0);
    timer_isr_callback_add(group, idx, isr, NULL, 0);
}


/************************************************************************
 * Start a timer with a specific interval
 ************************************************************************/

void clock_start(int group, int idx, double interval) 
{
    timer_set_counter_value(group, idx, 0);
    timer_set_alarm_value(group, idx, interval);
    timer_enable_intr(group, idx);
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

void clock_changeInterval(int group, int idx, double interval)
{
    uint64_t counter = 
    timer_group_get_counter_value_in_isr(group, idx);
    if (counter >= interval)
        timer_group_set_alarm_value_in_isr(group, idx, counter + 2);
    timer_group_set_alarm_value_in_isr(group, idx, interval);
}



