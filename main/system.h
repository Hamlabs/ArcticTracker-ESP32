 /*
 * Misc. System related stuff
 * By LA7ECA, ohanssen@acm.org
 */

#include "driver/uart.h"
#include "esp_log.h"
#include "driver/timer.h"


#if !defined __DEF_SYSTEM_H__
#define __DEF_SYSTEM_H__


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
void set_logLevels(void);
char* loglevel2str(esp_log_level_t lvl);
esp_log_level_t str2loglevel(char* str);


/* Serial communication */
bool readline(uart_port_t port, char* buf, const uint16_t max); 


/* Text utilities */
uint8_t tokenize(char*, char*[], uint8_t, char*, bool);

#endif
