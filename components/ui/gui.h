 /* 
  * Graphical user interface using Nokia LCD display. 
  * Routines for drawing text, lines and cicles on screen. 
  * By LA7ECA, ohanssen@acm.org
  */
 
 #define DISPLAY_WIDTH  84
 #define DISPLAY_HEIGHT 48
 
 
 void gui_welcome(void);
 void gui_welcome2(void);
 void gui_writeText(int x, int y, const char * strp);
 void gui_flush(void);
 void gui_clear(void);
 void gui_setPixel(int x, int y, bool on);
 void gui_inverseMode(bool on);
 void gui_vLine(int x, int y, int len);
 void gui_hLine(int x, int y, int len);
 void gui_line(int x0, int y0, int x1, int y1);
 void gui_circle(int xm, int ym, int r);
 void gui_box(int x, int y, int width, int height, bool fill);
 void gui_battery(int x, int y, int lvl);
 void gui_menu(const char* items[], int sel);
 void gui_flag(int x, int y, char *sign, bool on);
 void gui_label(int x, int y, char* lbl);
 void gui_frame(); 
 void gui_fwupgrade();
 bool gui_popupActive();
 void gui_sleepmode();
 
 void menu_init(void);
 bool menu_is_active(void);
 void menu_activate(void);
 void menu_increment(void);
 void menu_select(void);
 void menu_end(void);

 void status_init();
 void status_show(void);
 void status_next(void);
