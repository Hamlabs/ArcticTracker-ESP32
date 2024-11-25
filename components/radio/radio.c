
#include <stdint.h>
#include "driver/uart.h"
#include "defines.h"



#if defined(RADIO_DISABLE)

/* dummy functions */
uint8_t radio_getRssi() {return 0;}
void radio_on(bool on) {}
bool radio_is_on() { return false; }
bool radio_tx_is_on() { return false; }
void radio_require() {}
void radio_release() {}
void radio_wait_enabled() {}
void radio_init() {}
bool radio_setFreq(uint32_t txfreq, uint32_t rxfreq) { return false; }
bool radio_setSquelch(uint8_t sq) { return false; }
void radio_PTT(bool on) {}
void radio_PTT_I(bool on) {}
bool radio_setVolume(uint8_t vol) { return false; }
bool radio_setMicLevel(uint8_t level) { return false; }
bool radio_powerSave(bool on) { return false; }
bool radio_setLowTxPower(bool on) { return false; } 
bool radio_isLowTxPower(void) { return true; } 
void wait_channel_ready(void) {}


#elif defined(ARCTIC4_UHF)

void lora_init();
void lora_on(bool on); 
bool lora_is_on();
void lora_require();
void lora_release();
uint8_t lora_GetRssiInst();



/* dummy functions - mostly */
uint8_t radio_getRssi() { return lora_GetRssiInst(); }
void radio_on(bool on)  { lora_on(on); }
bool radio_is_on()      { return lora_is_on(); }
bool radio_tx_is_on()   { return false; }
void radio_require()    { lora_require();}
void radio_release()    { lora_release();}
void radio_wait_enabled() {}
void radio_init()       { lora_init(); }
bool radio_setFreq(uint32_t txfreq, uint32_t rxfreq) { return false;}
bool radio_setSquelch(uint8_t sq) { return false; }
void radio_PTT(bool on) {}
void radio_PTT_I(bool on) {}
bool radio_setVolume(uint8_t vol) { return false; }
bool radio_setMicLevel(uint8_t level) { return false; }
bool radio_powerSave(bool on) { return false; }
bool radio_setLowTxPower(bool on) { return false; } 
bool radio_isLowTxPower(void) { return true; } 
void wait_channel_ready(void) {}


#elif DEVICE == T_TWR || DEVICE == ARCTIC4
/*
 * FIXME: The two implementations sa8 and frs are rather similar. 
 * Maybe we can simplify this.
 */
 uint8_t sa8_getRssi(); 
 void sa8_on(bool on);
 bool sa8_is_on(void);
 bool sa8_tx_is_on(void); 
 void sa8_require(void);
 void sa8_release(void);
 void sa8_wait_enabled(void);
 void sa8_init(uart_port_t uart);
 bool sa8_setFreq(uint32_t txfreq, uint32_t rxfreq);
 bool sa8_setSquelch(uint8_t sq);
 bool sa8_getSquelch();
 void sa8_PTT(bool on);
 void sa8_PTT_I(bool on);
 bool sa8_setVolume(uint8_t vol);
 bool sa8_setMicLevel(uint8_t level);
 bool sa8_powerSave(bool on);
 bool sa8_setLowTxPower(bool on); 
 bool sa8_isLowTxPower(void); 
 void sa8_wait_channel_ready(void);
 void sa8_wait_tx_off(void);
 
 uint8_t radio_getRssi() { return sa8_getRssi(); }
 void radio_on(bool on)  { sa8_on(on); }
 bool radio_is_on() { return sa8_is_on(); }
 bool radio_tx_is_on() { return sa8_tx_is_on(); }
 void radio_require() { sa8_require(); }
 void radio_release() { sa8_release(); }
 void radio_wait_enabled() { sa8_wait_enabled(); }
 void radio_init() { sa8_init(RADIO_UART); }
 bool radio_setFreq(uint32_t txfreq, uint32_t rxfreq) { return sa8_setFreq(txfreq, rxfreq); }
 bool radio_setSquelch(uint8_t sq) { return sa8_setSquelch(sq); }
 bool radio_getSquelch() { return sa8_getSquelch(); }
 void radio_PTT(bool on) { sa8_PTT(on); }
 void radio_PTT_I(bool on) { sa8_PTT_I(on); }
 bool radio_setVolume(uint8_t vol) { return sa8_setVolume(vol); }
 bool radio_setMicLevel(uint8_t level) { return sa8_setMicLevel(level); }
 bool radio_powerSave(bool on) { return sa8_powerSave(on); }
 bool radio_setLowTxPower(bool on) { return sa8_setLowTxPower(on); } 
 bool radio_isLowTxPower(void) { return sa8_isLowTxPower(); }
 void wait_channel_ready(void) { sa8_wait_channel_ready(); }
 void wait_tx_off(void)  {sa8_wait_tx_off();}

 #else
 
 
 uint8_t frs_getRssi(); 
 void frs_on(bool on);
 bool frs_is_on(void);
 void frs_require(void);
 void frs_release(void);
 void frs_wait_enabled(void);
 void frs_init(uart_port_t uart);
 bool frs_setFreq(uint32_t txfreq, uint32_t rxfreq);
 bool frs_setSquelch(uint8_t sq);
 void frs_PTT(bool on);
 void frs_PTT_I(bool on);
 bool frs_setVolume(uint8_t vol);
 bool frs_setMicLevel(uint8_t level);
 bool frs_powerSave(bool on);
 bool frs_setLowTxPower(bool on); 
 bool frs_isLowTxPower(void); 
 void frs_wait_channel_ready(void);
 void frs_wait_tx_off(void);
 
 uint8_t radio_getRssi()   { return frs_getRssi(); }
 void radio_on(bool on) { frs_on(on); }
 bool radio_is_on() { return frs_is_on(); }
 bool radio_tx_is_on() { return frs_is_on(); }
 void radio_require() { frs_require(); }
 void radio_release() { frs_release(); }
 void radio_wait_enabled() { frs_wait_enabled(); }
 void radio_init() { frs_init(RADIO_UART); }
 bool radio_setFreq(uint32_t txfreq, uint32_t rxfreq) { return frs_setFreq(txfreq, rxfreq); }
 bool radio_setSquelch(uint8_t sq) { return frs_setSquelch(sq); }
 void radio_PTT(bool on) { frs_PTT(on); }
 void radio_PTT_I(bool on) { frs_PTT_I(on); }
 bool radio_setVolume(uint8_t vol) { return frs_setVolume(vol); }
 bool radio_setMicLevel(uint8_t level) { return frs_setMicLevel(level); }
 bool radio_powerSave(bool on) { return frs_powerSave(on); }
 bool radio_setLowTxPower(bool on) { return frs_setLowTxPower(on); } 
 bool radio_isLowTxPower(void) { return frs_isLowTxPower(); }
 void wait_channel_ready(void) { frs_wait_channel_ready(); }
 void wait_tx_off(void)  {frs_wait_tx_off();}
 
#endif

 
