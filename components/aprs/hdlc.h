/*
 * HDLC (AX.25) frames encoding/decoding. 
 * By LA7ECA, ohanssen@acm.org and LA3T
 */

#if !defined __HDLC_H__
#define __HDLC_H__

#include "fbuf.h"
#include "freertos/ringbuf.h"

#define HDLC_FLAG 0x7E
#define MAX_HDLC_FRAME_SIZE 289 // including FCS field

void hdlc_wait_idle(void);
void hdlc_monitor_tx(FBQ* m);
void hdlc_test_on(uint8_t b);
void hdlc_test_off(void);
FBQ* hdlc_init_encoder(RingbufHandle_t oq);
fbq_t* hdlc_get_encoder_queue(void);
bool hdlc_enc_packets_waiting(void);
uint8_t rand_u8(void);

void hdlc_subscribe_rx(fbq_t* q, uint8_t i);
void hdlc_init_decoder (RingbufHandle_t s);

#endif
