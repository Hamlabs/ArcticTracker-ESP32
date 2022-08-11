idf_component_register(
    SRCS "cmd_networking.c" "httpd.c" "tcpclient.c" "tcpserver.c" "wifi.c" "restapi.c" "api_test.c" "cuckoo_filter.c"
    
    INCLUDE_DIRS "." "../../main" 
    REQUIRES console ui espfs libesphttpd mdns esp_http_client
)
