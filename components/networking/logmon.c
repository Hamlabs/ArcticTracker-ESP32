

/* Monitor APRS traffic over UDP */
#include "esp_log.h"
#include "system.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "defines.h"
#include "config.h"
#include "networking.h"
#include "ax25.h"
#include "aprs.h"



static const char *TAG="logmon";

static bool mon_ax25 = true; 
static ServerInfo_t *srv = NULL;
static int clients = 0; 

static uint8_t subscribe(FBQ*, uint8_t*);
static void unsubscribe(FBQ*, uint8_t, uint8_t);


typedef struct workerctx {
    FBQ* mq; 
    bool mon_on;
} workerctx_t; 


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
    FBQ mq;
    FBUF frame; 
    fbq_init(&mq, 10);
    uint8_t subscr, txsubscr = 255;
    
    /* Worker context to be shared with the other thread */
    workerctx_t wrk; 
    wrk.mon_on = true;
    wrk.mq = &mq;

    subscr = subscribe(&mq, &txsubscr);
    xTaskCreate(tick, "netmon_tick", 1024, (void*) &wrk, 4, NULL); 
    while (wrk.mon_on)
    {
       /* Wait for frame. 
        */
        frame = fbq_get(&mq);
        if (!fbuf_empty(&frame)) {
                
            // Send a log-entry on UDP socket using the log format used for the lora-aprs.live service
            // Include metainformation: RSSI and SNR
        }
            
        /* Dispose the frame */
        fbuf_release(&frame);    
    }
    
    /* Close down */
    unsubscribe(&mq, subscr, txsubscr);  |
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
 * Start/stop the netmon service
 *************************************************************/

void logmon_start() {
    uint16_t port = get_u16_param("LOGMON.PORT", DFL_LOGMON_PORT);
    // Create UDP socket
    // Start worker thread (logmon_worker)
}


void logmon_stop() {
    // Stop worker thread by setting the wrk.mon_on to false
    // Close UDP socket
}


void logmon_init() {
    if (GET_BOOL_PARAM("LOGMON.on", false))
        logmon_start();
}








