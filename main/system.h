 /*
 * Misc. System related stuff
 * By LA7ECA, ohanssen@acm.org
 */

#include "driver/uart.h"
#include "esp_log.h"
#include "driver/timer.h"
#include "driver/adc.h"

#if !defined __DEF_SYSTEM_H__
#define __DEF_SYSTEM_H__

/* Firmware upgrade */
esp_err_t firmware_upgrade();


/* Time */
extern void time_init(); 
bool time_getUTC(struct tm *timeinfo);


/* Hardware timer - as periodic clocks */
void clock_init(int group, int idx, uint16_t divider,  void (*isr)(void *), bool iram);
void clock_start(int group, int idx, double interval);
void clock_stop(int group, int idx);
void clock_changeInterval(int group, int idx, double interval);
void IRAM_ATTR clock_clear_intr(int group, int index);

 
/* Logging */
bool hasTag(char*tag);
void set_logLevels(void);
char* loglevel2str(esp_log_level_t lvl);
esp_log_level_t str2loglevel(char* str);


/* Serial communication */
bool readline(uart_port_t port, char* buf, const uint16_t max); 


/* Text utilities */
uint8_t tokenize(char*, char*[], uint8_t, char*, bool);


/* ADC */
void adc_print_char();
void adc_init();
uint16_t adc_read(uint8_t chan);
uint16_t adc_toVoltage(uint16_t val);
uint16_t adc_batt();
uint16_t adc_batt_status(char* line1, char* line2);
void adc_start_sampling();
void adc_stop_sampling();


#endif
