 /*
  * Hardware timers - used as clocks with periodic ticks. 
  * By LA7ECA, ohanssen@acm.org
  */

#include "system.h"
#include "driver/gptimer.h"




/************************************************************************
 * Timer setup
 ************************************************************************/

void clock_init(gptimer_handle_t *clock, uint32_t resolution, uint32_t period, gptimer_alarm_cb_t cb, void* arg)
{
    /* Select and initialize basic parameters of the timer */
    gptimer_config_t config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = resolution
    };
    ESP_ERROR_CHECK( gptimer_new_timer(&config, clock) );

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    ESP_ERROR_CHECK( gptimer_set_raw_count(*clock, 0) );
    
    /* Alarm config */
    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0, // counter will reload with 0 on alarm event
        .alarm_count = period,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(*clock, &alarm_config));
    
    /* Alarm callback */
    gptimer_event_callbacks_t cbs = {
        .on_alarm = cb, // register user callback
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(*clock, &cbs, arg));
    ESP_ERROR_CHECK(gptimer_enable(*clock));
}


/************************************************************************
 * Start a timer with a specific interval
 ************************************************************************/

void clock_start(gptimer_handle_t clock)
{
   ESP_ERROR_CHECK(gptimer_start(clock));
}


/************************************************************************
 * Stop (pause) a timer
 ************************************************************************/

void clock_stop(gptimer_handle_t clock) 
{
    ESP_ERROR_CHECK(gptimer_stop(clock));
}


/************************************************************************
 * Change interval of the timer without stopping it. 
 * If the current count is greater than the new alarm value,
 * interrupt will be triggered. 
 ************************************************************************/

void clock_set_interval(gptimer_handle_t clock, uint32_t period)
{
    /* Alarm config */
    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0, // counter will reload with 0 on alarm event
        .alarm_count = period,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(clock, &alarm_config));
}



