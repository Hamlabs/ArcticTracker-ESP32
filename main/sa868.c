#include "defines.h"

#if !defined(RADIO_DISABLE) && DEVICE == T_TWR
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "hdlc.h"
#include "afsk.h"
#include "system.h"
#include "radio.h"
#include "ui.h"
#include "pmu.h"

#define TAG "radio"

#define rx_led_on(x) /* Placeholder */
#define rx_led_off(x) 


static bool     _on = false;
static bool     _sq_on = false;
static bool     _lowPower = false;
static uint8_t  _widebw; 
static int32_t  _txfreq;       // TX frequency in 100 Hz units
static int32_t  _rxfreq;       // RX frequency in 100 Hz units
static uint8_t  _squelch;      // Squelch level (0-8 where 0 is open)

static char*    _tcxcss = "0000"; 
static char*    _rcxcss = "0000";

static int count = 0; 
  
static bool _handshake(void);
static bool _setGroupParm(void);
static void _initialize(void);
 
 bool sa8_is_on(void);
 void sa8_require(void);
 void sa8_release(void);
 void sa8_wait_enabled(void);
 void sa8_init(uart_port_t uart);
 
 
 bool sa8_setFreq(uint32_t txfreq, uint32_t rxfreq);
 bool sa8_setSquelch(uint8_t sq);
 void sa8_on(bool on);
 void sa8_PTT(bool on);
 void sa8_PTT_I(bool on);
 bool sa8_setVolume(uint8_t vol);
 bool sa8_setMicLevel(uint8_t level);
 bool sa8_setPowerSave(bool on);
 bool sa8_setLowTxPower(bool on); 
 bool sa8_isLowTxPower(void); 
 void sa8_wait_channel_ready(void);
 bool sa8_setFilter(bool eemp, bool highpass, bool lowpass);
 
/*
 * Serial driver config
 */

static uart_config_t _serialConfig = {
    .baud_rate  = 9600,
    .data_bits  = UART_DATA_8_BITS,
    .parity     = UART_PARITY_DISABLE,
    .stop_bits  = UART_STOP_BITS_1,
    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT
};
static uart_port_t _serial;


static mutex_t radio_mutex;
static mutex_t ptt_mutex;

// Binary semaphore. Should it be a condition variable?? 
static cond_t tx_off;
  
static cond_t radio_rdy;
#define WAIT_RADIO_READY   cond_wait(radio_rdy)
#define SIGNAL_RADIO_READY cond_set(radio_rdy)
#define CLEAR_RADIO_READY  cond_clear(radio_rdy);

static cond_t chan_rdy;
#define WAIT_CHANNEL_READY cond_wait(chan_rdy)



/***********************************************
 * Initialize
 ***********************************************/

void sa8_init(uart_port_t uart)
{  
    _serial = uart; 
    ESP_LOGI(TAG, "sa8_init, uart=%d, pwr=%s", uart, (pmu_dc3_isOn() ? "ON":"OFF"));
    ESP_ERROR_CHECK(uart_param_config(uart, &_serialConfig));
    ESP_ERROR_CHECK(uart_driver_install(uart, RADIO_BUF_SIZE, RADIO_BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_set_pin(uart, RADIO_PIN_TXD, RADIO_PIN_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    gpio_set_direction(RADIO_PIN_PTT,   GPIO_MODE_OUTPUT); 
    gpio_set_direction(RADIO_PIN_PD,    GPIO_MODE_OUTPUT);
    gpio_set_direction(RADIO_PIN_PWR,   GPIO_MODE_OUTPUT);
    gpio_set_direction(RADIO_PIN_TXSEL, GPIO_MODE_OUTPUT);
    gpio_set_level(RADIO_PIN_PTT, 1);
    gpio_set_level(RADIO_PIN_TXSEL, 1);
        
    radio_mutex = mutex_create();
    ptt_mutex = mutex_create();
    tx_off = cond_create();
    radio_rdy = cond_create();
    chan_rdy = cond_create();
    sa8_setLowTxPower(true);
    
    sleepMs(500);
    if (get_byte_param("RADIO.on", 0)>0)
        sa8_require();
}
  
 
 
static void _initialize()
{  
    sleepMs(400);
    _handshake();
    sleepMs(50);
  
    /* Get parameters from NVS flash */
    _txfreq = get_i32_param("TXFREQ", DFL_TXFREQ);
    _rxfreq = get_i32_param("RXFREQ", DFL_RXFREQ);
    _squelch = get_byte_param("TRX_SQUELCH", DFL_TRX_SQUELCH);
    ESP_LOGI(TAG, "_initialize: %s, txfreq=%ld, rxfreq=%ld", 
        (pmu_dc3_isOn() ? "ON":"OFF"), _txfreq, _rxfreq);
    _widebw = 0;  /* Set to 1 for wide bandwidth */
    
    sa8_setFilter(false, false, false);
    sa8_setVolume(6);
    _setGroupParm();
    cond_set(tx_off);
   
    cond_clear(chan_rdy);
    
    sleepMs(50);
    ESP_LOGI(TAG, "_initialize: ok");
}
  
 
 

/**********************************************************
 * To be called when signal is detected and when
 * silence is detected. Excpected to be called 
 * from ISR. 
 **********************************************************/

void sa8_rx_signal(bool on)
{
    if (on) {
        if (!_sq_on && cond_isSetI(radio_rdy)) {
            _sq_on = true;
            afsk_rx_start();
            cond_clearI(chan_rdy);       
        }
    }
    else 
    {
        if (_sq_on) {
            _sq_on = false;
            afsk_rx_stop();
            cond_setI(chan_rdy);
        } 
    }
}



/******************************************************
 * Return true if radio is on
 ******************************************************/

bool sa8_is_on(void) {
    return (count >= 1); 
}


/******************************************************
 * Need radio - turn it on if not already on
 ******************************************************/
 
void sa8_require(void)
{
    mutex_lock(radio_mutex);
    if (++count == 1) {
        sa8_on(true);   
        afsk_tx_start();
        ESP_LOGI(TAG, "Radio is turned ON");
    }
    mutex_unlock(radio_mutex);
}


 
/*******************************************************
 * Radio not needed any more - turn it off if no others
 * need it
 *******************************************************/
 
void sa8_release(void)
{
    mutex_lock(radio_mutex);
    if (--count <= 0) {
       /* 
        * Before turning off transceiver, wait until
        * Packet is sent and transmitter is turned off. 
        */
       sleepMs(60);
       hdlc_wait_idle();
       cond_wait(tx_off);
       afsk_tx_stop();
       sa8_on(false);
       ESP_LOGI(TAG, "Radio is turned OFF");
    }
    if (count < 0) count = 0;
    mutex_unlock(radio_mutex);
}



/************************************************
 * Wait until radio is ready 
 ************************************************/

void sa8_wait_enabled() 
{
    WAIT_RADIO_READY;
}



/************************************************
 * Wait until channel is ready 
 ************************************************/
void sa8_wait_channel_ready()
{
    /* Wait to radio is on and squelch is closed */
//    WAIT_CHANNEL_READY; 
}



/************************************************
 * Power on
 ************************************************/

void sa8_on(bool on)
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

void sa8_PTT(bool on)
{
    if (!_on)
       return;
    
    ESP_LOGI(TAG, "radio_PTT: %s", (on? "true":"false"));
    if (on) {
        mutex_lock(ptt_mutex);
        gpio_set_level(RADIO_PIN_PTT, 0);
        tx_led_on();
        cond_set(tx_off);
        mutex_unlock(ptt_mutex);
    }
    else {
        mutex_lock(ptt_mutex);
        gpio_set_level(RADIO_PIN_PTT, 1);
        tx_led_off();
        cond_set(tx_off);
        mutex_unlock(ptt_mutex);
    }
}



void sa8_PTT_I(bool on)
{
    if (!_on)
       return;
    
    if (on) {
        gpio_set_level(RADIO_PIN_PTT, 0);
        tx_led_on(); 
        cond_clearI(tx_off);
    }
    else {
        gpio_set_level(RADIO_PIN_PTT, 1);
        tx_led_off();
        cond_setI(tx_off);
    }
}
 
  
/***********************************************
 * Set TX and RX frequency (100 Hz units)
 ***********************************************/
  
bool sa8_setFreq(uint32_t txfreq, uint32_t rxfreq)
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

bool sa8_setSquelch(uint8_t sq) 
{
    _squelch = sq;
    if (_squelch > 8)
       _squelch = 0; 
    return _setGroupParm();
}



/************************************************
 * Set receiver volume (1-8)
 ************************************************/

bool sa8_setVolume(uint8_t vol)
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
    return (reply[10] == '0');
}



/************************************************
 * Set mic sensitivity (1-8)
 ************************************************/

bool sa8_setMicLevel(uint8_t level)
{
    ESP_LOGW(TAG, "setMicLevel - command not supported");
    return false;
}



/*************************************************
 * If on=true, TX power is set to 0.5W. 
 * else it is set to 2W
 *************************************************/

bool sa8_setLowTxPower(bool on)
{
    ESP_LOGD(TAG, "SetLowTxPower: %s", (on ? "on" : "off"));
    _lowPower = on;
    gpio_set_level(RADIO_PIN_PWR, (on? 0 : 1) );
    return false; 
}



bool sa8_isLowTxPower() {
    return _lowPower; 
}



/************************************************
 * Auto powersave on/off. 
 ************************************************/

bool sa8_setPowerSave(bool on)
{
    ESP_LOGW(TAG, "setPowerSave - command not supported");
    return false; 
}


/************************************************
 * Filter: pre/de emphasis, highpass, lowpass 
 ************************************************/

bool sa8_setFilter(bool emp, bool highpass, bool lowpass)
{
    char buf[32];
    if (!_on)
        return true;
    char reply[16];
    int len = sprintf(buf, "AT+DMOSETFILTER=%1d,%1d,%1d\r\n", 
        (emp ? 1:0), (highpass ? 1:0), (lowpass ? 1:0)  );
    ESP_LOGD(TAG, "%s", buf);
    uart_write_bytes(_serial, buf, len);
    readline(_serial, reply, 16);
    return (reply[14] == '0');
}



static bool _handshake()
{
    char buf[32];
    char reply[16];
    int len = sprintf(buf, "AT+DMOCONNECT\r\n");
    ESP_LOGD(TAG, "%s", buf);
    uart_write_bytes(_serial, buf, len);
    readline(_serial, reply+1, 16);
    ESP_LOGD(TAG, "'%s'", reply);
    return (reply[12] == '0');
}



/***************************************************
 * Set a group of parameters 
 ***************************************************/

static bool _setGroupParm()
{
    char buf[67];
    if (!_on)
        return true;
    char txbuf[16], rxbuf[16], reply[16];
    sprintf(txbuf, "%lu.%04lu", _txfreq/10000, _txfreq%10000);
    sprintf(rxbuf, "%lu.%04lu", _rxfreq/10000, _rxfreq%10000);

    int len = sprintf(buf, "AT+DMOSETGROUP=%1d,%s,%s,%s,%1d,%s\r\n",
            _widebw, txbuf, rxbuf, _tcxcss, _squelch, _rcxcss);
    ESP_LOGD(TAG, "%s", buf);
    uart_write_bytes(_serial, buf, len);
    readline(_serial, reply, 16);
    ESP_LOGD(TAG, "'%s'", reply);
    return (reply[13] == '0');
}


#endif
