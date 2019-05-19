/* 
 * Graphical user interface using Nokia LCD display. 
 * Routines for drawing text, lines and cicles on screen. 
 * By LA7ECA, ohanssen@acm.org
 */



#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "lcd.h"
#include "gui.h"


uint8_t buffer [DISPLAY_HEIGHT/8] [DISPLAY_WIDTH];
bool    changed [DISPLAY_HEIGHT/8];


bool _inverse = false;
bool _popup = false; 

#define FONT_X_SIZE             5
#define FONT_Y_SIZE             8



const uint8_t  Fonts8x5 [][FONT_X_SIZE] =
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



void gui_writeText(int x, int y, const char * strp) 
{
  uint8_t i;
  int offset = y % 8; 
  changed[y/8] = true; 
  if (offset > 0)
      changed[y/8+1] = true;
  
  while (*strp && x < DISPLAY_WIDTH-1) {
     char c = *strp;
     if (c == '.' || c==',' || c==':' || c==';' || c== '1' || c=='l' || c == 'I' || c=='i' || c == 'j' || c== ' ')
         x--;
     for ( i = 0; i < FONT_X_SIZE; i++ ) {
         if (x < DISPLAY_WIDTH-1) {
            buffer[y/8][x] ^= (Fonts8x5[c - 32][i] << offset);   
            if (offset > 0) 
               buffer[y/8+1][x] ^= Fonts8x5[c - 32][i] >> (8-offset);
            x++; 
         }
     }
     if (c == '.'  || c==',' || c==':' || c==';' || c== '1' || c=='l' || c == 'I' || c=='i' || c == 'j' || c == ' ')
         x--;
     strp++;
     x++;
  }
}

   

/********************************************************
 * Synchronise buffer to physical screen
 ********************************************************/

void gui_flush() 
{
  uint16_t i,j;
  lcd_setPosXY(0,0);
  for (i = 0; i < DISPLAY_HEIGHT/8; i++) 
    if (changed[i]) {
       changed[i] = false;
       for (j = 0; j < DISPLAY_WIDTH; j++) 
         lcd_writeByte(buffer[i][j], LCD_SEND_DATA);
    }
}


/********************************************************
 * Clear screen (in buffer)
 ********************************************************/

void gui_clear() 
{ 
  uint16_t i, j;
  for (i = 0; i < DISPLAY_HEIGHT/8; i++) {
    changed[i] = true;
    for (j = 0; j < DISPLAY_WIDTH; j++)
      buffer[i][j] = 0;
  }
}


/********************************************************
 * Set one pixel at x,y to on or off (on=black)
 ********************************************************/

void gui_setPixel(int x, int y, bool on) 
{
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

void gui_inverseMode(bool on)
{ _inverse = on; }


/********************************************************
 * Draw vertical line starting at x,y with length len
 ********************************************************/

void gui_vLine(int x, int y, int len) 
{
  int i;
  for (i=y; i<y+len; i++)
     gui_setPixel(x, i, !_inverse);
}


/********************************************************
 * Draw horizontal line starting at x,y with length len
 ********************************************************/

void gui_hLine(int x, int y, int len) 
{
  int i;
  for (i=x; i<x+len; i++)
     gui_setPixel(i,y, !_inverse);
}


/**********************************************************
 * Plot line between two points 
 * Bresenhams algorithm. 
 *********************************************************/

void gui_line(int x0, int y0, int x1, int y1)
{
   int dx =  abs(x1-x0), sx = x0<x1 ? 1 : -1;
   int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1; 
   int err = dx+dy, e2; /* error value e_xy */
 
   for(;;){  /* loop */
      gui_setPixel(x0,y0, !_inverse);
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

void gui_circle(int xm, int ym, int r)
{
   int x = -r, y = 0, err = 2-2*r; /* II. Quadrant */ 
   do {
      gui_setPixel(xm-x, ym+y, !_inverse); /*   I. Quadrant */
      gui_setPixel(xm-y, ym-x, !_inverse); /*  II. Quadrant */
      gui_setPixel(xm+x, ym-y, !_inverse); /* III. Quadrant */
      gui_setPixel(xm+y, ym+x, !_inverse); /*  IV. Quadrant */
      
      r = err;
      if (r <= y) err += ++y*2+1;           /* e_xy+e_y < 0 */
      if (r > x || err > y) err += ++x*2+1; /* e_xy+e_x > 0 or no 2nd y-step */
   } while (x < 0);
}



/********************************************************
 * Draw box starting at x,y of width w and height h
 ********************************************************/

void gui_box(int x, int y, int width, int height, bool fill) 
{
   int i;
   if (fill)
      for (i=0;i<width; i++)
	 gui_vLine(x+i, y, height);
   else {
      gui_vLine(x,y,height);
      gui_vLine(x+width-1, y, height);
      gui_hLine(x,y,width);
      gui_hLine(x,y+height-1, width);
   }
}



/********************************************************
 * Battery indicator 
 *  has 5 levels. 0 is empty, 4 is full
 ********************************************************/

void gui_battery(int x, int y, int lvl) 
{
   gui_vLine(x,y,6);
   gui_hLine(x,y,11);
   gui_hLine(x,y+6,11);
   gui_vLine(x+10,y,2);
   gui_vLine(x+10,y+5,2);
   if (lvl >= 1) gui_box(x+1,y+1,3,5, true);
   if (lvl >= 2) gui_box(x+4,y+1,2,5, true);
   if (lvl >= 3) gui_box(x+6,y+1,2,5, true);
   if (lvl >= 4) gui_box(x+8,y+1,2,5, true);
   gui_vLine(x+11,y+2,3);
}



/***********************************************************
 * Flag indicator
 ***********************************************************/

void gui_flag(int x, int y, char *sign, bool on) 
{
  if (!on) return; 
  gui_box(x, y, 7, 11, on); 
  int offs = 1;
  if (sign[0] == 'i')
    offs = 2;
  if (on)
    gui_writeText(x+offs, y+2, sign);
  gui_setPixel(x,y, false);
}



/*************************************************
 * Label
 *************************************************/

void gui_label(int x, int y, char* lbl)
{
  gui_box(x,y,27,11, true);
  gui_writeText(x+2, y+2, lbl);
  gui_setPixel(x,y, false);
}


/*************************************************
 * Frame/popup
 *************************************************/

void gui_frame() 
{
   gui_hLine(1,0,82);
   gui_hLine(1,44,83);
   gui_hLine(3,45,82);
   gui_vLine(0,0,45);
   gui_vLine(82,0,45);
   gui_vLine(83,3,41);
}


void gui_fwupgrade()
{
    _popup = true; 
    gui_clear(); 
    gui_frame(); 
    gui_writeText(7,7, "Firmware");
    gui_writeText(7,16, "Upgrade...");
    gui_flush();
}

bool gui_popupActive()
    {return _popup; }
    
    
    

/*************************************************
 * Welcome message
 *************************************************/

void gui_welcome() 
{
  gui_clear();
  gui_circle(40,24,10);
  gui_line(40,2,40,55);
  gui_line(14,24,66,24);
  gui_writeText(2,7,"Arctic");
  gui_writeText(43,36, "Tracker");
  gui_flush();
}



void gui_welcome2() 
{
  gui_clear();
  gui_circle(40,24,10);
  gui_line(40,2,40,55);
  gui_line(14,24,66,24);
  gui_writeText(4,7,"Arctic Tracker");
  gui_writeText(4,36, "Hacker Cracker");
  gui_flush();
}




