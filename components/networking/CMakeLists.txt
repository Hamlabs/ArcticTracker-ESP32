idf_component_register(
    SRCS "cmd_networking.c" "tcpclient.c" "tcpserver.c" "wifi.c" "mdns.c"  "restapi.c" "api_test.c" "cuckoo_filter.c"
    
    INCLUDE_DIRS "." "../../main" 
    REQUIRES console ui mdns esp_http_client
)
