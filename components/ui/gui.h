 /* 
  * Graphical user interface using a LCD or OLED display. 
  * Routines for drawing text, lines and cicles on screen. 
  * Menu and status screens. 
  * By LA7ECA, ohanssen@acm.org
  */


#if DISPLAY_TYPE == 0

#define DISPLAY_WIDTH  84
#define DISPLAY_HEIGHT 48

#elif DISPLAY_TYPE == 1

#define DISPLAY_WIDTH  SSD1306_WIDTH
#define DISPLAY_HEIGHT SSD1306_HEIGHT

#endif

 void disp_toggleBacklight(void);
 void disp_backlight(void);
 void disp_writeText(int x, int y, const char * strp);
 void disp_flush(void);
 void disp_clear(void);
 void disp_setPixel(int x, int y, bool on);
 void disp_inverseMode(bool on);
 void disp_vLine(int x, int y, int len);
 void disp_hLine(int x, int y, int len);
 void disp_line(int x0, int y0, int x1, int y1);
 void disp_circle(int xm, int ym, int r);
 void disp_box(int x, int y, int width, int height, bool fill);
 void disp_battery(int x, int y, int lvl);
 void disp_flag(int x, int y, char *sign, bool on);
 void disp_label(int x, int y, char* lbl);
 void disp_frame(); 
 void disp_setPopup();
 bool disp_popupActive();
 void disp_sleepmode(bool);
 void disp_init();
 void disp_setBoldFont(bool on);
 
 void gui_menu(const char* items[], int sel);
 void gui_welcome(void);
 void gui_fwupgrade();

 void menu_init(void);
 bool menu_is_active(void);
 void menu_activate(void);
 void menu_increment(void); 
 void menu_decrement(void);
 void menu_select(void);
 void menu_end(void);

 void status_init();
 void status_show(void);
 void status_next(void);
 void status_prev(void);
