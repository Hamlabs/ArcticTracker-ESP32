/* Copyright (C) 2026 Øyvind Hanssen, LA7ECA
 *
 * Arctic Tracker - Monitor APRS traffic over UDP using syslog protocol
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


#include "esp_log.h"
#include "system.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include "defines.h"
#include "config.h"
#include "system.h"
#include "networking.h"
#include "ax25.h"
#include "aprs.h"



static const char *TAG="logmon";


static uint8_t subscribe(FBQ*, uint8_t*);
static void unsubscribe(FBQ*, uint8_t, uint8_t);
static int init_socket(struct sockaddr_in *dest_addr);


typedef struct workerctx {
    FBQ* mq; 
    bool mon_on;
} workerctx_t; 

static workerctx_t g_wrk = {0};
static FBQ g_mq;



/**************************************************************
 * Send a signal to wake up the worker, periodically. 
 **************************************************************/

static void tick(void *wParam) {
    
    workerctx_t *wrk = (workerctx_t*) wParam;
    while (wrk->mon_on) {
        sleepMs(60000);
        if (wrk->mon_on)
            fbq_signal(wrk->mq, SRC_RX);
    }
    vTaskDelete(NULL);
}


/**************************************************************
 * Worker thread
 **************************************************************/

static void logmon_worker(void *wParam)
{
    /* Get internet */
    struct sockaddr_in dest_addr;
    int sock = init_socket(&dest_addr);
    if (sock < 0)
        goto END;
    
    FBUF frame; 
    fbq_init(&g_mq, 10);
    uint8_t subscr, txsubscr = 255;
    subscr = subscribe(&g_mq, &txsubscr);
    char mycall[11]; 
    get_str_param("MYCALL", mycall, 10, DFL_MYCALL);
                
    /* Worker context to be shared with the other thread */
    g_wrk.mon_on = true;
    g_wrk.mq = &g_mq;
    xTaskCreate(tick, "logmon_tick", 1024, (void*) &g_wrk, 4, NULL); 
    
    while (g_wrk.mon_on)
    {
       /* Wait for frame. 
        */
        frame = fbq_get(&g_mq);
        if (!fbuf_empty(&frame)) {

            char pktbuf[256];
            int plen = ax25_frame2str(pktbuf, &frame);
            pktbuf[plen] = '\0';

            char metabuf[26];
            metabuf[0] = '\0';
            if (frame.meta != NULL) {
                lorameta_t *meta = (lorameta_t*) frame.meta;
                sprintf(metabuf, " / %ddBm / %ddB / %ldHz", meta->rssi, meta->snr, meta->ferror); 
            }
            
            /* Create log entry */
            char tbuf[21];
            char monbuf[350];
            int len = sprintf(monbuf, "<165>1 %s %s Arctic_Tracker - - - %s / %s %s\n", datetime2str_iso(tbuf, getTime()), mycall, 
                   (fbuf_getTag(&frame) == SRC_TRACKER ? "Tx":"Rx"), pktbuf, metabuf);
            
            /* Send a log-entry on UDP socket using the log format used for the lora-aprs.live service */
            if (len > 0) {
                if (sendto(sock, monbuf, len, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0)
                    ESP_LOGW(TAG, "sendto failed: errno=%d", errno);
            }
        
        }
            
        /* Dispose the frame */
        fbuf_release(&frame);    
    }
    
    /* Close down */
    unsubscribe(&g_mq, subscr, txsubscr);
    g_wrk.mq = NULL;
    
    END:
    if (sock > 0) 
        close(sock);
    vTaskDelete(NULL);
}



static char* getFromFrame(char* buf, char* frame) {
    char type = *(strchr(frame, ':')+1);
    char from[11];
    // TBD
    return buf;
}




/*************************************************************
 * Subscribe to incoming packets
 *************************************************************/

static uint8_t subscribe(FBQ* mq, uint8_t *txsubscr) {
    uint8_t subscription = APRS_SUBSCRIBE_RX(mq);
    if (GET_BOOL_PARAM("TXMON.on", DFL_TXMON_ON))
       *txsubscr = APRS_SUBSCRIBE_TXMON(mq);
    return subscription;
}


/*************************************************************
 * Unsubscribe
 *************************************************************/

static void unsubscribe(FBQ* mq, uint8_t subscr, uint8_t txsubscr) {
    fbq_signal(mq, SRC_RX);
    APRS_UNSUBSCRIBE_RX(subscr);
    if (txsubscr != 255)
        APRS_UNSUBSCRIBE_TXMON(txsubscr);
}



/*************************************************************
 * Wait for wifi and initialize socket and dest address
 *************************************************************/

static int init_socket(struct sockaddr_in *dest_addr) {
    
    
    uint16_t port = get_u16_param("LOGMON.PORT", DFL_LOGMON_PORT);
    char host[64];
    get_str_param("LOGMON.HOST", host, sizeof(host), DFL_LOGMON_HOST);

    wifi_enable(true);
    sleepMs(1000);
    while (!wifi_isConnected()) 
        sleepMs(10000);
    struct hostent *addr = gethostbyname(host);
    if (addr == NULL) {
        ESP_LOGW(TAG, "Failed DNS lookup for logmon host: %s", host);
        return -1;
    }

    int udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket: errno=%d", errno);
        return -1;
    }

    memset(dest_addr, 0, sizeof(dest_addr));
    dest_addr->sin_family = AF_INET;
    memcpy(&dest_addr->sin_addr.s_addr, addr->h_addr, sizeof(struct in_addr));
    dest_addr->sin_port = htons(port);
    ESP_LOGI(TAG, "Started logmon socket to %s:%d", host, port);
    return udp_sock;
}



/*************************************************************
 * Start/stop the logmon service
 *************************************************************/

void logmon_start() {
    xTaskCreate(logmon_worker, "logmon", 4096, NULL, 4, NULL);
}


void logmon_stop() {
    g_wrk.mon_on = false;
    if (g_wrk.mq != NULL)
        fbq_signal(g_wrk.mq, SRC_RX);
}


void logmon_init() {
    if (GET_BOOL_PARAM("LOGMON.on", false))
        logmon_start();
}
