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
static FBQ mq;
static FBUF frame; 
static bool inited, mon_on;
static bool mon_ax25 = true; 
static ServerInfo_t *srv = NULL;
static int clients = 0; 

static void subscribe();
static void unsubscribe();

static void tick(void *wParam) {
    while (mon_on) {
        sleepMs(60000);
        fbq_signal(&mq, SRC_RX);
    }
    vTaskDelete(NULL);
}


void netmon_worker(void *wParam)
{
    int sock = *((int*) wParam);
    FILE *f = fdopen(sock, "r+");
    char mycall[12];
    get_str_param("MYCALL", mycall, 10, DFL_MYCALL);
    fprintf(f, "# ArcticTracker Monitor: %s\n", mycall);
    fflush(f);
    clients++;
    if (clients == 1) {
        subscribe();
        mon_on = true;
        xTaskCreate(tick, "netmon_tick", 1024, (void*) NULL, 4, NULL); 
        while (mon_on)
        {
            /* Wait for frame and then to AFSK decoder/encoder 
            * is not running. 
            */
            frame = fbq_get(&mq);
            if (!fbuf_empty(&frame)) {
                /* Display it */
                if (mon_ax25)
                    ax25_display_frame(f, &frame);
                else 
                    fbuf_print(f, &frame);
                fprintf(f, "\n");
                sleepMs(10);
            }
            else {
                char buf[16];
                fprintf(f, "# %s\n", datetime2str(buf, getTime(), false)); 
            }
            
            /* Flush and dispose the frame */
            fbuf_release(&frame);    
            if (fflush(f) == EOF)
                mon_on = false;
        }
        /* Close down */
        unsubscribe();
    }
    else
        fprintf(f, "# Max 1 client allowed\n");
    fflush(f);
    sleepMs(100);
    clients--;
    fclose(f); 
    close(sock);
    vTaskDelete(NULL);
}




static void subscribe() {
    APRS_SUBSCRIBE_RX(&mq, 3);
    if (GET_BOOL_PARAM("TXMON.on", DFL_TXMON_ON))
        APRS_MONITOR_TX(&mq);
}


static void unsubscribe() {
    fbq_signal(&mq, SRC_RX);
    APRS_SUBSCRIBE_RX(NULL, 3);
    APRS_MONITOR_TX(NULL);
}



void netmon_start() {
    if (!inited) {
        fbq_init(&mq, HDLC_DECODER_QUEUE_SIZE);
        inited = true;
    }
    uint16_t port = get_u16_param("NETMON.PORT", DFL_NETMON_PORT);
    srv = tcpserver_start(port, &netmon_worker, 4096, "netmon");
}


void netmon_stop() {
    tcpserver_stop(srv);
}








