idf_component_register(
    SRCS "buzzer.c" "commands.c" "cmd_register.c" "gui_menu.c" "gui_status.c" "display.c" "disp_nokia.c" "ui.c" "ssd1306_i2c.c"
    
    INCLUDE_DIRS "." "../../main" 
    REQUIRES networking console aprs main
)
