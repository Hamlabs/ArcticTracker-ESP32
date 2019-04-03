/*
 * Common definitions/configuration of the firmware
 * By LA7ECA, ohanssen@acm.org
 */

#ifndef _DEFINES_H_
#define _DEFINES_H_


#define VERSION_STRING "V0.1 alpha"

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
#define FEET2M 3.2898


/* Queues for AFSK encoder/decoder */
#define AFSK_RX_QUEUE_SIZE      128
#define AFSK_TX_QUEUE_SIZE      128
#define HDLC_DECODER_QUEUE_SIZE  16
#define HDLC_ENCODER_QUEUE_SIZE  16

/* Radio */
#define RADIO_PIN_PTT       17
#define RADIO_PIN_TXP        5
#define RADIO_PIN_PD        18
#define RADIO_PIN_SQUELCH   16

/* GPS */
#define GPS_UART        UART_NUM_1
#define GPS_TXD_PIN     26
#define GPS_RXD_PIN     35


/* SPI and Display */
#define LCD_PIN_CS      33
#define LCD_PIN_BL      32
#define LCD_PIN_DC      27
#define LCD_PIN_RST     .1
#define SPI_PIN_MISO    -1
#define SPI_PIN_MOSI    14 
#define SPI_PIN_CLK     12

/* Buzzer */
#define BUZZER_PIN      21
#define BUZZER_TIMERGRP  0
#define BUZZER_TIMERIDX  0

#define LED_STATUS_PIN  22
#define BUTTON_PIN      16

/* Tone generation (for AFSK) */
#define TONE_DAC DAC_CHANNEL_1
#define TONE_TIMERGRP 0
#define TONE_TIMERIDX 1
#define AFSK_MARK 1200
#define AFSK_SPACE 2200


#define BLINK_NORMAL ; // FIXME
#define BLINK_GPS_SEARCHING ; // FIXME

#define HTTPD_DEFAULT_USR "arctic"
#define HTTPD_DEFAULT_PWD "hacker"
#define AP_DEFAULT_PASSWD ""
#define AP_DEFAULT_IP     "192.168.0.1"
#define AP_MAX_CLIENTS    4

#define AUTOCONNECT_PERIOD 240

/* Stack sizes for tasks */
#define STACK_AUTOCON        2800
#define STACK_HDLC_TEST      1000
#define STACK_HDLC_TXENCODER 2000
#define STACK_NMEALISTENER   1600
#define STACK_LEDBLINKER     1500
#define STACK_UI_SRV         800
#define STACK_TRACKER        2000


#define BBUF_SIZE 3000

#define FBUF_SLOTSIZE 32
#define FBUF_SLOTS 1024

/* Regular expressions defining various input formats */
#define REGEX_AXADDR   "\\w{3,6}(-\\d{1,2})?"
#define REGEX_DIGIPATH "\\w{3,6}(-\\d{1,2})?(\\,\\w{3,6}(-\\d{1,2})?)*"
#define REGEX_IPADDR   "(\\d{1,3}\\.){3}\\d{1,3}"
#define REGEX_APRSSYM  "[0-9A-Z\\/\\\\&]."
#define REGEX_HOSTNAME "[0-9-a-zA-Z\\-\\_\\.]+"

#define NORMALPRIO 5


#define min(x,y) (x<y? x : y)
#define max(x,y) (x>y? x : y)


/* Simplified semaphore operations */
#define semaphore_t     SemaphoreHandle_t
#define sem_create(cnt) xSemaphoreCreateCounting(65000, cnt)
#define sem_delete(sem) vSemaphoreDelete(sem)
#define sem_up(x)       xSemaphoreGive(x)
#define sem_down(x)     xSemaphoreTake(x, portMAX_DELAY)
#define sem_getCount(x) uxSemaphoreGetCount(x)

#define mutex_t SemaphoreHandle_t
#define mutex_lock(x)    xSemaphoreTake((x), portMAX_DELAY)
#define mutex_unlock(x)  xSemaphoreGive((x))


#define sleepMs(n)  vTaskDelay(pdMS_TO_TICKS(n))
#define t_yield     taskYIELD

/* Make event groups look like simpler condition variables */
#define cond_t               EventGroupHandle_t
#define cond_create          xEventGroupCreate
#define cond_wait(cond)      xEventGroupWaitBits(cond, BIT_0, pdFALSE, pdFALSE,  portMAX_DELAY)
#define cond_notify(cond)    xEventGroupSetBits(cond, BIT_0)
#define cond_clear(cond)     xEventGroupClearBits(cond, BIT_0)

#define cond_waitB(cond, bits)       xEventGroupWaitBits(cond, bits, pdFALSE, pdFALSE,  portMAX_DELAY)
#define cond_notifyB(cond, bits)     xEventGroupSetBits(cond, bits)
#define cond_notifyB_isr(cond, bits) xEventGroupSetBitsFromIsr(cond, bits)
#define cond_test(cond, bits)        (xEventGroupGetBits(cond) & bits)
#define cond_clearB(cond, bits)      xEventGroupClearBits(cond, bits)
#endif
