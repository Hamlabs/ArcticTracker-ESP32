idf_component_register(
    SRCS "adc.c" "clock.c" "cmd_system.c" "config.c" "fbuf.c" 
         "gps.c" "main.c" "sr_frs.c" "sa868.c" "system.c" "tone.c" "radio.c"
    
    INCLUDE_DIRS "." "../components/ui" "../components/aprs" "../components/pmu"
)

spiffs_create_partition_image(webapp ../webapp FLASH_IN_PROJECT)
