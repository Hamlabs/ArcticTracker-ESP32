#include "defines.h"


#if !defined(RADIO_DISABLE) && (DEVICE == T_TWR || DEVICE == ARCTIC4)
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
static int32_t  _txfreq;       // TX frequency in 100 Hz units
static int32_t  _rxfreq;       // RX frequency in 100 Hz units
static uint8_t  _squelch;      // Squelch level (0-8 where 0 is open)

static char*    _tcxcss = "0000"; 
static char*    _rcxcss = "0000";

static int count = 0; 
  
static bool _handshake(void);
static bool _setGroupParm(void);
static bool _getVersion(void);
static void _initialize(void);
static void _squelch_handler(void* arg);
 
 bool sa8_is_on(void);
 void sa8_require(void);
 void sa8_release(void);
 void sa8_wait_enabled(void);
 void sa8_init(uart_port_t uart);
 
 
 bool sa8_setFreq(uint32_t txfreq, uint32_t rxfreq);
 bool sa8_setSquelch(uint8_t sq);
 bool sa8_getSquelch();
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
 bool sa8_setTail(int tail);
 
 
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


  
static cond_t radio_rdy;
#define WAIT_RADIO_READY   cond_wait(radio_rdy)
#define SIGNAL_RADIO_READY cond_set(radio_rdy)
#define CLEAR_RADIO_READY  cond_clear(radio_rdy)

static cond_t chan_rdy;
#define WAIT_CHANNEL_READY cond_wait(chan_rdy)
 
static cond_t tx_off;
#define WAIT_TX_OFF  cond_wait(tx_off)


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

    gpio_set_direction(RADIO_PIN_PTT,     GPIO_MODE_OUTPUT); 
    gpio_set_direction(RADIO_PIN_PD,      GPIO_MODE_OUTPUT);
    gpio_set_direction(RADIO_PIN_SQUELCH, GPIO_MODE_INPUT);
    gpio_set_level(RADIO_PIN_PTT, 1);
    gpio_set_level(RADIO_PIN_PD, 0);
    
#if DEVICE == T_TWR
    /* On T_TWR, route audio from ESP32 to mic-input on radio module */
    gpio_set_direction(RADIO_PIN_TXSEL, GPIO_MODE_OUTPUT); 
    gpio_set_level(RADIO_PIN_TXSEL, 1);
    gpio_set_direction(RADIO_PIN_LOWPWR, GPIO_MODE_OUTPUT);

#elif DEVICE == ARCTIC4
    gpio_set_direction(RADIO_PIN_PWRON, GPIO_MODE_OUTPUT);
    gpio_set_level(RADIO_PIN_PWRON, 1);
#endif
    
    radio_mutex = mutex_create();
    ptt_mutex = mutex_create();
    tx_off = cond_create();
    radio_rdy = cond_create();
    chan_rdy = cond_create();
//    sa8_setLowTxPower(true);
    
    
    /* Squelch input. Pin interrupt */
    gpio_set_intr_type(RADIO_PIN_SQUELCH, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(RADIO_PIN_SQUELCH, _squelch_handler, NULL);
    gpio_set_pull_mode(RADIO_PIN_SQUELCH, GPIO_PULLUP_ONLY);
    gpio_intr_enable(RADIO_PIN_SQUELCH);  
    
    sleepMs(500);
    if (get_byte_param("RADIO.on", 0)>0)
        sa8_require();
}
  

 
static void _initialize()
{  
    sleepMs(500);
    _handshake();
    sleepMs(50);
  
    _getVersion();
    
    /* Get parameters from NVS flash */
    _txfreq = get_i32_param("TXFREQ", DFL_TXFREQ);
    _rxfreq = get_i32_param("RXFREQ", DFL_RXFREQ);
    _squelch = get_byte_param("TRX_SQUELCH", DFL_TRX_SQUELCH);
    
    ESP_LOGI(TAG, "_initialize: %s, txfreq=%ld, rxfreq=%ld", 
        (pmu_dc3_isOn() ? "ON":"OFF"), _txfreq, _rxfreq);
    
    
    sa8_setFilter(true, false, false);
    sa8_setVolume(6);
    sa8_setTail(0);
    _setGroupParm();
    cond_set(tx_off);
   
    if (gpio_get_level(RADIO_PIN_SQUELCH))
        cond_set(chan_rdy);
    else
        cond_clear(chan_rdy);
    
    sleepMs(50);
    ESP_LOGI(TAG, "_initialize: ok");
}
  
  
  
/************************************************
 * Squelch handler (ISR function)
 ************************************************/

static void IRAM_ATTR _squelch_handler(void* arg) 
{
    if (!_sq_on && cond_isSetI(radio_rdy) && !gpio_get_level(RADIO_PIN_SQUELCH)) {
        _sq_on = true;
        afsk_rx_start();
        cond_clearI(chan_rdy);       
    }
    else if (_sq_on) {
        _sq_on = false;
        afsk_rx_stop();
        cond_setI(chan_rdy);
    } 
}



/******************************************************
 * Wait for reply from radio module
 ******************************************************/

static void waitReply(char* reply) {
    reply[0] = '\0';
    while (reply[0] == '\0') {
        sleepMs(10);
        readline(_serial, reply, 15);
    }
    ESP_LOGD(TAG, "Reply: %s", reply);
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
    WAIT_CHANNEL_READY; 
}


/************************************************
 * Wait until tx is off
 ************************************************/
void sa8_wait_tx_off()
{
    WAIT_TX_OFF;
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
        cond_clear(tx_off);
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
        cond_clearI(tx_off);
    }
    else {
        gpio_set_level(RADIO_PIN_PTT, 1);
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
       _squelch = 8; 
    return _setGroupParm();
}



/***********************************************
 * Return true if squelch is open
 ***********************************************/

bool sa8_getSquelch() 
{
    return !gpio_get_level(RADIO_PIN_SQUELCH);
}



/***********************************************
 * Get RSSI level
 ***********************************************/

int sa8_getRSSI()
{
    char buf[32];
    if (!_on)
        return 0;
    
    char reply[16];
    int rssi;
    int len = sprintf(buf, "AT+RSSI?\r\n");
    uart_write_bytes(_serial, buf, len);
    waitReply(reply);
    sscanf(reply, "RSSI=%d", &rssi);
    return rssi;
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
    waitReply(reply);
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
#if (DEVICE==T_TWR)
    gpio_set_level(RADIO_PIN_LOWPWR, (on? 0 : 1) );
    return true;
#elif (DEVICE==ARCTIC4)
    return _setGroupParm();
#endif
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
    int len = sprintf(buf, "AT+SETFILTER=%1d,%1d,%1d\r\n", 
        (emp ? 1:0), (highpass ? 1:0), (lowpass ? 1:0)  );
    ESP_LOGD(TAG, "Request: %s", buf);
    uart_write_bytes(_serial, buf, len);
    waitReply(reply);
    return (reply[14] == '0');
}



/************************************************
 * Set tail 
 ************************************************/

bool sa8_setTail(int tail)
{
    char buf[32];
    if (!_on)
        return true;
    char reply[16];
    int len = sprintf(buf, "AT+SETTAIL=%1d\r\n", tail);
    ESP_LOGD(TAG, "Request: %s", buf);
    uart_write_bytes(_serial, buf, len);
    waitReply(reply);
    return (reply[14] == '0');
}



/****************************************************
 * Ask for module version (set log to debug to see) 
 ****************************************************/

static bool _getVersion()
{
    char buf[32];
    if (!_on)
        return true;
    char reply[16];
    int len = sprintf(buf, "AT+VERSION\r\n");
    ESP_LOGD(TAG, "Request: %s", buf);
    uart_write_bytes(_serial, buf, len);
    waitReply(reply);
    waitReply(reply);
    return (reply[14] == '0');
}



static bool _handshake()
{
    char buf[32];
    char reply[16];
    int len = sprintf(buf, "AT+DMOCONNECT\r\n");
    ESP_LOGD(TAG, "Request: %s", buf);
    uart_write_bytes(_serial, buf, len);
    waitReply(reply);
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

    int len = sprintf(buf, "AT+DMOSETGROUP=%1d,%s,%s,%s,%01d,%s\r\n",
            (_lowPower? 1 : 0), txbuf, rxbuf, _tcxcss, _squelch, _rcxcss);
    ESP_LOGD(TAG, "%s", buf);
    uart_write_bytes(_serial, buf, len);
    waitReply(reply);
    return (reply[13] == '0');
}


#endif
