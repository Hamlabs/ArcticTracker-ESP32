/* 
 * Routines for controling a Nokia LCD display, 
 * using the SPI bus. 
 * By LA7ECA, ohanssen@acm.org
 */ 

#include <inttypes.h>
#include <string.h>
#include "defines.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h" 
#include "lcd.h"


static spi_device_handle_t _spip;
static TimerHandle_t bltimer;
static void blhandler(TimerHandle_t timer);


/************************************************************
 * SPI initialization
 ************************************************************/

static void spi_init() {   
    esp_err_t ret;
    
    /* Setup for SPI bus */
    spi_bus_config_t buscfg={
        .miso_io_num=SPI_PIN_MISO,
        .mosi_io_num=SPI_PIN_MOSI,
        .sclk_io_num=SPI_PIN_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=0
    };    
    
    /* Setup for LCD device on SPI bus */
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=10*1000*1000,     
        .mode=0,                       
        .spics_io_num=LCD_PIN_CS,  
        .queue_size=7,                         
    };
    
    //Initialize the SPI bus
    ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
    ESP_ERROR_CHECK(ret);
    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(HSPI_HOST, &devcfg, &_spip);
    ESP_ERROR_CHECK(ret);
}



/************************************************************
 * LCD display initialization
 ************************************************************/

void lcd_init() {
    spi_init();
    
    /* GPIO direction */
    gpio_set_direction(LCD_PIN_DC,  GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_PIN_BL,  GPIO_MODE_OUTPUT);
    
    /* Reset LCD */
    gpio_set_level(LCD_PIN_BL, 1);
    gpio_set_level(LCD_PIN_RST, 0);
    sleepMs(15);
    gpio_set_level(LCD_PIN_RST, 1);
    sleepMs(15);
   
    /* Send configuration commands to LCD */
    lcd_writeByte(0x21, LCD_SEND_CMD);  /* LCD extended commands */
    lcd_writeByte(0xC8, LCD_SEND_CMD);  /* Set LCD Vop (Contrast) */
    lcd_writeByte(0x05, LCD_SEND_CMD);  /* Set start line S6 to 1 TLS8204 */
    lcd_writeByte(0x40, LCD_SEND_CMD);  /* Set start line S[5:0] to 0x00 TLS8204 */
    lcd_writeByte(0x12, LCD_SEND_CMD);  /* LCD bias mode 1:68. */
    lcd_writeByte(0x20, LCD_SEND_CMD);  /* LCD standard Commands, horizontal addressing mode. */
    lcd_writeByte(0x08, LCD_SEND_CMD);  /* LCD blank */
    lcd_writeByte(0x0C, LCD_SEND_CMD);  /* LCD in normal mode. */
    sleepMs(15);
   
    lcd_clear(); /* Clear LCD */
    lcd_setPosXY(1, 1);
   
    /* Software timer for backlight */
    bltimer = xTimerCreate ( 
        "BacklightTimer", pdMS_TO_TICKS(8000),  pdFALSE,
        ( void * ) 0, blhandler
    );
}
 
 
 
 
/************************************************************
 * Turn on backlight and turn it off again after 10 seconds
 ************************************************************/

static void blhandler(TimerHandle_t timer) {
    gpio_set_level(LCD_PIN_BL, 1);
}
 
 
void lcd_backlight() { 
    xTimerStart(bltimer, 0);
    gpio_set_level(LCD_PIN_BL, 0);
}    

 
  
 
/************************************************************
 * Send a single byte to the display. If cd is set, 
 * it is a command. 
 ************************************************************/
 
 void lcd_writeByte(uint8_t data, uint8_t cd) {
   if(cd == LCD_SEND_DATA) 
       gpio_set_level(LCD_PIN_DC, 1);
   else 
       gpio_set_level(LCD_PIN_DC, 0);
   
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));      
    t.length=8;                    
    t.tx_buffer=&data;    
    t.user=(void*)1;  // FIXME: How does this work???? 
    ret=spi_device_polling_transmit(_spip, &t);
    assert(ret==ESP_OK);
 }
 
 
 
 

/************************************************************
 * Clear the display 
 ************************************************************/

void lcd_clear() { 
   uint32_t i, j;
   
   for (i = 0; i < LCD_Y_RES/LCD_FONT_Y_SIZE; i++) {
     lcd_setPosXY(0, i);
     for (j = 0; j < LCD_X_RES; j++)
       lcd_writeByte(0x00, LCD_SEND_DATA);
   }
}
 

 
/************************************************************
 * Set the position on display for writing the next character. 
 ************************************************************/
 
 void lcd_setPosXY(uint8_t x, uint8_t y) {
   
   if (y > LCD_Y_RES/LCD_FONT_Y_SIZE) return;
   if (x > LCD_X_RES) return;
   
   lcd_writeByte(0x80 | x, LCD_SEND_CMD);   /* Set x position */
   lcd_writeByte(0x40 | y, LCD_SEND_CMD);   /* Set y position */  
 }
 
 

 
/************************************************************
 * Adjust the contrast 
 ************************************************************/
 
 void lcd_contrast (uint8_t contrast) {
   
   lcd_writeByte(0x21, LCD_SEND_CMD);              /* LCD Extended Commands */
   lcd_writeByte(0x80 | contrast, LCD_SEND_CMD);   /* Set LCD Vop (Contrast) */
   lcd_writeByte(0x20, LCD_SEND_CMD);              /* LCD Standard Commands, horizontal addressing mode */
 }
 
 
 
