 
#if !defined __APRS_H__
#define __APRS_H__

#if defined(ARCTIC4_UHF)

void loraprs_subscribe_rx(fbq_t* q, uint8_t i);
#define APRS_SUBSCRIBE_RX(q, i) loraprs_subscribe_rx((q), (i))

#else
 
void hdlc_subscribe_rx(fbq_t* q, uint8_t i);
#define APRS_SUBSCRIBE_RX(q, i) loraprs_subscribe_rx((q), (i))

#endif

#endif
