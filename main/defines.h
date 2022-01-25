/*
 * Common definitions/configuration of the firmware
 * By LA7ECA, ohanssen@acm.org
 */

#ifndef _DEFINES_H_
#define _DEFINES_H_


// #define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE


#define VERSION_STRING "v2.0 alpha4"
#define FW_NAME "Arctic esp32"
#define FW_DATE "2021-01-22"

#define BIT_0	( 1 << 0 )

/* 
 * If set to true, radio will be turned off even if tracking is active
 * and will be turned on only when sending packets. Otherwise it will
 * be on as long as tracking is on. 
 */
#define TRACKER_TRX_ONDEMAND false


/* APRS tracking FIXME */
#define TRACKER_SLEEP_TIME 10
#define TIMER_RESOLUTION   1000 
#define GPS_FIX_TIME       3
#define COMMENT_PERIOD     4
#define GPS_TIMEOUT        3 


/* Conversions */
#define KNOTS2KMH 1.853
#define KNOTS2MPS 0.5148
#define FEET2M    3.2898


/* Queues for AFSK encoder/decoder */
#define AFSK_RX_QUEUE_SIZE      128
#define AFSK_TX_QUEUE_SIZE      128
#define HDLC_DECODER_QUEUE_SIZE  16
#define HDLC_ENCODER_QUEUE_SIZE  16

/* Radio */
#define RADIO_UART          UART_NUM_2
#define RADIO_PIN_TXD        2
#define RADIO_PIN_RXD       15
#define RADIO_PIN_PTT       17
#define RADIO_PIN_TXP        5
#define RADIO_PIN_PD        18
#define RADIO_PIN_SQUELCH   16
#define RADIO_BUF_SIZE      256
#define RADIO_INPUT         ADC1_CHANNEL_0

/* Misc. ADC inputs */
#define X1_ADC_INPUT        ADC1_CHANNEL_6
#define BATT_ADC_INPUT      ADC1_CHANNEL_3

/* GPS */
#define GPS_UART        UART_NUM_1
#define GPS_TXD_PIN     26
#define GPS_RXD_PIN     35


/* SPI and Display */
#define LCD_PIN_CS      33
#define LCD_PIN_BL      32
#define LCD_PIN_DC      27
#define LCD_PIN_RST     -1
#define SPI_PIN_MISO    -1
#define SPI_PIN_MOSI    14 
#define SPI_PIN_CLK     12


#define LED_STATUS_PIN  23
#define LED_TX_PIN      22
#define BUTTON_PIN      13

/* Buzzer */
#define BUZZER_PIN      21
#define BUZZER_TIMERGRP  0
#define BUZZER_TIMERIDX  0

/* Tone generation (for AFSK) */
#define TONE_DAC        DAC_CHANNEL_1
#define AFSK_MARK       1200
#define AFSK_SPACE      2200

/* Shared timer for AFSK RX and TX */
#define AFSK_TIMERGRP   1
#define AFSK_TIMERIDX   0

/* Timer for AFSK tone generation */
#define TONE_TIMERGRP   0
#define TONE_TIMERIDX   1


#define HTTPD_DEFAULT_USR "arctic"
#define HTTPD_DEFAULT_PWD "hacker"
#define AP_DEFAULT_PASSWD ""
#define AP_DEFAULT_IP     "192.168.0.1"
#define AP_MAX_CLIENTS    4

#define AUTOCONNECT_PERIOD 240

/* Stack sizes for tasks */
#define STACK_AUTOCON        3000
#define STACK_HDLC_TEST      1000
#define STACK_HDLC_TXENCODER 2200
#define STACK_HDLC_RXDECODER 2200
#define STACK_NMEALISTENER   2900
#define STACK_LEDBLINKER     1100
#define STACK_UI_SRV         3600
#define STACK_TRACKER        3700
#define STACK_MONITOR        2700
#define STACK_GUI            2600
#define STACK_HLIST           900
#define STACK_DIGI           2800
#define STACK_TCP_REC        3000
#define STACK_IGATE          3000
#define STACK_IGATE_RADIO    2300
#define STACK_TRACKLOG       2700

    
#define CORE_AUTOCON        0
#define CORE_NMEALISTENER   1
#define CORE_TRACKER        1
#define CORE_LEDBLINKER     0
#define CORE_UI_SRV         0
#define CORE_GUI            1
#define CORE_HDLC_RXDECODER 0
#define CORE_HDLC_TXENCODER 1
#define CORE_HDLC_TEST      1
#define CORE_HLIST          0
#define CORE_DIGI           0
#define CORE_TCP_REC        1
#define CORE_IGATE          1
#define CORE_IGATE_RADIO    0
#define CORE_TRACKLOG       1



// FIXME: If ledblinker is run on CPU 1, system will crash! 
// FIXME: Seems to be an issue with beep if it runs on CPU 1 ???

#define BBUF_SIZE 4096

#define FBUF_SLOTSIZE 32
#define FBUF_SLOTS 512

/* Regular expressions defining various input formats */
#define REGEX_AXADDR   "\\w{3,6}(-\\d{1,2})?"
#define REGEX_DIGIPATH "\\w{3,6}(-\\d{1,2})?(\\,\\w{3,6}(-\\d{1,2})?)*"
#define REGEX_IPADDR   "(\\d{1,3}\\.){3}\\d{1,3}"
#define REGEX_APRSSYM  "[0-9A-Z\\/\\\\&]."
#define REGEX_HOSTNAME "[0-9a-zA-Z\\-\\_\\.]+"
#define REGEX_FPATH    "[0-9a-zA-Z\\-\\_\\.\\/]+"

#define NORMALPRIO 5



#define min(x,y) (x<y? x : y)
#define max(x,y) (x>y? x : y)


#endif
