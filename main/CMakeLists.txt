idf_component_register(
    SRCS "adc.c" "clock.c" "cmd_aprs.c" "cmd_networking.c" "cmd_system.c" "config.c" "fbuf.c" 
         "gps.c" "httpd.c" "main.c" "sr_frs.c" "system.c" "tcpclient.c" "tcpserver.c" "tone.c"
         "tracker.c" "tracklogger.c" "trackstore.c" "wifi.c" "xreport.c"
    
    INCLUDE_DIRS "." "../components/ui" "../components/aprs"
)
