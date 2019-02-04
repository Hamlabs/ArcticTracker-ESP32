/* 
 * Routines for controlling a Nokia LCD display, 
 * using the SPI bus. 
 * By LA7ECA, ohanssen@acm.org
 */ 

#ifndef LCD_H
#define LCD_H

#include "driver/spi_master.h"

#define LCD_X_RES                  84
#define LCD_Y_RES                  48

#define LCD_FONT_X_SIZE             5
#define LCD_FONT_Y_SIZE             8

#define LCD_SEND_CMD                0
#define LCD_SEND_DATA               1



void lcd_init();
void lcd_backlight(void);
void lcd_writeByte(uint8_t data, uint8_t cd);
void lcd_contrast(uint8_t contrast);
void lcd_clear(void);
void lcd_setPosXY(uint8_t x, uint8_t y);



#endif /* LCD_H */


