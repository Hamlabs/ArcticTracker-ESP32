 
#if DEVICE == T_TWR

#include <stdio.h>
#include "defines.h"
#include "system.h"
#include "ui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"


#if DEVICE == T_TWR

// Numbers of the LED in the strip
#define LED_STRIP_LED_NUMBERS 1
// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)


led_strip_handle_t neopixel;



led_strip_handle_t neopixel_init(void)
{
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = NEOPIXEL_PIN,   // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_NUMBERS,        // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
    };

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    return led_strip;
}


void rgbLED_init(void) {
    neopixel = neopixel_init(); 
}


bool override = false;

void _rgbLED_on(int red, int green, int blue) {
    ESP_ERROR_CHECK(led_strip_set_pixel(neopixel, 0, red, green, blue));
    ESP_ERROR_CHECK(led_strip_refresh(neopixel));
}

void rgbLED_on(int red, int green, int blue) {
    override = true;
    _rgbLED_on(red, green, blue);
}


void _rgbLED_off() {
    ESP_ERROR_CHECK(led_strip_clear(neopixel));
}


void rgbLED_off() {
    override = false;
    _rgbLED_off();
}

#endif


/*********************************************************************
 * Set style of RGB LED blinking
 *********************************************************************/

#if DEVICE == T_TWR

struct style {
    uint16_t blink_length;
    uint16_t blink_interval;
    uint16_t red;
    uint16_t green;
    uint16_t blue;
};
struct style st[2];
uint8_t level;


void rgbLED_setBlink(uint8_t lvl, uint16_t r, uint16_t g, uint16_t b, uint16_t len, uint16_t interv) {
    st[lvl].red=r;
    st[lvl].green=g;
    st[lvl].blue=b;
    st[lvl].blink_length=len;
    st[lvl].blink_interval=interv;
    
    if (lvl > level)
        level = lvl;
}

void rgbLED_down() {
    level = 0;
}



#else
uint16_t blink_length=500, blink_interval=500;
bool blink_both=false;

void led_setBlink(uint16_t len, uint16_t interv, bool both ) {
    blink_length=len;
    blink_interval=interv;
    blink_both=both;
}

#endif


#if DEVICE == ARCTIC4
#define ARCTIC_LED_OFF 1
#define ARCTIC_LED_ON 0
#else
#define ARCTIC_LED_ON 1
#define ARCTIC_LED_OFF 0
#endif


/*********************************************************************
 * Main thread. LED blinking to indicate  things
 *********************************************************************/
 
static void led_thread(void* arg)
{
    (void)arg;
    blipUp();
    sleepMs(300);
    BLINK_NORMAL;
#if DEVICE == T_TWR
    while (1) {
        int16_t pbatt = batt_percent();
        if (batt_charge())
            BLINK_CHARGE;
        else if (pbatt < 20)
            BLINK_BATTLOW;
        else if (pbatt < 10)
            BLINK_BATTCRITICAL;
        else
            rgbLED_down();
        
        struct style s = st[level];
        if (!override)
            _rgbLED_on(s.red, s.green, s.blue);
        sleepMs(s.blink_length);
        if (!override) 
            _rgbLED_off();
        sleepMs(s.blink_interval);
    }
#else
    while (1) {
        gpio_set_level(LED_STATUS_PIN, ARCTIC_LED_ON);
        if (blink_both)
            gpio_set_level(LED_TX_PIN, ARCTIC_LED_OFF);
        sleepMs(blink_length);
        gpio_set_level(LED_STATUS_PIN, ARCTIC_LED_OFF);
        if (blink_both)
            gpio_set_level(LED_TX_PIN, ARCTIC_LED_ON);
        sleepMs(blink_interval);
    }
#endif
}
 
 
 
 
void led_init() {
        
#if DEVICE == T_TWR
    rgbLED_init();
#else
    gpio_set_direction(LED_STATUS_PIN,  GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_TX_PIN,  GPIO_MODE_OUTPUT);
    gpio_set_level(LED_STATUS_PIN, ARCTIC_LED_OFF);
    gpio_set_level(LED_TX_PIN, ARCTIC_LED_OFF);
#endif
    
    /* LED blinker thread */
    xTaskCreatePinnedToCore(&led_thread, "LED blinker", 
        STACK_LEDBLINKER, NULL, NORMALPRIO, NULL, CORE_LEDBLINKER);
}



#endif
