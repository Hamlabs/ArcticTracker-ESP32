 #if !defined __RADIO_H__
 #define __RADIO_H__
  
 #include <stdint.h>
 #include "driver/uart.h"

 bool radio_is_on(void);
 void radio_require(void);
 void radio_release(void);
 void radio_wait_enabled(void);
 void radio_init(uart_port_t uart);
 bool radio_setFreq(uint32_t txfreq, uint32_t rxfreq);
 bool radio_setSquelch(uint8_t sq);
 void radio_on(bool on);
 void radio_PTT(bool on);
 void radio_PTT_I(bool on);
 bool radio_setVolume(uint8_t vol);
 bool radio_setMicLevel(uint8_t level);
 bool radio_powerSave(bool on);
 bool radio_setLowTxPower(bool on); 
 bool radio_isLowTxPower(void); 
 // void squelch_handler(EXTDriver *extp, expchannel_t channel);
 void wait_channel_ready(void);
 void radio_wait_enabled(void);
 
#endif
