idf_component_register(
    SRCS "clock.c" "cmd_system.c" "config.c" "fbuf.c" 
         "gps.c" "main.c" "system.c" "filesys.c"
    
    INCLUDE_DIRS "." "../components/ui" "../components/aprs" "../components/pmu" "../components/radio"
    REQUIRES afsk secutils nvs_flash esp_wifi spiffs spi_flash esp_adc esp_http_server fatfs esp_driver_gptimer esp_driver_uart
)

fatfs_create_rawflash_image(webapp ../webapp FLASH_IN_PROJECT PRESERVE_TIME)

