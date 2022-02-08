idf_component_register(
    SRCS "afsk_rx.c" "afsk_tx.c" "afsk.c" "ax25.c" "cmd_aprs.c" "digipeater.c" "fifo.c" "hdlc_decoder.c" "hdlc_encoder.c"
         "heardlist.c" "igate.c" "monitor.c" "tracker.c" "tracklogger.c" "trackstore.c" "xreport.c"
    
    INCLUDE_DIRS "." "../../main" 
    REQUIRES mbedtls console ui
)
