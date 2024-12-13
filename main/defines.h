/*
 * Common definitions/configuration of the firmware
 * By LA7ECA, ohanssen@acm.org
 */

#ifndef _DEFINES_H_
#define _DEFINES_H_

#include "sdkconfig.h"


/* Version of software */
#define VERSION_SSTRING "4.0"
#define VERSION_STRING  "v4.0"


#define FW_NAME "Arctic Tracker"
#define FW_DATE __DATE__

#define BIT_0	( 1 << 0 )


/* Supported devices */
#define ARCTIC3  0
#define T_TWR    1
#define ARCTIC4  2


/* 
 * Device - use what is selected by menuconfig 
 * See Kconfig.projbuild 
 */

#if defined CONFIG_T_TWR
  #define DEVICE T_TWR
  #define T_TWR_VER 20
  #define DEVICE_STRING "LilyGo T-TWR-plus"

#elif defined CONFIG_T_TWR_21
  #define DEVICE T_TWR
  #define T_TWR_VER 21
  #define DEVICE_STRING "LilyGo T-TWR-plus 2.1"

#elif defined CONFIG_ARCTIC3
  #define DEVICE_STRING "Arctic Tracker 3"
  #define DEVICE ARCTIC3

#elif defined CONFIG_ARCTIC4
  #define DEVICE ARCTIC4
  #define DEVICE_STRING "Arctic Tracker 4 VHF"
#elif defined CONFIG_ARCTIC4_UHF
  #define DEVICE ARCTIC4
  #define ARCTIC4_UHF
  #define DEVICE_STRING "Arctic Tracker 4 UHF/L"
#endif

#if DEVICE == T_TWR || DEVICE == ARCTIC4
  #define USE_PMU 1
#endif



/* Webserver settings */
#define WEBSERVER_HTTPS 
#define HTTP_PORT     80
#define HTTPS_PORT    443

/* Default WIFI SOFTAP settings */
#define AP_MAX_CLIENTS    4

/* Time between connect attempts */
#define AUTOCONNECT_PERIOD 240


/* 
 * If set to true, radio will be turned off even if tracking is active
 * and will be turned on only when sending packets. Otherwise it will
 * be on as long as tracking is on. 
 */
#define TRACKER_TRX_ONDEMAND false


/* APRS tracking */
#define TRACKER_SLEEP_TIME 10
#define TIMER_RESOLUTION   1000 
#define GPS_FIX_TIME       3
#define COMMENT_PERIOD     4
#define GPS_TIMEOUT        3 


/* Conversions */
#define KNOTS2KMH 1.853
#define KNOTS2MPS 0.5148
#define FEET2M    3.2898


/* FIXME: Need to find or specify the clock frequency !!! 
 */ 
#define TIMER_BASE_CLK 80000000
 


/* Queues for AFSK encoder/decoder */
#define AFSK_RX_QUEUE_SIZE        8
#define AFSK_TX_QUEUE_SIZE      256
#define HDLC_DECODER_QUEUE_SIZE  16
#define HDLC_ENCODER_QUEUE_SIZE  16
#define LORA_ENCODER_QUEUE_SIZE  16


/***************************************************
 * Radio module 
 ***************************************************/

#if !defined(RADIO_DISABLE)

#define RADIO_BUF_SIZE      256

#if DEVICE == T_TWR
/****************************************
 *  T-TWR device
 ****************************************/
#define RADIO_UART          UART_NUM_0
#define RADIO_PIN_TXD       39 
#define RADIO_PIN_RXD       48
#define RADIO_PIN_PTT       41
#define RADIO_PIN_PD        40
#define RADIO_PIN_LOWPWR    38
#define RADIO_PIN_TXSEL     17 
#define RADIO_PIN_SQUELCH   15

/* Radio audio input */
#define RADIO_INPUT          1  

/* Tone generation (for AFSK) */
#define TONE_SDELTA_ENABLE
#define TONE_SDELTA_CHAN    SIGMADELTA_CHANNEL_0
#define TONE_SDELTA_PIN     18

#if T_TWR_VER == 21
#define RADIO_PIN_SQUELCH    2
#define RADIO_PIN_MICSEL    17
#endif


#elif defined ARCTIC4_UHF
/****************************************
 *  UHF version - LoRa
 ****************************************/
#define RADIO_INPUT     -1 // FIXME

#define SPI_HOST        SPI3_HOST
#define SPI_PIN_MOSI    11
#define SPI_PIN_MISO    13
#define SPI_PIN_CLK     12

#define LORA_PIN_CS     10 
#define LORA_PIN_RST    40
#define LORA_PIN_BUSY   41
#define LORA_PIN_DIO1   42 
#define LORA_PIN_DIO3   44 // Need change in MUX
#define RADIO_PIN_PWRON  9


#elif DEVICE == ARCTIC4
/***************************************
 *  VHF/APRS version - Use SA868
 ***************************************/
#define RADIO_UART          UART_NUM_0
#define RADIO_PIN_TXD       44 
#define RADIO_PIN_RXD       43
#define RADIO_PIN_PTT       42
#define RADIO_PIN_PD        41
#define RADIO_PIN_PWRON      9
#define RADIO_PIN_SQUELCH   40

/* Radio audio input */
#define RADIO_INPUT          1 

/* Tone generation (for AFSK) */
#define TONE_SDELTA_ENABLE
#define TONE_SDELTA_CHAN    SIGMADELTA_CHANNEL_0
#define TONE_SDELTA_PIN      2

#else
/***************************************
 *  Old Arctic 3
 ***************************************/
#define RADIO_UART          UART_NUM_0
#define RADIO_PIN_PTT       38
#define RADIO_PIN_TXP       40
#define RADIO_PIN_PD        39
#define RADIO_PIN_SQUELCH   13

/* Radio audio input */
#define RADIO_INPUT         ADC2_CHANNEL_3 // FIXME

/* Tone generation (for AFSK) */
#define TONE_SDELTA_ENABLE
#define TONE_SDELTA_CHAN SIGMADELTA_CHANNEL_0
#define TONE_SDELTA_PIN  48

#endif
#endif //  !defined(RADIO_DISABLE)



/*********************************************
 * ADC And battery charget test for Arctic
 *********************************************/ 

#if DEVICE != T_TWR && DEVICE != ARCTIC4

/* Misc. ADC inputs */
#define X1_ADC_INPUT        ADC1_CHANNEL_4 
#define BATT_ADC_INPUT      ADC2_CHANNEL_7 

#if DEVICE == ARCTIC3
/* Batt charger */
#define BATT_CHG_TEST     12
#endif
#endif



/***********************************************
 * GPS 
 ***********************************************/

#if DEVICE == T_TWR
#define GPS_UART        UART_NUM_1
#define GPS_TXD_PIN     6
#define GPS_RXD_PIN     5

#else /* The same for ARCTIC3 and ARCTIC4 */
#define GPS_UART        UART_NUM_1
#define GPS_TXD_PIN     17
#define GPS_RXD_PIN     18
#endif



/*******************************************************************
 * DISPLAY CONFIG: 
 * DISPLAY_TYPE is 1=SSD1306 (I2C) (support for 0=Nokia is removed)
 * -1 to disable
 * SSD1306_HEIGHT and SSD1306_WIDTH should be set according to the 
 * actual display used. Known alternatives are: 
 * 
 * 128x64  (0.96") is a very popular configuration
 * 128x32  (0.91") half height 
 * 72x40   (0.42") e.g. DM-OLED042-647 from DisplayModule 
 * 
 *******************************************************************/

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
#if DEVICE == T_TWR
#define DISP_SDA_PIN     8
#define DISP_SCL_PIN     9

#elif DEVICE == ARCTIC4
#define DISP_SDA_PIN    21
#define DISP_SCL_PIN    47

#else
#define DISP_SDA_PIN     9
#define DISP_SCL_PIN    10
#endif




/********************************************
 * LEDs and buttons 
 ********************************************/

#if DEVICE == T_TWR

#define NEOPIXEL_PIN    42
#define BUTTON_PIN       3
#define ENC_PUSH_PIN    21
#define ENC_A_PIN       47
#define ENC_B_PIN       46

#elif DEVICE == ARCTIC4
#define LED_STATUS_PIN  38
#define LED_TX_PIN      39
#define BUTTON_PIN       4 /* Same as X1_PIN */

#else
#define LED_STATUS_PIN  41
#define LED_TX_PIN      42
#define BUTTON_PIN       0
#endif


/*********************************************
 * Buzzer 
 *********************************************/

#if DEVICE == T_TWR
#define BUZZER_PIN      -1

#elif DEVICE == ARCTIC4
#define BUZZER_PIN       8

#else
#define BUZZER_PIN      45
#endif

#define BUZZER_TIMERGRP  0
#define BUZZER_TIMERIDX  0


/**********************************************
 * AFSK generation 
 **********************************************/

#define AFSK_MARK        1200
#define AFSK_SPACE       2200

/* Shared timer for AFSK RX and TX */
#define AFSK_TIMERGRP    1
#define AFSK_TIMERIDX    0

/* Timer for AFSK tone generation */
#define TONE_TIMERGRP    0
#define TONE_TIMERIDX    1


/**********************************************
 * Extra pins
 **********************************************/

#if DEVICE == ARCTIC4
#define X1_PIN 4
#define X2_PIN 5
#define X3_PIN 6
#define X4_PIN 7
#endif





/**********************************************
 * Tasks: 
 * Stack sizes and allocation to cores 
 **********************************************/

#define STACK_AUTOCON        4000
#define STACK_HDLC_TEST      1000
#define STACK_HDLC_TXENCODER 3100
#define STACK_HDLC_RXDECODER 3100
#define STACK_AFSK_RXDECODER 3800
#define STACK_NMEALISTENER   3600
#define STACK_LEDBLINKER     2800
#define STACK_UI_SRV         3800
#define STACK_TRACKER        3800
#define STACK_MONITOR        3200
#define STACK_GUI            3400
#define STACK_HLIST          1500
#define STACK_DIGI           3200
#define STACK_TCP_REC        3000
#define STACK_IGATE          4000
#define STACK_IGATE_RADIO    3700
#define STACK_TRACKLOG       3500
#define STACK_TRACKLOGPOST   4100
#define STACK_HTTPD          5500
#define STACK_LORA_RXDECODER 4100
#define STACK_LORA_TXENCODER 4100
    
#define CORE_AUTOCON        0
#define CORE_NMEALISTENER   1
#define CORE_TRACKER        1
#define CORE_LEDBLINKER     0
#define CORE_UI_SRV         0
#define CORE_GUI            1
#define CORE_AFSK_RXDECODER 0
#define CORE_HDLC_RXDECODER 1
#define CORE_HDLC_TXENCODER 1
#define CORE_HDLC_TEST      1
#define CORE_HLIST          0
#define CORE_DIGI           0
#define CORE_TCP_REC        1
#define CORE_IGATE          1
#define CORE_IGATE_RADIO    0
#define CORE_TRACKLOG       1
#define CORE_TRACKLOGPOST   1
#define CORE_LORA_RXDECODER 0
#define CORE_LORA_TXENCODER 1


/* Default task priority */
#define NORMALPRIO 5


/*********************************************
 *  Misc. 
 *********************************************/

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
#define REGEX_TIMEZONE ".*"

#define min(x,y) (x<y? x : y)
#define max(x,y) (x>y? x : y)

#endif

