 /*
 * Misc. System related stuff
 * By LA7ECA, ohanssen@acm.org
 */

#include "driver/uart.h"


#if !defined __DEF_SYSTEM_H__
#define __DEF_SYSTEM_H__

/* Time */
extern void time_init(); 
bool time_getUTC(struct tm *timeinfo);
 
/* Logging */
void set_logLevels(void);
char* loglevel2str(esp_log_level_t lvl);
esp_log_level_t str2loglevel(char* str);

/* Serial communication */
bool readline(uart_port_t port, char* buf, const uint16_t max); 

/* Text utilities */
uint8_t tokenize(char*, char*[], uint8_t, char*, bool);

#endif
