idf_component_register(
    SRCS "led.c" "buzzer.c" "commands.c" "cmd_register.c" "gui_menu.c" "gui_status.c" "display.c" "ui.c" "ssd1306_i2c.c"
    
    INCLUDE_DIRS "." "../../main" 
    REQUIRES networking console aprs led_strip main
)
