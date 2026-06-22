/* 
 * Copyright (C) 2026 Øyvind Hanssen, LA7ECA
 *
 * Arctic Tracker - Implement LoRa APRS sending and receiving
 *
 * Arctic Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details: 
 * <https://www.gnu.org/licenses/>.
 */

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
#include "aprs.h"

#define TAG "lora-aprs"

#define CAD_RETRIES  5

                         // SF    5     6     7     8     9    10     11     12
                         //--------------------------------------------------------
const uint16_t cad_min_delay[] = {4,    6,   10,   18,   33,   61,   111,   200 };
const uint16_t cad_max_delay[] = {40,  60,  100,  180,  330,  610,  1110,  2000 };

static uint8_t cur_sf = 12;

FBQ encoder_queue; 

static FBUF frame; 
static cond_t packet_rdy;
static cond_t cad_done;
static volatile bool cad_detected;
static FBQSW_t *psub, *psubtx;

char  last_packet[256]; 
int8_t last_rssi;
int8_t last_snr;
int32_t last_ferror;
time_t last_time; 

bool txon = false; 


static void alt_setting(bool on, FBUF *frame);
static void cad_wait();

lorameta_t *loraprs_meta(int8_t rssi, int8_t snr, int32_t ferr) {
    lorameta_t * x = malloc(sizeof(lorameta_t));
    x->rssi = rssi;
    x->snr = snr;
    x->ferror = ferr;
    return x;
}

   
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


/* Return true if last heard packet was via digipeater. */
bool loraprs_last_digied() {
    const char* hstart = strchr(last_packet, '>');
    if (hstart == NULL)
        return false;

    const char* hend = strchr(hstart + 1, ':');
    if (hend == NULL)
        return false;

    for (const char* p = hstart + 1; p < hend; p++) {
        if (*p == '*') {
            /* Find the start of the current token (delimited by ',' or '>') */
            const char* tok = p - 1;
            while (tok > hstart && *tok != ',' && *tok != '>')
                tok--;
            tok++;
            /* Ignore TCPIP*: packet was relayed via internet, not digipeated */
            if (strncasecmp(tok, "TCPIP", 5) == 0)
                continue;
            return true;
        }
    }
    return false;
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


uint8_t loraprs_subscribe_rx(fbq_t* q) {
    return fbqsw_subscribe(psub, q);
}

void loraprs_unsubscribe_rx(uint8_t i) {
    fbqsw_unsubscribe(psub, i);
}

uint8_t loraprs_subscribe_txmon(fbq_t* q) {
    return fbqsw_subscribe(psubtx, q);
}

void loraprs_unsubscribe_txmon(uint8_t i) {
    fbqsw_unsubscribe(psubtx, i);
}


/***********************************************************
 * Interrupt handler - attached to DIO1 on LoRa module
 *  - RX packet ready
 *  - TX packet sent
 ***********************************************************/

static void IRAM_ATTR intrHandler(void* x) {
    cond_setI(packet_rdy);
}



/************************************************************
 * Main receiver thread 
 ************************************************************/

static void rxdecoder (void* arg) {
    uint8_t buf[256];
    int16_t len = 0;
    int8_t rssi, sigrssi;
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
        
        /* CAD done: relay result to txencoder */
        if (irqRegs & SX126X_IRQ_CAD_DONE) {
            cad_detected = (irqRegs & SX126X_IRQ_CAD_DETECTED) != 0;
            lora_ClearIrqStatus(SX126X_IRQ_ALL);
            cond_set(cad_done);
            continue;
        }
        
        len = lora_ReceivePacket(buf, 200);
        buf[len] = '\0';
        lora_GetPacketStatus(&rssi, &sigrssi, &snr);
        long ferror = lora_GetFreqError(125);
        lora_ClearIrqStatus(SX126X_IRQ_ALL);
        
        if (len == 0) {
            ESP_LOGW(TAG, "Packet not ready. 0 bytes returned");
            continue;
        }
        if (len < 0) {
            ESP_LOGI(TAG, "CRC error");
            continue;
        }
        
        rssi = rssi-LORA_LNA_GAIN;
        
        ESP_LOGI(TAG, "RX packet: len=%d, rssi=%d, signal-rssi=%d, snr=%d, freqd=%ld",
                 len, rssi, (snr<0 ? rssi+snr: rssi), snr, ferror);
        
        /* Is it APRS? */
        if (len < 20 || buf[0]!= '<' || buf[1]!= 0xff || buf[2]!= 0x01) {
            ESP_LOGW(TAG, "Packet is not LoRa APRS");
            continue;
        }
        
        fbuf_new(&frame, SRC_RX);
        frame.meta = loraprs_meta(rssi, snr, ferror);
        ax25_str2frame(&frame,  (char*) buf+3, len-3);
        strcpy(last_packet, (char*) buf+3);
        last_rssi = rssi; last_snr = snr;
        last_time = getTime();
        last_ferror = ferror;

        /* Distribute packet to subscribers */
        if (fbqsw_publish(psub, frame) == 0)
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
     int len = ax25_frame2str(txbuf+3, sizeof(txbuf)-3, &frame);
     alt_setting(true, &frame); 
     
      /* CAD: check the channel is free before transmitting */
     cad_wait();
     
     ESP_LOGI(TAG, "TX packet: %d bytes", len);
     tx_led_on();
     txon = true;
     lora_SendPacket((uint8_t*) txbuf, len+3);
     
     /* Distribute packet to subscribers */
     if (fbqsw_publish(psubtx, frame) == 0)
        fbuf_release(&frame);

  }
}

/*******************************************************************************
 * CAD: check the channel is free before transmitting 
 *******************************************************************************/

static void cad_wait() {
    int cad_min = cad_min_delay[cur_sf - 5];
    int cad_max = cad_max_delay[cur_sf - 5];
    
    bool channel_free = false;
    for (int retry = 0; retry < CAD_RETRIES; retry++) {
        cond_clear(cad_done);
        lora_SetCad();
        cond_wait(cad_done);
        if (!cad_detected) {
            channel_free = true;
            break;
        }
        ESP_LOGI(TAG, "CAD: channel busy, retry %d/%d", retry+1, CAD_RETRIES);
        lora_TxOff();   /* back to RX while waiting */
        sleepMs(cad_min + (rand() % (cad_max - cad_min + 1)));
    }
    if (!channel_free)
        ESP_LOGW(TAG, "CAD: channel busy after %d retries, transmitting anyway", CAD_RETRIES);

}


/*******************************************************************************
 * Switch to/from alternative SF/CR setting
 *******************************************************************************/

static void alt_setting(bool on, FBUF *frame) {
    if (!GET_BOOL_PARAM("LORA_ALT.on", DFL_LORA_ALT_ON) 
        || (frame != NULL && frame->tag != SRC_DIGIPEATER))
        return; 
        
    uint8_t cr, sf; 
    if (on) {
        sf = get_byte_param("LORA_ALT_SF", DFL_LORA_ALT_SF);
        cr = get_byte_param("LORA_ALT_CR", DFL_LORA_ALT_CR);
        ESP_LOGD(TAG, "Switching to alternative setting: sf=%d, cr=%d", sf, cr);
        cur_sf = sf;
    }
    else {
        sf = get_byte_param("LORA_SF", DFL_LORA_SF);
        cr = get_byte_param("LORA_CR", DFL_LORA_CR);
        ESP_LOGD(TAG, "Switching to normal setting: sf=%d, cr=%d", sf, cr);
        cur_sf = sf;
    }
    lora_SetModulationParams(sf, SX126X_LORA_BW_125_0, cr-4, (sf>=11 ? 1:0)); 
}




/*************************************************************
 * Initialize lora aprs decoder
 *************************************************************/

void loraprs_init_decoder() 
{
    cur_sf = get_byte_param("LORA_SF", DFL_LORA_SF);
    radio_require();
    packet_rdy = cond_create();
    cond_clear(packet_rdy);
    cad_done = cond_create();
    cond_clear(cad_done);
    
    psub = fbqsw_create(10);
    lora_SetIrqHandler(intrHandler, SX126X_IRQ_RX_DONE | SX126X_IRQ_TX_DONE | 
                                    SX126X_IRQ_CAD_DONE);
    
    /* Start RX thread */
    xTaskCreatePinnedToCore(&rxdecoder, "LoRa APRS RX", 
        STACK_LORA_RXDECODER, NULL, NORMALPRIO, NULL, CORE_LORA_RXDECODER);

}


/*************************************************************
 * Initialize lora aprs encoder
 *************************************************************/

FBQ* loraprs_init_encoder() 
{
  psubtx = fbqsw_create(10);
  fbq_init(&encoder_queue, LORA_ENCODER_QUEUE_SIZE);
  /* Start TX thread */
  xTaskCreatePinnedToCore(&txencoder, "LoRa APRS TX", 
     STACK_LORA_TXENCODER, NULL, NORMALPRIO, NULL, CORE_LORA_TXENCODER);
  return &encoder_queue; 
}

#endif














