idf_component_register (
    SRCS "cmd_networking.c" "tcpclient.c" "wifi.c" "mdns.c"  "restapi.c" "api_rest.c" "fileserver.c" "security.c" "cuckoo_filter.c"
    
    INCLUDE_DIRS "." "../../main" 
    REQUIRES console ui mdns esp_http_client
    
    EMBED_TXTFILES "certs/cert.pem"
               "certs/privkey.pem"
)

