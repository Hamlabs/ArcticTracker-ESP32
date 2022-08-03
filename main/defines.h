/*
 * Common definitions/configuration of the firmware
 * By LA7ECA, ohanssen@acm.org
 */

#ifndef _DEFINES_H_
#define _DEFINES_H_


// #define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE


#define VERSION_STRING "v3.0 alpha"
#define FW_NAME "Arctic esp32"
#define FW_DATE "2022-07-22"

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


#define MCU ESP32S3

/* Queues for AFSK encoder/decoder */
#define AFSK_RX_QUEUE_SIZE      256
#define AFSK_TX_QUEUE_SIZE      256
#define HDLC_DECODER_QUEUE_SIZE  16
#define HDLC_ENCODER_QUEUE_SIZE  16

/* Radio module */
//#define RADIO_DISABLE
//#define DAC_DISABLE
#if !defined(RADIO_DISABLE)

#define RADIO_UART          UART_NUM_0
// #define RADIO_PIN_TXD        2
// #define RADIO_PIN_RXD       15
#define RADIO_PIN_PTT       38
#define RADIO_PIN_TXP       40
#define RADIO_PIN_PD        39
#define RADIO_PIN_SQUELCH   13
#define RADIO_BUF_SIZE      256
#endif

/* Radio audio input */
#define RADIO_INPUT         ADC2_CHANNEL_3


/* Misc. ADC inputs */
#define X1_ADC_INPUT        ADC1_CHANNEL_4
#define BATT_ADC_INPUT      ADC2_CHANNEL_7

/* Batt charger */
#define BATT_CHG_TEST     12


/* GPS */
#define GPS_UART        UART_NUM_1
#define GPS_TXD_PIN     17
#define GPS_RXD_PIN     18


/* DISPLAY CONFIG: 
 * DISPLAY_TYPE is either 0=NOKIA (SPI) or 1=SSD1306 (I2C)
 * -1 to disable
 * SSD1306_HEIGHT and SSD1306_WIDTH should be set according to the 
 * actual display used. Known alternatives are: 
 * 
 * 128x64  (0.96") is a very popular configuration
 * 128x32  (0.91") half height (not supported yet)
 * 72x40   (0.42") e.g. DM-OLED042-647 from DisplayModule (not supported yet)
 * 
 */

#define DISPLAY_TYPE     1
#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT  64

/* 
 * Some displays have the SH1106 chip instead of SSD1306 but have
 * a resolution of 128x64 - mostly compatible with a little hack. 
 */
#define SH1106_HACK     1


/* These are for the Nokia display on SPI */
#define LCD_PIN_CS      33
#define LCD_PIN_BL      32
#define LCD_PIN_DC      27
#define LCD_PIN_RST     -1

/* These are for the SSD1306 display on I2C */
#define DISP_SDA_PIN     9
#define DISP_SCL_PIN    10


/* SPI setup */
#define SPI_HOST        SPI3_HOST
#define SPI_PIN_MISO    -1
#define SPI_PIN_MOSI    14 
#define SPI_PIN_CLK     12

/* LEDs and button */
#define LED_STATUS_PIN  41
#define LED_TX_PIN      42
#define BUTTON_PIN       0

/* Buzzer */
#define BUZZER_PIN      45
#define BUZZER_TIMERGRP  0
#define BUZZER_TIMERIDX  0

/* Tone generation (for AFSK) */
#define TONE_SDELTA_ENABLE
// #define TONE_DAC_ENABLE

#define TONE_DAC_CHAN    DAC_CHANNEL_1
#define TONE_SDELTA_CHAN SIGMADELTA_CHANNEL_0
#define TONE_SDELTA_PIN  48

#define AFSK_MARK        1200
#define AFSK_SPACE       2200

/* Shared timer for AFSK RX and TX */
#define AFSK_TIMERGRP    1
#define AFSK_TIMERIDX    0

/* Timer for AFSK tone generation */
#define TONE_TIMERGRP    0
#define TONE_TIMERIDX    1


#define HTTPD_DEFAULT_USR "arctic"
#define HTTPD_DEFAULT_PWD "hacker"
#define AP_DEFAULT_PASSWD ""
#define AP_DEFAULT_IP     "192.168.0.1"
#define AP_MAX_CLIENTS    4

#define AUTOCONNECT_PERIOD 240

/* Stack sizes for tasks */
#define STACK_AUTOCON        3000
#define STACK_HDLC_TEST      1000
#define STACK_HDLC_TXENCODER 3100
#define STACK_HDLC_RXDECODER 3100
#define STACK_AFSK_RXDECODER 3000
#define STACK_NMEALISTENER   3600
#define STACK_LEDBLINKER     1300
#define STACK_UI_SRV         3500
#define STACK_TRACKER        3500
#define STACK_MONITOR        3000
#define STACK_GUI            3000
#define STACK_HLIST           900
#define STACK_DIGI           3200
#define STACK_TCP_REC        3000
#define STACK_IGATE          3000
#define STACK_IGATE_RADIO    2300
#define STACK_TRACKLOG       3100
#define STACK_TRACKLOGPOST   3900

    
#define CORE_AUTOCON        0
#define CORE_NMEALISTENER   1
#define CORE_TRACKER        1
#define CORE_LEDBLINKER     0
#define CORE_UI_SRV         0
#define CORE_GUI            1
#define CORE_AFSK_RXDECODER 0
#define CORE_HDLC_RXDECODER 0
#define CORE_HDLC_TXENCODER 1
#define CORE_HDLC_TEST      1
#define CORE_HLIST          0
#define CORE_DIGI           0
#define CORE_TCP_REC        1
#define CORE_IGATE          1
#define CORE_IGATE_RADIO    0
#define CORE_TRACKLOG       1
#define CORE_TRACKLOGPOST   1

// FIXME: If ledblinker is run on CPU 1, system will crash! 
// FIXME: Seems to be an issue with beep if it runs on CPU 1 ???


#define BBUF_SIZE 4096

#define FBUF_SLOTSIZE   32
#define FBUF_SLOTS    2048

/* Regular expressions defining various input formats */
#define REGEX_AXADDR   "\\w{3,6}(-\\d{1,2})?"
#define REGEX_DIGIPATH "\\w{3,6}(-\\d{1,2})?(\\,\\w{3,6}(-\\d{1,2})?)*"
#define REGEX_IPADDR   "(\\d{1,3}\\.){3}\\d{1,3}"
#define REGEX_APRSSYM  "[0-9A-Z\\/\\\\&]."
#define REGEX_HOSTNAME "[0-9a-zA-Z\\-\\_\\.]+"
#define REGEX_URL      "http(s?):\\/\\/[0-9a-zA-Z\\-\\_\\.\\/]+"
#define REGEX_FPATH    "[0-9a-zA-Z\\-\\_\\.\\/]+"

#define NORMALPRIO 5



#define min(x,y) (x<y? x : y)
#define max(x,y) (x>y? x : y)


#endif
