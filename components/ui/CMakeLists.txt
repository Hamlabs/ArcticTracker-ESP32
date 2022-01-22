idf_component_register(
    SRCS "buzzer.c" "commands.c" "cmd_register.c" "gui_menu.c" "gui_status.c" "gui.c" "lcd.c" "ui.c"
    
    INCLUDE_DIRS "." "../../main" 
    REQUIRES networking console aprs
)
