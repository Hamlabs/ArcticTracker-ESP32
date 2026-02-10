idf_component_register(
    SRCS "encryption.c" "base91.c" "micro_aes.c"
    INCLUDE_DIRS "." "../aprs" "../../main"
    REQUIRES mbedtls
)
