 
#if !defined __APRS_H__
#define __APRS_H__

#define MAX_SUBSCRIBE_CHAN 3

#if defined(ARCTIC4_UHF)
char* loraprs_last_packet();
char* loraprs_last_heard(char* buf);
int8_t loraprs_last_rssi();
int8_t loraprs_last_snr();
time_t loraprs_last_time();
uint8_t loraprs_subscribe_rx(fbq_t* q);
void loraprs_unsubscribe_rx(uint8_t i);
uint8_t loraprs_subscribe_txmon(fbq_t* q);
void loraprs_unsubscribe_txmon(uint8_t i);
fbq_t* loraprs_get_encoder_queue();
bool loraprs_tx_is_on(); 


#define APRS_SUBSCRIBE_RX(q) loraprs_subscribe_rx((q))
#define APRS_UNSUBSCRIBE_RX(i) loraprs_unsubscribe_rx((i))
#define APRS_SUBSCRIBE_TXMON(q) loraprs_subscribe_txmon((q))
#define APRS_UNSUBSCRIBE_TXMON(i) loraprs_unsubscribe_txmon((i))
#define APRS_GET_ENCODER_QUEUE loraprs_get_encoder_queue

#else
 
uint8_t hdlc_subscribe_rx(fbq_t* q);
void hdlc_unsubscribe_rx(uint8_t i);
uint8_t hdlc_subscribe_txmon(fbq_t* q);
void hdlc_unsubscribe_txmon(uint8_t i);

#define APRS_SUBSCRIBE_RX(q) hdlc_subscribe_rx((q))
#define APRS_UNSUBSCRIBE_RX(i) hdlc_unsubscribe_rx((i))
#define APRS_SUBSCRIBE_TXMON(q) hdlc_subscribe_txmon((q))
#define APRS_UNSUBSCRIBE_TXMON(i) hdlc_unsubscribe_txmon((i))
#define APRS_GET_ENCODER_QUEUE hdlc_get_encoder_queue

#endif

#endif
