 
#if !defined __APRS_H__
#define __APRS_H__

#if defined(ARCTIC4_UHF)
char* loraprs_last_packet();
char* loraprs_last_heard(char* buf);
int8_t loraprs_last_rssi();
int8_t loraprs_last_snr();
time_t loraprs_last_time();
void loraprs_subscribe_rx(fbq_t* q, uint8_t i);
void loraprs_monitor_tx(fbq_t* q); 
fbq_t* loraprs_get_encoder_queue();
bool loraprs_tx_is_on(); 


#define APRS_SUBSCRIBE_RX(q, i) loraprs_subscribe_rx((q), (i))
#define APRS_MONITOR_TX(q) loraprs_monitor_tx((q))
#define APRS_GET_ENCODER_QUEUE loraprs_get_encoder_queue

#else
 
void hdlc_subscribe_rx(fbq_t* q, uint8_t i);
void hdlc_monitor_tx(fbq_t* q); 

#define APRS_SUBSCRIBE_RX(q, i) hdlc_subscribe_rx((q), (i))
#define APRS_MONITOR_TX(q) hdlc_monitor_tx((q))
#define APRS_GET_ENCODER_QUEUE hdlc_get_encoder_queue

#endif

#endif
