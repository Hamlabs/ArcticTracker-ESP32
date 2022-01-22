idf_component_register(
    SRCS "cmd_networking.c" "httpd.c" "tcpclient.c" "tcpserver.c" "wifi.c"
    
    INCLUDE_DIRS "." "../../main" 
    REQUIRES console ui espfs libesphttpd
)
