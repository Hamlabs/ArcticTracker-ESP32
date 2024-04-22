idf_component_register (
    SRCS "radio.c" "adc.c" "tone.c" "sr_frs.c"  "sa868.c"
    REQUIRES driver aprs
    INCLUDE_DIRS "." "../../main" 
    
)

