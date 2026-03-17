idf_component_register (
    SRCS "radio.c" "adc.c" "tone.c" "sr_frs.c" "sa868.c" "spi.c" "lora1268.c"
    REQUIRES driver ui
    PRIV_REQUIRES esp_adc afsk 
    INCLUDE_DIRS "." "../../main" 
)

