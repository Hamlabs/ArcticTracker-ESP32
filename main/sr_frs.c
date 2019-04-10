
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h" 
#include "defines.h"
#include "config.h"
#include "hdlc.h"
#include "afsk.h"
#include "system.h"
#include "radio.h"
#include "ui.h"


#define FLAG_BUSY_LOCK  0x01
#define FLAG_COMP_EXP   0x02
#define FLAG_LO_POWER   0x04
#define TAG "radio"


static bool     _on = false;
static bool     _sq_on = false; 
static uint8_t  _widebw; 
static uint8_t  _flags;        
static uint32_t _txfreq;       // TX frequency in 100 Hz units
static uint32_t _rxfreq;       // RX frequency in 100 Hz units
static uint8_t  _squelch;      // Squelch level (0-8 where 0 is open)

static int count = 0; 
  
static bool _handshake(void);
static bool _setGroupParm(void);
static void _initialize(void);
static void squelch_handler(void* arg);

/*
 * Serial driver config
 */

static uart_config_t _serialConfig = {
    .baud_rate = 9600,
    .data_bits = UART_DATA_8_BITS,
    .parity    = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
};
static uart_port_t _serial;


static mutex_t radio_mutex;
static mutex_t ptt_mutex;

// FIXME: Should it be a condition variable? 
static semaphore_t tx_off;
#define WAIT_TX_OFF   sem_down(tx_off)
#define SIGNAL_TX_OFF sem_up(tx_off);
#define CLEAR_TX_OFF 
  
static cond_t radio_rdy;
#define WAIT_RADIO_READY cond_wait(radio_rdy)
#define SIGNAL_RADIO_READY cond_set(radio_rdy)
#define CLEAR_RADIO_READY cond_clear(radio_rdy);

static cond_t chan_rdy;
#define WAIT_CHANNEL_READY cond_wait(chan_rdy)



/***********************************************
 * Initialize
 ***********************************************/


void radio_init(uart_port_t uart)
{  
    uart_param_config(uart, &_serialConfig);
    uart_set_pin(uart, RADIO_PIN_TXD, RADIO_PIN_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(uart, RADIO_BUF_SIZE, 0, 0, NULL, 0);
    _serial = uart;
    
    gpio_set_direction(RADIO_PIN_PTT,  GPIO_MODE_OUTPUT); 
    gpio_set_direction(RADIO_PIN_TXP,  GPIO_MODE_OUTPUT); 

    radio_mutex = mutex_create();
    ptt_mutex = mutex_create();
    tx_off = sem_createBin();
    radio_rdy = cond_create();
    chan_rdy = cond_create();
    radio_setLowTxPower(true);
    radio_PTT(false);
    
    /* Squelch input. Pin interrupt */
    gpio_set_intr_type(RADIO_PIN_SQUELCH, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(RADIO_PIN_SQUELCH, squelch_handler, NULL);
    gpio_set_direction(RADIO_PIN_SQUELCH, GPIO_MODE_INPUT);
    gpio_intr_enable(RADIO_PIN_SQUELCH);
}
  
 
 
static void _initialize()
{  
   radio_PTT(false);
   sleepMs(600);
   _handshake();
   sleepMs(100);
  
   /* Get parameters from EEPROM */
   _txfreq = GET_I32_PARAM("TXFREQ");
   _rxfreq = GET_I32_PARAM("RXFREQ");
   _squelch = GET_BYTE_PARAM("TRX_SQUELCH");
   _flags = 0x00;
   _widebw = 0;
   _setGroupParm();  
   sleepMs(100);
   ESP_LOGI(TAG, "_initialize: txfreq=%u, rxfreq=%u", _txfreq, _rxfreq);
}
  
 
 

/************************************************
 * Squelch handler (ISR function)
 ************************************************/

static void squelch_handler(void* arg) 
{
    if (!_sq_on && cond_isSetI(radio_rdy) && !gpio_get_level(RADIO_PIN_SQUELCH)) {
        _sq_on = true;
        afsk_rx_enable();
        cond_clearI(radio_rdy);
    }
    else if (_sq_on) {
        _sq_on = false; 
        afsk_rx_disable();
        cond_setI(radio_rdy);
    }
}




/******************************************************
 * Need radio - turn it on if not already on
 ******************************************************/
 
void radio_require(void)
{
   mutex_lock(radio_mutex);
   if (++count == 1) 
     radio_on(true);
   mutex_unlock(radio_mutex);
}



/*******************************************************
 * Radio not needed any more - turn it off if no others
 * need it
 *******************************************************/
 
void radio_release(void)
{
    mutex_lock(radio_mutex);
    if (--count == 0) {
       /* 
        * Before turning off transceiver, wait until
        * Packet is sent and transmitter is turned off. 
        */
       sleepMs(60);
       hdlc_wait_idle();
       WAIT_TX_OFF;
       radio_on(false);
    }
    if (count < 0) count = 0;
    mutex_unlock(radio_mutex);
}



/************************************************
 * Wait until radio is ready 
 ************************************************/

void radio_wait_enabled() 
{
  mutex_lock(radio_mutex);
  WAIT_RADIO_READY;
  mutex_unlock(radio_mutex);
}



/************************************************
 * Wait until channel is ready 
 ************************************************/
void wait_channel_ready()
{
  /* Wait to radio is on and squelch is closed */
  WAIT_CHANNEL_READY;
}



/************************************************
 * Power on
 ************************************************/

void radio_on(bool on)
{
   if (on == _on)
      return; 
   
   ESP_LOGI(TAG, "radio_on: %s", (on? "true":"false"));
   _on = on;
   if (on) {
      gpio_set_level(RADIO_PIN_PD, 1);
      _initialize();
      SIGNAL_RADIO_READY;
   }
   else {
      gpio_set_level(RADIO_PIN_PD, 0);
      CLEAR_RADIO_READY;
   }
}



/************************************************
 * PTT 
 ************************************************/

void radio_PTT(bool on)
{
    if (!_on)
       return;
    
    ESP_LOGI(TAG, "radio_PTT: %s", (on? "true":"false"));
    if (on) {
        WAIT_TX_OFF;
        mutex_lock(ptt_mutex);
        gpio_set_level(RADIO_PIN_PTT, 0);
        gpio_set_level(RADIO_PIN_TXP, 1); 
        tx_led_on();
        CLEAR_TX_OFF;
        mutex_unlock(ptt_mutex);
    }
    else {
        mutex_lock(ptt_mutex);
        gpio_set_level(RADIO_PIN_PTT, 0);
        gpio_set_level(RADIO_PIN_TXP, 1); 
        tx_led_off();
        SIGNAL_TX_OFF;
        mutex_unlock(ptt_mutex);
    }
}

 
  
/***********************************************
 * Set TX and RX frequency (100 Hz units)
 ***********************************************/
  
bool radio_setFreq(uint32_t txfreq, uint32_t rxfreq)
{
    if (txfreq > 0)
       _txfreq = txfreq;
    if (rxfreq > 0)
       _rxfreq = rxfreq;
    return _setGroupParm();
}
 
 
 
/***********************************************
 * Set squelch level (1-8)
 ***********************************************/

bool radio_setSquelch(uint8_t sq) 
{
    _squelch = sq;
    if (_squelch > 8)
       _squelch = 0; 
    return _setGroupParm();
}



/************************************************
 * Set receiver volume (1-8)
 ************************************************/

bool radio_setVolume(uint8_t vol)
{
    char buf[32];
    if (!_on)
        return true;
    char reply[16];
    if (vol > 8)
        vol = 8;
    int len = sprintf(buf, "AT+DMOSETVOLUME=%1d\r\n", vol);
    ESP_LOGD(TAG, "%s", buf);
    uart_write_bytes(_serial, buf, len);
    readline(_serial, reply, 16);
    return (reply[13] == '0');
}



/************************************************
 * Set mic sensitivity (1-8)
 ************************************************/

bool radio_setMicLevel(uint8_t level)
{
    char buf[32];
    if (!_on)
        return true;
    if (level > 8)
        level = 8;
    char reply[16];
    int len = sprintf(buf, "AT+DMOSETMIC=%1d,0\r\n", level);
    ESP_LOGD(TAG, "%s", buf);
    uart_write_bytes(_serial, buf, len);
    readline(_serial, reply, 16);
    return (reply[13] == '0');
}



/*************************************************
 * If on=true, TX power is set to 0.5W. 
 * else it is set to 1W
 *************************************************/

bool radio_setLowTxPower(bool on)
{
    if (on)
        _flags ^= FLAG_LO_POWER;
    else
        _flags |= FLAG_LO_POWER;
    return _setGroupParm();
}



bool radio_isLowTxPower() {
    return !(_flags & FLAG_LO_POWER);
}



/************************************************
 * Auto powersave on/off. 
 ************************************************/

bool radio_setPowerSave(bool on)
{
    char buf[32];
    if (!_on)
        return true;
    char reply[16];
    int len = printf(buf, "AT+DMOAUTOPOWCONTR=%1d\r\n", (on ? 1:0));
    ESP_LOGD(TAG, "%s", buf);
    uart_write_bytes(_serial, buf, len);
    readline(_serial, reply, 16);
    return (reply[13] == '0');
}



static bool _handshake()
{
    char buf[32];
    char reply[16];
    int len = sprintf(buf, "AT+DMOCONNECT\r\n");
    ESP_LOGD(TAG, "%s", buf);
    uart_write_bytes(_serial, buf, len);
    readline(_serial, reply, 16);
    return (reply[14] == '0');
}



/***************************************************
 * Set a group of parameters 
 ***************************************************/

static bool _setGroupParm()
{
    char buf[32];
    if (!_on)
        return true;
    char txbuf[16], rxbuf[16], reply[16];
    sprintf(txbuf, "%u.%04u", _txfreq/10000, _txfreq%10000);
    sprintf(rxbuf, "%u.%04u", _rxfreq/10000, _rxfreq%10000);

    int len = sprintf(buf, "AT+DMOSETGROUP=%1d,%s,%s,00,%1d,00,%1d\r\n",
            _widebw, txbuf, rxbuf, _squelch, _flags);
    ESP_LOGD(TAG, "%s", buf);
    uart_write_bytes(_serial, buf, len);
    readline(_serial, reply, 16);
    return (reply[15] == '0');
}
