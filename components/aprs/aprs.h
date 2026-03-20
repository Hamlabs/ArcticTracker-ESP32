 
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
void loraprs_monitor_tx(fbq_t* q); 
fbq_t* loraprs_get_encoder_queue();
bool loraprs_tx_is_on(); 


#define APRS_SUBSCRIBE_RX(q) loraprs_subscribe_rx((q))
#define APRS_UNSUBSCRIBE_RX(i) loraprs_unsubscribe_rx((i))
#define APRS_MONITOR_TX(q) loraprs_monitor_tx((q))
#define APRS_GET_ENCODER_QUEUE loraprs_get_encoder_queue

#else
 
uint8_t hdlc_subscribe_rx(fbq_t* q);
void hdlc_unsubscribe_rx(uint8_t i);
void hdlc_monitor_tx(fbq_t* q); 

#define APRS_SUBSCRIBE_RX(q) hdlc_subscribe_rx((q))
#define APRS_UNSUBSCRIBE_RX(i) hdlc_unsubscribe_rx((i))
#define APRS_MONITOR_TX(q) hdlc_monitor_tx((q))
#define APRS_GET_ENCODER_QUEUE hdlc_get_encoder_queue

#endif

#endif
