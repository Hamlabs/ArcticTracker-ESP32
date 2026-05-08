/* Monitor APRS traffic over UDP */
#include "esp_log.h"
#include "system.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include "defines.h"
#include "config.h"
#include "networking.h"
#include "ax25.h"
#include "aprs.h"



static const char *TAG="logmon";

static int udp_sock = -1;
static struct sockaddr_in dest_addr;

static uint8_t subscribe(FBQ*, uint8_t*);
static void unsubscribe(FBQ*, uint8_t, uint8_t);


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
    int sock = *((int*) wParam);
    FBUF frame; 
    fbq_init(&g_mq, 10);
    uint8_t subscr, txsubscr = 255;
    
    /* Worker context to be shared with the other thread */
    g_wrk.mon_on = true;
    g_wrk.mq = &g_mq;

    subscr = subscribe(&g_mq, &txsubscr);
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

            int8_t rssi = 0, snr = 0;
            if (frame.meta != NULL) {
                lorameta_t *meta = (lorameta_t*) frame.meta;
                rssi = meta->rssi;
                snr = meta->snr;
            }

            /* Extract 'from' callsign (text before '>') */
            char from[10] = "";
            int i;
            for (i = 0; i < plen && pktbuf[i] != '>' && i < (int)sizeof(from)-1; i++)
                from[i] = pktbuf[i];
            from[i] = '\0';

            /* Send a log-entry on UDP socket using the log format used for the lora-aprs.live service */
            char json[512];
            int jlen = snprintf(json, sizeof(json),
                "{\"software\":{\"name\":\"ArcticTracker\",\"version\":\"%s\"},"
                "\"hardware\":{\"name\":\"%s\"},"
                "\"data\":{\"type\":\"aprs-lora\",\"from\":\"%s\","
                "\"data\":\"%s\",\"rssi\":%d,\"snr\":%d,\"freq_offset\":0}}",
                VERSION_SSTRING, DEVICE_STRING, from, pktbuf, rssi, snr);
            if (jlen > 0 && jlen < (int)sizeof(json))
                sendto(sock, json, jlen, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }
            
        /* Dispose the frame */
        fbuf_release(&frame);    
    }
    
    /* Close down */
    unsubscribe(&g_mq, subscr, txsubscr);
    g_wrk.mq = NULL;
    vTaskDelete(NULL);
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
 * Start/stop the logmon service
 *************************************************************/

void logmon_start() {
    uint16_t port = get_u16_param("LOGMON.PORT", DFL_LOGMON_PORT);
    char host[64];
    get_str_param("LOGMON.HOST", host, sizeof(host), DFL_LOGMON_HOST);

    struct hostent *addr = gethostbyname(host);
    if (addr == NULL) {
        ESP_LOGE(TAG, "Failed DNS lookup for logmon host: %s", host);
        return;
    }

    udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket: errno=%d", errno);
        return;
    }

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    memcpy(&dest_addr.sin_addr.s_addr, addr->h_addr, sizeof(struct in_addr));
    dest_addr.sin_port = htons(port);

    ESP_LOGI(TAG, "Starting logmon to %s:%d", host, port);
    xTaskCreate(logmon_worker, "logmon", 3072, (void*) &udp_sock, 4, NULL);
}


void logmon_stop() {
    g_wrk.mon_on = false;
    if (g_wrk.mq != NULL)
        fbq_signal(g_wrk.mq, SRC_RX);
    if (udp_sock >= 0) {
        close(udp_sock);
        udp_sock = -1;
    }
}


void logmon_init() {
    if (GET_BOOL_PARAM("LOGMON.on", false))
        logmon_start();
}
