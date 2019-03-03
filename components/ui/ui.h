#if !defined __UI_H__
#define __UI_H__


typedef void (*butthandler_t)(void*);
 
void ui_init(void);
void register_button_handlers(butthandler_t h1, butthandler_t h2);

#define BEEP_FREQ 2900
#define BEEP_ALT_FREQ 3040

#define beep(t) _beep(BEEP_FREQ, (t))
#define hbeep(t) _beep(BEEP_ALT_FREQ, (t))

void _beep(uint16_t freq, uint16_t time);
void blipUp();
void blipDown();
void beeps(char* s);

#endif
