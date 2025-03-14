#include "defines.h"

#if defined(ARCTIC4_UHF)
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "ax25.h"
#include "hdlc.h"
#include "system.h"
#include "radio.h"
#include "lora1268.h"
#include "ui.h"

#define TAG "lora-aprs"

static FBQ* mqueue[3];
FBQ encoder_queue; 

static FBQ mon;
static FBQ *txmon = NULL; 
static FBUF frame; 
static cond_t packet_rdy;

char  last_packet[256]; 
int8_t last_rssi;
int8_t last_snr;
time_t last_time; 

bool txon = false; 


static void alt_setting(bool on, FBUF *frame);


/* Set packet queue for TX monitoring */
void loraprs_monitor_tx(FBQ* m)
   { txmon = m; }
   
   
/* Return queue for packets to TX */
fbq_t* loraprs_get_encoder_queue()
   { return &encoder_queue; }
   
   
/* Return last heard packet */
char* loraprs_last_packet() {
    return last_packet;
}

/* Return last heard callsign */
char* loraprs_last_heard(char* buf) {
    int pos =  strcspn(last_packet, ">");
    strncpy(buf, last_packet, pos);
    buf[pos] = '\0';
    return buf;
}
   
int8_t loraprs_last_rssi() {
    return last_rssi;
}

int8_t loraprs_last_snr() {
    return last_snr;
}

time_t loraprs_last_time() {
    return last_time;
}

bool loraprs_tx_is_on() {
    return txon;
}


/***********************************************************
 * Subscribe or unsubscribe to packets from decoder
 * packets are put into the given frame queue.
 ***********************************************************/
 
void loraprs_subscribe_rx(fbq_t* q, uint8_t i)
{
    if (i > 2)
        return;
    if (mqueue[i] != NULL)
        fbq_clear(mqueue[i]);
    mqueue[i] = q;
}



/***********************************************************
 * Interrupt handler - attached to DIO1 on LoRa module
 *  - RX packet ready
 *  - TX packet sent
 ***********************************************************/

static void IRAM_ATTR intrHandler(void* x) {
    cond_setI(packet_rdy);
}




/* Main receiver thread */
static void rxdecoder (void* arg) {
    uint8_t buf[256];
    int16_t len = 0;
    int8_t rssi; 
    int8_t snr;
    FBUF frame;
    
    
    ESP_LOGI(TAG, "RX decoder thread");
    while (true) {
        cond_wait(packet_rdy);
        cond_clear(packet_rdy);
        uint16_t irqRegs = lora_GetIrqStatus();
        
        /* Transmitting packet is done. Turn off TX */
        if (irqRegs & SX126X_IRQ_TX_DONE) {
            lora_ClearIrqStatus(SX126X_IRQ_ALL);
            lora_TxOff();
            tx_led_off();
            txon = false; 
            alt_setting(false, NULL); 
            continue;
        }
        
        len = lora_ReceivePacket(buf, 200);
        lora_ClearIrqStatus(SX126X_IRQ_ALL);
        if (len == 0)
            continue;
        lora_GetPacketStatus(&rssi, &snr);
        ESP_LOGI(TAG, "RX packet: len=%d, rssi=%d, snr=%d", len, rssi, snr);
        
        /* Is it APRS? */
        if (len < 20 || buf[0]!= '<' || buf[1]!= 0xff || buf[2]!= 0x01) {
            ESP_LOGW(TAG, "Packet is not LoRa APRS");
            continue;
        }
        
        fbuf_new(&frame, SRC_RX);
        ax25_str2frame(&frame,  (char*) buf+3, len-3);
        strcpy(last_packet, (char*) buf+3);
        last_rssi = rssi; last_snr = snr;
        last_time = getTime();
        
        if (mqueue[0] || mqueue[1] || mqueue[2]) { 
            if (mqueue[0]) fbq_put( mqueue[0], frame);                       // Monitor 
            if (mqueue[1]) fbq_put( mqueue[1], fbuf_newRef(&frame, SRC_RX)); // Digipeater 
            if (mqueue[2]) fbq_put( mqueue[2], fbuf_newRef(&frame, SRC_RX)); // Igate 
        }
        if (mqueue[0]==NULL)
            fbuf_release(&frame); 
    }
}


/*******************************************************************************
 * TX encoder thread
 *
 * This function gets a frame from frame-queue, and starts the transmitter
 * as soon as the channel is free.   
 *******************************************************************************/

static void txencoder (void* arg)
{ 
  (void)arg;
  char txbuf[256];
  ESP_LOGI(TAG, "TX encoder thread");
  while (true) {
     /* Get frame from frame-queue when available. 
      * This is a blocking call.
      */  
     frame = fbq_get(&encoder_queue); 
     
     /* Now send it */
     txbuf[0]='<'; 
     txbuf[1]=0xFF;
     txbuf[2]=0x01;
     int len = ax25_frame2str(txbuf+3, &frame);    
     
     alt_setting(true, &frame); 
     ESP_LOGI(TAG, "TX packet: %d bytes", len);
     tx_led_on();
     txon = true;
     lora_SendPacket((uint8_t*) txbuf, len+3);

     
     if (txmon != NULL)
        fbq_put(txmon, frame);
     else
        fbuf_release(&frame);
  }
}


static void alt_setting(bool on, FBUF *frame) {
    if (!GET_BOOL_PARAM("LORA_ALT.on", DFL_LORA_ALT_ON) 
        || (frame != NULL && frame->tag != SRC_DIGIPEATER))
        return; 
        
    uint8_t cr, sf; 
    if (on) {
        sf = get_byte_param("LORA_ALT_SF", DFL_LORA_ALT_SF);
        cr = get_byte_param("LORA_ALT_CR", DFL_LORA_ALT_CR);
        ESP_LOGD(TAG, "Switching to alternative setting: sf=%d, cr=%d", sf, cr);
    }
    else {
        sf = get_byte_param("LORA_SF", DFL_LORA_SF);
        cr = get_byte_param("LORA_CR", DFL_LORA_CR);
        ESP_LOGD(TAG, "Switching to normal setting: sf=%d, cr=%d", sf, cr);
    }
    lora_SetModulationParams(sf, SX126X_LORA_BW_125_0, cr-4, (sf>=11 ? 1:0)); 
}



/*************************************************************
 * Initialize lora aprs decoder
 *************************************************************/

void loraprs_init_decoder() 
{
    radio_require();
    packet_rdy = cond_create();
    cond_clear(packet_rdy);
    
    mqueue[0] = mqueue[1] = mqueue[2] = NULL;
    fbq_init(&mon, 10);    
    lora_SetIrqHandler(intrHandler, SX126X_IRQ_RX_DONE | SX126X_IRQ_TX_DONE);
    
    /* Start RX thread */
    xTaskCreatePinnedToCore(&rxdecoder, "LoRa APRS RX", 
        STACK_LORA_RXDECODER, NULL, NORMALPRIO, NULL, CORE_LORA_RXDECODER);

}


/*************************************************************
 * Initialize lora aprs encoder
 *************************************************************/

FBQ* loraprs_init_encoder() 
{
  fbq_init(&encoder_queue, LORA_ENCODER_QUEUE_SIZE);
  /* Start TX thread */
  xTaskCreatePinnedToCore(&txencoder, "LoRa APRS TX", 
     STACK_LORA_TXENCODER, NULL, NORMALPRIO, NULL, CORE_LORA_TXENCODER);
  return &encoder_queue; 
}

#endif















