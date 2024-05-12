#if !defined __UI_H__
#define __UI_H__


typedef void (*butthandler_t)(void*);
 
void ui_init(void);
void register_button_handlers(butthandler_t h1, butthandler_t h2);

extern uint16_t blink_length, blink_interval; 
extern bool blink_both;

#if DEVICE == T_TWR
#define BLINK_GPS_SEARCHING  rgbLED_setBlink(0, 6, 6, 0, 100, 2100)
#define BLINK_NORMAL         rgbLED_setBlink(0, 5, 5, 5, 100, 2100)
#define BLINK_CHARGE         rgbLED_setBlink(1, 6, 0, 6, 100, 2100)
#define BLINK_BATTLOW        rgbLED_setBlink(1, 7, 0, 0, 100, 2100)
#define BLINK_BATTCRITICAL   rgbLED_setBlink(1, 7, 0, 0, 100, 500)
#define BLINK_FWUPGRADE      rgbLED_setBlink(1, 0, 2, 40, 80, 80)

#else
#define BLINK_GPS_SEARCHING  led_setBlink(500, 500, false)
#define BLINK_NORMAL         led_setBlink(30, 2000, false)
#define BLINK_CHARGE        
#define BLINK_BATTLOW        
#define BLINK_BATTCRITICAL 
#define BLINK_FWUPGRADE      led_setBlink(50, 50, true)
#endif


#if DEVICE == T_TWR
#define BEEP_FREQ 1800
#define BEEP_ALT_FREQ 2000
#elif  DEVICE == ARCTIC4 
#define BEEP_FREQ 2750
#define BEEP_ALT_FREQ 2900
#else
#define BEEP_FREQ 2800
#define BEEP_ALT_FREQ 3050
#endif


#define beep(t) _beep(BEEP_FREQ, (t))
#define hbeep(t) _beep(BEEP_ALT_FREQ, (t))
#define ring(t)  _beep(20, (t))
#define alarm(t)  _beep(6, (t))

#if DEVICE == T_TWR

#define tx_led_on()  rgbLED_on(60, 0, 0) 
#define tx_led_off() rgbLED_off() 
void rgbLED_init();
void rgbLED_on(int red, int green, int blue);
void rgbLED_off();
void rgbLED_setBlink(uint8_t lvl, uint16_t r, uint16_t g, uint16_t b, uint16_t len, uint16_t interv );
void rgbLED_down();

#else
#if DEVICE == ARCTIC4
#define tx_led_on()  gpio_set_level(LED_TX_PIN, 0)
#define tx_led_off() gpio_set_level(LED_TX_PIN, 1)
#else
#define tx_led_on()  gpio_set_level(LED_TX_PIN, 1)
#define tx_led_off() gpio_set_level(LED_TX_PIN, 0)
#endif

void led_setBlink(uint16_t len, uint16_t interv, bool both );

#endif

void led_init();
void _beep(uint16_t freq, uint16_t time);
void blipUp();
void blipDown();
void beeps(char* s);



#endif
