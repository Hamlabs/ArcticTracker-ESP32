idf_component_register(
    SRCS "ax25.c" "cmd_aprs.c" "digipeater.c" "heardlist.c" "igate.c" "monitor.c" 
    "tracker.c" "tracklogger.c" "trackstore.c" "xreport.c" "lora_aprs.c"
    
    INCLUDE_DIRS "." "../../main" "../radio"
    REQUIRES mbedtls console ui radio
)
