

/* Monitor APRS traffic over TCP */
#include "esp_log.h"
#include "system.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "defines.h"
#include "config.h"
#include "networking.h"
#include "ax25.h"
#include "aprs.h"



static const char *TAG="tcpserver";
static bool mon_ax25 = true; 
static ServerInfo_t *srv = NULL;
static int clients = 0; 
static mutex_t cnt_mutex;

static uint8_t subscribe(FBQ*, uint8_t*);
static void unsubscribe(FBQ*, uint8_t, uint8_t);


typedef struct workerctx {
    FBQ* mq; 
    bool mon_on;
} workerctx_t; 


/**************************************************************
 * Send a signal to wake up the worker, periodically. 
 * FIXME: We may use a timer instead of a thread? 
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

static void netmon_worker(void *wParam)
{
    /* Note that sock is passed by value by casting it to an intptr_t */
    int sock = (int)(intptr_t) wParam;
    FILE *f = fdopen(sock, "r+");
    char mycall[12];
    FBQ mq;
    FBUF frame; 
    fbq_init(&mq, 10);
    uint8_t subscr, txsubscr = 255;
    
    /* Worker context to be shared with the other thread */
    workerctx_t wrk; 
    wrk.mon_on = true;
    wrk.mq = &mq;

    get_str_param("MYCALL", mycall, 10, DFL_MYCALL);
    fprintf(f, "# ArcticTracker Monitor: %s\n", mycall);
    fflush(f);

    mutex_lock(cnt_mutex);
    int cli = ++clients;
    mutex_unlock(cnt_mutex);

    if (cli <= 5) {
        char buf[16];
        subscr = subscribe(&mq, &txsubscr);
        xTaskCreate(tick, "netmon_tick", 1024, (void*) &wrk, 4, NULL); 
        while (wrk.mon_on)
        {
            /* Wait for frame and then to AFSK decoder/encoder 
            * is not running. 
            */
            frame = fbq_get(&mq);
            if (!fbuf_empty(&frame)) {
                
                /* Display metainformation */
                fprintf(f, "# src=%s", fbuf_showtag(buf, &frame));

#if defined(ARCTIC4_UHF)
                if (frame.meta != NULL) {
                    lorameta_t *meta = (lorameta_t*) frame.meta;
                    fprintf(f,", rssi=%d dBm, snr=%d dB, ferr=%ld Hz", meta->rssi, meta->snr, meta->ferror);
                }
#endif

                fprintf(f, "\n");
                
                /* Display frame */
                if (mon_ax25)
                    ax25_display_frame(f, &frame);
                else 
                    fbuf_print(f, &frame);
                fprintf(f, "\n");
                sleepMs(10);
            }
            else {
                fprintf(f, "# %s\n", datetime2str(buf, getTime(), false)); 
            }
            
            /* Flush and dispose the frame */
            fbuf_release(&frame);    
            if (fflush(f) == EOF)
                wrk.mon_on = false;
        }
        /* Close down */
        unsubscribe(&mq, subscr, txsubscr);  
    }
    else
        fprintf(f, "# Max 5 clients allowed\n");
    fflush(f);
    sleepMs(500);
    mutex_lock(cnt_mutex);
    clients--;
    mutex_unlock(cnt_mutex);
    fclose(f); 
    close(sock);
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

void netmon_start() {
    uint16_t port = get_u16_param("NETMON.PORT", DFL_NETMON_PORT);
    srv = tcpserver_start(port, &netmon_worker, 3072, "netmon");
}


void netmon_stop() {
    tcpserver_stop(srv);
}


void netmon_init() {
    cnt_mutex = mutex_create();
    if (GET_BOOL_PARAM("NETMON.on", false))
        netmon_start();
}








