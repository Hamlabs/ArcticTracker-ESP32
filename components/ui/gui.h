 /* 
  * Graphical user interface using a LCD or OLED display. 
  * Routines for drawing text, lines and cicles on screen. 
  * Menu and status screens. 
  * By LA7ECA, ohanssen@acm.org
  */


#if DISPLAY_TYPE == 0
// Nokia display. Not supported anymore

#define DISPLAY_WIDTH  84
#define DISPLAY_HEIGHT 48

#elif DISPLAY_TYPE == 1
// (0.96") is a very popular configuration, using SSH1306

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT  64

#elif DISPLAY_TYPE == 2
// (1.3")  is used in T-TWR and Arctic Tracker, using SH1106

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT  64
#define SH1106_HACK      1

#elif DISPLAY_TYPE == 3
// (1.5")  planned, using SH1107

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 128

#elif DISPLAY_TYPE == 4
// (0.91") half height

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 32

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
 bool disp_isDimmed();
 void disp_init();
 void disp_setBoldFont(bool on);
 void disp_setHighFont(bool on, bool xspace);
 
 
 void gui_menu(const char* items[], int sel);
 void gui_welcome(void);
 void gui_fwupgrade();
 void gui_setPause(int n);

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
