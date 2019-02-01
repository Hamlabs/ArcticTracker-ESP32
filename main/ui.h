#if !defined __UI_H__
#define __UI_H__


typedef void (*butthandler_t)(void*);
 
void ui_init(void);

#define BEEP_FREQ 2900
#define BEEP_ALT_FREQ 3040

#define beep(t) _beep(BEEP_FREQ, (t))
#define hbeep(t) _beep(BEEP_ALT_FREQ, (t))


#endif
