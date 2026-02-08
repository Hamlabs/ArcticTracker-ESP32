idf_component_register(
    SRCS "afsk_rxsampler.c" "afsk_rx.c" "afsk_tx.c" "afsk.c" "fifo.c" "hdlc_decoder.c" "hdlc_encoder.c"
             
    INCLUDE_DIRS "." "../../main" "../radio"
    REQUIRES radio
)
