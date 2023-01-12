/* 
 * Graphical user interface using a display. Either a 
 * SSD1306 based 128x64 pix or a old Nokia display.   
 * Routines for drawing text, lines and cicles on screen. 
 * By LA7ECA, ohanssen@acm.org
 */

#include "defines.h"
#include "system.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "gui.h"


/********************************CONFIG FOR NOKIA DISPLAY************************************/
#if DISPLAY_TYPE == 0

#include "disp_nokia.h"

/*******************************Config for SSD1306 displays**********************************/
#elif DISPLAY_TYPE == 1

#include "ssd1306.h"

#define DISP_RESET_PIN  -1
SSD1306_t  _dev_;

void i2c_master_init(SSD1306_t * dev, int16_t sda, int16_t scl, int16_t reset);
void i2c_init(SSD1306_t * dev, int width, int height);
void i2c_display_image(SSD1306_t * dev, int page, int seg, uint8_t * images, int width);
static void blhandler(TimerHandle_t timer);


/************************************** Not defined ******************************************/

#else

#define DISPLAY_HEIGHT 8
#define DISPLAY_WIDTH 8

#endif

/*********************************************************************************************/

#if defined SH1106_HACK
uint8_t buffer [DISPLAY_HEIGHT/8] [DISPLAY_WIDTH+4];
#else
uint8_t buffer [DISPLAY_HEIGHT/8] [DISPLAY_WIDTH];
#endif 

bool    changed [DISPLAY_HEIGHT/8];

bool _inverse = false;
bool _popup = false; 


const uint8_t  Font8x5 [][5] =
{
  { 0x00, 0x00, 0x00, 0x00, 0x00 },   /* space */
  { 0x00, 0x00, 0x2f, 0x00, 0x00 },   /* ! */
  { 0x00, 0x07, 0x00, 0x07, 0x00 },   /* " */
  { 0x14, 0x7f, 0x14, 0x7f, 0x14 },   /* # */
  { 0x24, 0x2a, 0x7f, 0x2a, 0x12 },   /* $ */
  { 0xc4, 0xc8, 0x10, 0x26, 0x46 },   /* % */
  { 0x36, 0x49, 0x55, 0x22, 0x50 },   /* & */
  { 0x00, 0x05, 0x03, 0x00, 0x00 },   /* ' */
  { 0x00, 0x1c, 0x22, 0x41, 0x00 },   /* ( */
  { 0x00, 0x41, 0x22, 0x1c, 0x00 },   /* ) */
  { 0x14, 0x08, 0x3E, 0x08, 0x14 },   /* * */
  { 0x08, 0x08, 0x3E, 0x08, 0x08 },   /* + */
  { 0x00, 0x00, 0xA0, 0x60, 0x00 },   /* , */
  { 0x10, 0x10, 0x10, 0x10, 0x10 },   /* - */
  { 0x00, 0x60, 0x60, 0x00, 0x00 },   /* . */
  { 0x20, 0x10, 0x08, 0x04, 0x02 },   /* / */
  { 0x3E, 0x51, 0x49, 0x45, 0x3E },   /* 0 */
  { 0x00, 0x42, 0x7F, 0x40, 0x00 },   /* 1 */
  { 0x42, 0x61, 0x51, 0x49, 0x46 },   /* 2 */
  { 0x21, 0x41, 0x45, 0x4B, 0x31 },   /* 3 */
  { 0x18, 0x14, 0x12, 0x7F, 0x10 },   /* 4 */
  { 0x27, 0x45, 0x45, 0x45, 0x39 },   /* 5 */
  { 0x3C, 0x4A, 0x49, 0x49, 0x30 },   /* 6 */
  { 0x01, 0x71, 0x09, 0x05, 0x03 },   /* 7 */
  { 0x36, 0x49, 0x49, 0x49, 0x36 },   /* 8 */
  { 0x06, 0x49, 0x49, 0x29, 0x1E },   /* 9 */
  { 0x00, 0x36, 0x36, 0x00, 0x00 },   /* : */
  { 0x00, 0xAC, 0x6C, 0x00, 0x00 },   /* ; */
  { 0x08, 0x14, 0x22, 0x41, 0x00 },   /* < */
  { 0x14, 0x14, 0x14, 0x14, 0x14 },   /* = */
  { 0x00, 0x41, 0x22, 0x14, 0x08 },   /* > */
  { 0x02, 0x01, 0x51, 0x09, 0x06 },   /* ? */
  { 0x32, 0x49, 0x59, 0x51, 0x3E },   /* @ */
  { 0x7E, 0x11, 0x11, 0x11, 0x7E },   /* A */
  { 0x7F, 0x49, 0x49, 0x49, 0x36 },   /* B */
  { 0x3E, 0x41, 0x41, 0x41, 0x22 },   /* C */
  { 0x7F, 0x41, 0x41, 0x22, 0x1C },   /* D */
  { 0x7F, 0x49, 0x49, 0x49, 0x41 },   /* E */
  { 0x7F, 0x09, 0x09, 0x09, 0x01 },   /* F */
  { 0x3E, 0x41, 0x49, 0x49, 0x7A },   /* G */
  { 0x7F, 0x08, 0x08, 0x08, 0x7F },   /* H */
  { 0x00, 0x41, 0x7F, 0x41, 0x00 },   /* I */
  { 0x20, 0x40, 0x41, 0x3F, 0x01 },   /* J */
  { 0x7F, 0x08, 0x14, 0x22, 0x41 },   /* K */
  { 0x7F, 0x40, 0x40, 0x40, 0x40 },   /* L */
  { 0x7F, 0x02, 0x0C, 0x02, 0x7F },   /* M */
  { 0x7F, 0x04, 0x08, 0x10, 0x7F },   /* N */
  { 0x3E, 0x41, 0x41, 0x41, 0x3E },   /* O */
  { 0x7F, 0x09, 0x09, 0x09, 0x06 },   /* P */
  { 0x3E, 0x41, 0x51, 0x21, 0x5E },   /* Q */
  { 0x7F, 0x09, 0x19, 0x29, 0x46 },   /* R */
  { 0x46, 0x49, 0x49, 0x49, 0x31 },   /* S */
  { 0x01, 0x01, 0x7F, 0x01, 0x01 },   /* T */
  { 0x3F, 0x40, 0x40, 0x40, 0x3F },   /* U */
  { 0x1F, 0x20, 0x40, 0x20, 0x1F },   /* V */
  { 0x3F, 0x40, 0x38, 0x40, 0x3F },   /* W */
  { 0x63, 0x14, 0x08, 0x14, 0x63 },   /* X */
  { 0x07, 0x08, 0x70, 0x08, 0x07 },   /* Y */
  { 0x61, 0x51, 0x49, 0x45, 0x43 },   /* Z */
  { 0x00, 0x7F, 0x41, 0x41, 0x00 },   /* [ */
  { 0x55, 0x2A, 0x55, 0x2A, 0x55 },   /* \ */
  { 0x00, 0x41, 0x41, 0x7F, 0x00 },   /* ] */
  { 0x04, 0x02, 0x01, 0x02, 0x04 },   /* ^ */
  { 0x40, 0x40, 0x40, 0x40, 0x40 },   /* _ */
  { 0x00, 0x01, 0x02, 0x04, 0x00 },   /* ' */
  { 0x20, 0x54, 0x54, 0x54, 0x78 },   /* a */
  { 0x7F, 0x48, 0x44, 0x44, 0x38 },   /* b */
  { 0x38, 0x44, 0x44, 0x44, 0x20 },   /* c */
  { 0x38, 0x44, 0x44, 0x48, 0x7F },   /* d */
  { 0x38, 0x54, 0x54, 0x54, 0x18 },   /* e */
  { 0x08, 0x7E, 0x09, 0x01, 0x02 },   /* f */
  { 0x18, 0xA4, 0xA4, 0xA4, 0x7C },   /* g */
  { 0x7F, 0x08, 0x04, 0x04, 0x78 },   /* h */
  { 0x00, 0x44, 0x7D, 0x40, 0x00 },   /* i */
  { 0x40, 0x80, 0x84, 0x7D, 0x00 },   /* j */
  { 0x7F, 0x10, 0x28, 0x44, 0x00 },   /* k */
  { 0x00, 0x41, 0x7F, 0x40, 0x00 },   /* l */
  { 0x7C, 0x04, 0x18, 0x04, 0x78 },   /* m */
  { 0x7C, 0x08, 0x04, 0x04, 0x78 },   /* n */
  { 0x38, 0x44, 0x44, 0x44, 0x38 },   /* o */
  { 0xFC, 0x24, 0x24, 0x24, 0x18 },   /* p */
  { 0x18, 0x24, 0x24, 0x28, 0xFC },   /* q */
  { 0x7C, 0x08, 0x04, 0x04, 0x08 },   /* r */
  { 0x48, 0x54, 0x54, 0x54, 0x20 },   /* s */
  { 0x04, 0x3F, 0x44, 0x40, 0x20 },   /* t */
  { 0x3C, 0x40, 0x40, 0x20, 0x7C },   /* u */
  { 0x1C, 0x20, 0x40, 0x20, 0x1C },   /* v */
  { 0x3C, 0x40, 0x30, 0x40, 0x3C },   /* w */
  { 0x44, 0x28, 0x10, 0x28, 0x44 },   /* x */
  { 0x1C, 0xA0, 0xA0, 0xA0, 0x7C },   /* y */
  { 0x44, 0x64, 0x54, 0x4C, 0x44 },   /* z */
  { 0x00, 0x08, 0x36, 0x41, 0x00 },   /* { */
  { 0x00, 0x00, 0x7F, 0x00, 0x00 },   /* | */
  { 0x00, 0x41, 0x36, 0x08, 0x00 },   /* } */
};

const uint8_t Font8x7[][7] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // (space)
    { 0x00, 0x00, 0x06, 0x5F, 0x5F, 0x06, 0x00 },   // (!)
    { 0x00, 0x03, 0x03, 0x00, 0x03, 0x03, 0x00 },   // (")
    { 0x14, 0x7F, 0x7F, 0x14, 0x7F, 0x7F, 0x14 },   // (#)
    { 0x24, 0x2E, 0x6B, 0x6B, 0x3A, 0x12, 0x00 },   // ($)
    { 0x46, 0x66, 0x30, 0x18, 0x0C, 0x66, 0x62 },   // (%)
    { 0x30, 0x7A, 0x4F, 0x5D, 0x37, 0x7A, 0x48 },   // (&)
    { 0x04, 0x07, 0x03, 0x00, 0x00, 0x00, 0x00 },   // (')
    { 0x00, 0x1C, 0x3E, 0x63, 0x41, 0x00, 0x00 },   // (()
    { 0x00, 0x41, 0x63, 0x3E, 0x1C, 0x00, 0x00 },   // ())
    { 0x08, 0x2A, 0x3E, 0x1C, 0x1C, 0x3E, 0x2A },   // (*)
    { 0x08, 0x08, 0x3E, 0x3E, 0x08, 0x08, 0x00 },   // (+)
    { 0x00, 0x80, 0xE0, 0x60, 0x00, 0x00, 0x00 },   // (,)
    { 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00 },   // (-)
    { 0x00, 0x00, 0x60, 0x60, 0x00, 0x00, 0x00 },   // (.)
    { 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01 },   // (/)
    { 0x3E, 0x7F, 0x71, 0x59, 0x4D, 0x7F, 0x3E },   // (0)
    { 0x40, 0x42, 0x7F, 0x7F, 0x40, 0x40, 0x00 },   // (1)
    { 0x62, 0x73, 0x59, 0x49, 0x6F, 0x66, 0x00 },   // (2)
    { 0x22, 0x63, 0x49, 0x49, 0x7F, 0x36, 0x00 },   // (3)
    { 0x18, 0x1C, 0x16, 0x53, 0x7F, 0x7F, 0x50 },   // (4)
    { 0x27, 0x67, 0x45, 0x45, 0x7D, 0x39, 0x00 },   // (5)
    { 0x3C, 0x7E, 0x4B, 0x49, 0x79, 0x30, 0x00 },   // (6)
    { 0x03, 0x03, 0x71, 0x79, 0x0F, 0x07, 0x00 },   // (7)
    { 0x36, 0x7F, 0x49, 0x49, 0x7F, 0x36, 0x00 },   // (8)
    { 0x06, 0x4F, 0x49, 0x69, 0x3F, 0x1E, 0x00 },   // (9)
    { 0x00, 0x00, 0x66, 0x66, 0x00, 0x00, 0x00 },   // (:)
    { 0x00, 0x80, 0xE6, 0x66, 0x00, 0x00, 0x00 },   // (;)
    { 0x08, 0x1C, 0x36, 0x63, 0x41, 0x00, 0x00 },   // (<)
    { 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x00 },   // (=)
    { 0x00, 0x41, 0x63, 0x36, 0x1C, 0x08, 0x00 },   // (>)
    { 0x02, 0x03, 0x51, 0x59, 0x0F, 0x06, 0x00 },   // (?)
    { 0x3E, 0x7F, 0x41, 0x5D, 0x5D, 0x1F, 0x1E },   // (@)
    { 0x7C, 0x7E, 0x13, 0x13, 0x7E, 0x7C, 0x00 },   // (A)
    { 0x41, 0x7F, 0x7F, 0x49, 0x49, 0x7F, 0x36 },   // (B)
    { 0x1C, 0x3E, 0x63, 0x41, 0x41, 0x63, 0x22 },   // (C)
    { 0x41, 0x7F, 0x7F, 0x41, 0x63, 0x3E, 0x1C },   // (D)
    { 0x41, 0x7F, 0x7F, 0x49, 0x5D, 0x41, 0x63 },   // (E)
    { 0x41, 0x7F, 0x7F, 0x49, 0x1D, 0x01, 0x03 },   // (F)
    { 0x1C, 0x3E, 0x63, 0x41, 0x51, 0x73, 0x72 },   // (G)
    { 0x7F, 0x7F, 0x08, 0x08, 0x7F, 0x7F, 0x00 },   // (H)
    { 0x00, 0x41, 0x7F, 0x7F, 0x41, 0x00, 0x00 },   // (I)
    { 0x30, 0x70, 0x40, 0x41, 0x7F, 0x3F, 0x01 },   // (J)
    { 0x41, 0x7F, 0x7F, 0x08, 0x1C, 0x77, 0x63 },   // (K)
    { 0x41, 0x7F, 0x7F, 0x41, 0x40, 0x60, 0x70 },   // (L)
    { 0x7F, 0x7F, 0x0E, 0x1C, 0x0E, 0x7F, 0x7F },   // (M)
    { 0x7F, 0x7F, 0x06, 0x0C, 0x18, 0x7F, 0x7F },   // (N)
    { 0x1C, 0x3E, 0x63, 0x41, 0x63, 0x3E, 0x1C },   // (O)
    { 0x41, 0x7F, 0x7F, 0x49, 0x09, 0x0F, 0x06 },   // (P)
    { 0x1E, 0x3F, 0x21, 0x71, 0x7F, 0x5E, 0x00 },   // (Q)
    { 0x41, 0x7F, 0x7F, 0x09, 0x19, 0x7F, 0x66 },   // (R)
    { 0x26, 0x6F, 0x4D, 0x59, 0x73, 0x32, 0x00 },   // (S)
    { 0x03, 0x41, 0x7F, 0x7F, 0x41, 0x03, 0x00 },   // (T)
    { 0x7F, 0x7F, 0x40, 0x40, 0x7F, 0x7F, 0x00 },   // (U)
    { 0x1F, 0x3F, 0x60, 0x60, 0x3F, 0x1F, 0x00 },   // (V)
    { 0x7F, 0x7F, 0x30, 0x18, 0x30, 0x7F, 0x7F },   // (W)
    { 0x43, 0x67, 0x3C, 0x18, 0x3C, 0x67, 0x43 },   // (X)
    { 0x07, 0x4F, 0x78, 0x78, 0x4F, 0x07, 0x00 },   // (Y)
    { 0x47, 0x63, 0x71, 0x59, 0x4D, 0x67, 0x73 },   // (Z)
    { 0x00, 0x7F, 0x7F, 0x41, 0x41, 0x00, 0x00 },   // ([)
    { 0x01, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60 },   // (\)
    { 0x00, 0x41, 0x41, 0x7F, 0x7F, 0x00, 0x00 },   // (])
    { 0x08, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x08 },   // (^)
    { 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80 },   // (_)
    { 0x00, 0x00, 0x03, 0x07, 0x04, 0x00, 0x00 },   // (`)
    { 0x20, 0x74, 0x54, 0x54, 0x3C, 0x78, 0x40 },   // (a)
    { 0x41, 0x7F, 0x3F, 0x48, 0x48, 0x78, 0x30 },   // (b)
    { 0x38, 0x7C, 0x44, 0x44, 0x6C, 0x28, 0x00 },   // (c)
    { 0x30, 0x78, 0x48, 0x49, 0x3F, 0x7F, 0x40 },   // (d)
    { 0x38, 0x7C, 0x54, 0x54, 0x5C, 0x18, 0x00 },   // (e)
    { 0x48, 0x7E, 0x7F, 0x49, 0x03, 0x02, 0x00 },   // (f)
    { 0x98, 0xBC, 0xA4, 0xA4, 0xF8, 0x7C, 0x04 },   // (g)
    { 0x41, 0x7F, 0x7F, 0x08, 0x04, 0x7C, 0x78 },   // (h)
    { 0x00, 0x44, 0x7D, 0x7D, 0x40, 0x00, 0x00 },   // (i)
    { 0x60, 0xE0, 0x80, 0x80, 0xFD, 0x7D, 0x00 },   // (j)
    { 0x41, 0x7F, 0x7F, 0x10, 0x38, 0x6C, 0x44 },   // (k)
    { 0x00, 0x41, 0x7F, 0x7F, 0x40, 0x00, 0x00 },   // (l)
    { 0x7C, 0x7C, 0x18, 0x38, 0x1C, 0x7C, 0x78 },   // (m)
    { 0x7C, 0x7C, 0x04, 0x04, 0x7C, 0x78, 0x00 },   // (n)
    { 0x38, 0x7C, 0x44, 0x44, 0x7C, 0x38, 0x00 },   // (o)
    { 0x84, 0xFC, 0xF8, 0xA4, 0x24, 0x3C, 0x18 },   // (p)
    { 0x18, 0x3C, 0x24, 0xA4, 0xF8, 0xFC, 0x84 },   // (q)
    { 0x44, 0x7C, 0x78, 0x4C, 0x04, 0x1C, 0x18 },   // (r)
    { 0x48, 0x5C, 0x54, 0x54, 0x74, 0x24, 0x00 },   // (s)
    { 0x00, 0x04, 0x3E, 0x7F, 0x44, 0x24, 0x00 },   // (t)
    { 0x3C, 0x7C, 0x40, 0x40, 0x3C, 0x7C, 0x40 },   // (u)
    { 0x1C, 0x3C, 0x60, 0x60, 0x3C, 0x1C, 0x00 },   // (v)
    { 0x3C, 0x7C, 0x70, 0x38, 0x70, 0x7C, 0x3C },   // (w)
    { 0x44, 0x6C, 0x38, 0x10, 0x38, 0x6C, 0x44 },   // (x)
    { 0x9C, 0xBC, 0xA0, 0xA0, 0xFC, 0x7C, 0x00 },   // (y)
    { 0x4C, 0x64, 0x74, 0x5C, 0x4C, 0x64, 0x00 },   // (z)
    { 0x08, 0x08, 0x3E, 0x77, 0x41, 0x41, 0x00 },   // ({)
    { 0x00, 0x00, 0x00, 0x77, 0x77, 0x00, 0x00 },   // (|)
    { 0x41, 0x41, 0x77, 0x3E, 0x08, 0x08, 0x00 }    // (})
};




/* Default font and font width. disp_setBoldFont changes these 
 * Bold font is 0x7 
 */
static uint8_t *font = Font8x5; 
static uint8_t font_width = 5;

   
void disp_setBoldFont(bool on) {
    if (on) {
        font = Font8x7;
        font_width = 7;
    }
    else {
        font = Font8x5; 
        font_width = 5; 
    }
}



/************************************************************
 * Write text to display buffer. Max one line. Characters 
 * exceeding line lenght of display is simply cut off. 
 * 
 *  - x, y : offset (in pixels from upper left corner)
 *  - strp : text (null terminated).
 ************************************************************/

void disp_writeText(int x, int y, const char * strp) 
{
#if defined SH1106_HACK
    x += 2;
#endif
    
    
  uint8_t i;
  if (y+8 > DISPLAY_HEIGHT)
      return;
  
  /* y offset within a single row */
  int offset = y % 8; 
  /* 
   * Mark this row as changed. If offset within row, we also use 
   * the next row and need to mark this as well. 
   */
  changed[y/8] = true; 
  if (offset > 0)
      changed[y/8+1] = true;
  
  /* For each charater in string */
  while (*strp && x < DISPLAY_WIDTH-1) {
     char c = *strp;
     int ftindex = (c-32)*font_width; 
     
     if (c == '.' || c==',' || c==':' || c==';' || c== '1' || c=='l' || c == 'I' || c=='i' || c == 'j' || c== ' ')
         x--;
     
     /* For each 8-bit part of character */
     for ( i = 0; i < font_width; i++ ) {
         if (x < DISPLAY_WIDTH-1) {
            buffer[y/8][x] ^= (font[ftindex+i] << offset);   
            if (offset > 0) 
               buffer[y/8+1][x] ^= font[ftindex+i] >> (8-offset);
            x++; 
         }
     }
     if (c == '.'  || c==',' || c==':' || c==';' || c== '1' || c=='l' || c == 'I' || c=='i' || c == 'j' || c == ' ')
         x--;
     strp++;
     x++;
  }
}



#define BL_LOW 100
#define BL_HIGH 255

static TimerHandle_t bltimer;
static int backlightLevel = BL_HIGH;


   
void disp_init() 
{ 
#if DISPLAY_TYPE == 0
    spi_init(); 
    lcd_init();
#elif DISPLAY_TYPE == 1
    i2c_master_init(&_dev_, DISP_SDA_PIN, DISP_SCL_PIN, DISP_RESET_PIN); // S3 hangs here !!
    i2c_init(&_dev_, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    i2c_contrast(&_dev_, backlightLevel); 
    
    /* Software timer for backlight */
    uint32_t cnt = 0;
    bltimer = xTimerCreate ( 
        "DispLightTimer", pdMS_TO_TICKS(300000),  pdFALSE,
        ( void * ) &cnt, blhandler
    ); 
#endif  
}

    
/************************************************************
 * Turn on backlight and turn it off again after 10 seconds
 ************************************************************/

static void blhandler(TimerHandle_t timer) {
    i2c_contrast(&_dev_, 1);
}


static BaseType_t hpw = pdFALSE;     
void disp_backlight() 
{ 
#if DISPLAY_TYPE == 0
    lcd_backlight();
#elif DISPLAY_TYPE == 1
    i2c_contrast(&_dev_, backlightLevel);
    if (bltimer != NULL) 
        xTimerStartFromISR(bltimer, &hpw);
    
#endif
}  

void disp_toggleBacklight() {
    if (backlightLevel == BL_HIGH)
        backlightLevel = BL_LOW;
    else
        backlightLevel = BL_HIGH;
    i2c_contrast(&_dev_, backlightLevel);
} 

void disp_sleepmode() {
}



   
/********************************************************
 * Synchronise buffer to physical screen
 ********************************************************/

void disp_flush() 
{
    uint16_t i,j;
#if DISPLAY_TYPE == 0
    lcd_setPosXY(0,0);
#endif
    for (i = 0; i < DISPLAY_HEIGHT/8; i++) 
        if (changed[i]) {
            changed[i] = false;
#if DISPLAY_TYPE == 0 
            for (j = 0; j < DISPLAY_WIDTH; j++) 
                lcd_writeByte(buffer[i][j], LCD_SEND_DATA);
#elif DISPLAY_TYPE == 1        
            i2c_display_image(&_dev_, i, 0, buffer[i], DISPLAY_WIDTH+4);
#endif
        }
}




/********************************************************
 * Clear screen (in buffer)
 ********************************************************/

void disp_clear() 
{ 
  uint16_t i, j;
  for (i = 0; i < DISPLAY_HEIGHT/8; i++) {
    changed[i] = true;
#if defined SH1106_HACK
    for (j = 0; j < DISPLAY_WIDTH; j++)
#else
    for (j = 0; j < DISPLAY_WIDTH+4; j++)   
#endif
        buffer[i][j] = 0;
  }
}


/********************************************************
 * Set one pixel at x,y to on or off (on=black)
 ********************************************************/

void disp_setPixel(int x, int y, bool on) 
{
#if defined SH1106_HACK
    x += 2;
#endif
    
    if (y >= DISPLAY_HEIGHT)
        return;
    changed[y/8] = true;
    uint8_t b = (0x01 << (y % 8));
    if (on)
       buffer[y/8][x] |= b;
    else
       buffer[y/8][x] &= ~b;
}


/********************************************************
 * Set inverse graphics mode
 ********************************************************/

void disp_inverseMode(bool on)
{ _inverse = on; }


/********************************************************
 * Draw vertical line starting at x,y with length len
 ********************************************************/

void disp_vLine(int x, int y, int len) 
{
  int i;
  for (i=y; i<y+len; i++)
     disp_setPixel(x, i, !_inverse);
}


/********************************************************
 * Draw horizontal line starting at x,y with length len
 ********************************************************/

void disp_hLine(int x, int y, int len) 
{
  int i;
  for (i=x; i<x+len; i++)
     disp_setPixel(i,y, !_inverse);
}


/**********************************************************
 * Plot line between two points 
 * Bresenhams algorithm. 
 *********************************************************/

void disp_line(int x0, int y0, int x1, int y1)
{
   int dx =  abs(x1-x0), sx = x0<x1 ? 1 : -1;
   int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1; 
   int err = dx+dy, e2; /* error value e_xy */
 
   for(;;){  /* loop */
      disp_setPixel(x0,y0, !_inverse);
      if (x0==x1 && y0==y1) break;
      e2 = 2*err;
      if (e2 >= dy) { err += dy; x0 += sx; } /* e_xy+e_x > 0 */
      if (e2 <= dx) { err += dx; y0 += sy; } /* e_xy+e_y < 0 */
   }
}


/**********************************************************
 * Plot circle
 * Bresenhams algorithm. 
 *********************************************************/

void disp_circle(int xm, int ym, int r)
{
   int x = -r, y = 0, err = 2-2*r; /* II. Quadrant */ 
   do {
      disp_setPixel(xm-x, ym+y, !_inverse); /*   I. Quadrant */
      disp_setPixel(xm-y, ym-x, !_inverse); /*  II. Quadrant */
      disp_setPixel(xm+x, ym-y, !_inverse); /* III. Quadrant */
      disp_setPixel(xm+y, ym+x, !_inverse); /*  IV. Quadrant */
      
      r = err;
      if (r <= y) err += ++y*2+1;           /* e_xy+e_y < 0 */
      if (r > x || err > y) err += ++x*2+1; /* e_xy+e_x > 0 or no 2nd y-step */
   } while (x < 0);
}



/********************************************************
 * Draw box starting at x,y of width w and height h
 ********************************************************/

void disp_box(int x, int y, int width, int height, bool fill) 
{
   int i;
   if (fill)
      for (i=0;i<width; i++)
	 disp_vLine(x+i, y, height);
   else {
      disp_vLine(x,y,height);
      disp_vLine(x+width-1, y, height);
      disp_hLine(x,y,width);
      disp_hLine(x,y+height-1, width);
   }
}



/********************************************************
 * Battery indicator 
 *  has 5 levels. 0 is empty, 4 is full
 ********************************************************/

void disp_battery(int x, int y, int lvl) 
{
#if DISPLAY_TYPE == 0
    disp_vLine(x,y,6);
    disp_hLine(x,y,11);
    disp_hLine(x,y+6,11);
    disp_vLine(x+10,y,2);
    disp_vLine(x+10,y+5,2);
    
    if (lvl >= 1) disp_box(x+1,y+1,3,5, true);
    if (lvl >= 2) disp_box(x+4,y+1,2,5, true);
    if (lvl >= 3) disp_box(x+6,y+1,2,5, true);
    if (lvl >= 4) disp_box(x+8,y+1,2,5, true);
    disp_vLine(x+11,y+2,3);
#else
    disp_vLine(x,y,8);
    disp_hLine(x,y,12);
    disp_hLine(x,y+8,12);
    disp_vLine(x+12,y,2);
    disp_vLine(x+12,y+7,2);
   
    if (lvl >= 1) disp_box(x+1,y+2,2,5, true);
    if (lvl >= 2) disp_box(x+4,y+2,2,5, true);
    if (lvl >= 3) disp_box(x+7,y+2,2,5, true);
    if (lvl >= 4) disp_box(x+10,y+2,2,5, true);
    disp_vLine(x+13,y+2,5);
    disp_vLine(x+14,y+2,5);
#endif
}



/***********************************************************
 * Flag indicator
 ***********************************************************/

void disp_flag(int x, int y, char *sign, bool on) 
{
    if (!on) return; 
    
disp_setBoldFont(true);    
#if DISPLAY_TYPE == 0
    disp_box(x, y, 7, 11, on); 
    int offs = 1;
    if (sign[0] == 'i')
        offs = 2;
#else  
    disp_box(x, y, 9, 11, on); 
    int offs = 1;
    if (sign[0] == 'i')
        offs = 3;

#endif  
    if (on)
        disp_writeText(x+offs, y+2, sign);
    disp_setPixel(x,y, false);
    disp_setBoldFont(false);
}



/*************************************************
 * Label
 *************************************************/

void disp_label(int x, int y, char* lbl)
{
    disp_box(x,y,27,11, true);
    disp_writeText(x+2, y+2, lbl);
    disp_setPixel(x,y, false);
}



/*************************************************
 * Frame/popup
 *************************************************/

void disp_frame() 
{
#if DISPLAY_TYPE == 0
   disp_hLine(1,0,82);
   disp_hLine(1,44,83);
   disp_hLine(3,45,82);
   disp_vLine(0,0,45);
   disp_vLine(82,0,45);
   disp_vLine(83,3,41);
#else
   disp_hLine(1,0,110);   // top
   disp_hLine(1,59,110);  // bottom
   disp_vLine(0,0,59);    //left
   disp_vLine(110,0,59);  // right
#endif   
}


/* Set popup mode */ 
void disp_setPopup() 
    { _popup = true; } 

/* return true if popup mode */
bool disp_popupActive()
    { return _popup; }
    






