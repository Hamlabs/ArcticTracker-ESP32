
/*
 * interface: 
 *  igate_init()
 *  igate_on()
 *  igate_off()
 *  igate_status()
 * 
 * Packet queue from receiver and from tracker 
 * 
 * Do we want RF transmission? 
 * If so, packet queue to transmitter
 *
 * Config: 
 *   IGATE_ON
 *   IGATE_HOST
 *   IGATE_PORT
 *   IGATE_PASSCODE
 *   IGATE_FILTER 
 * 
 * Add to config (later?):
 *   IGATE_DIGIPATH
 *   IGATE_RF_ON  
 *   IGATE_OBJ_RADIUS 
 */ 

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "system.h"
#include "defines.h"
#include "config.h"
#include "ui.h"
#include "fbuf.h"
#include "ax25.h"
#include "hdlc.h"
#include "afsk.h"
#include "radio.h"
#include "heardlist.h"
#include "tracker.h"
#include "igate.h"
#include "networking.h"


#define INET_NAME_LENGTH 64
#define CRED_LENGTH 32
#define FRAME_LEN 256

static void rf2inet(FBUF *);
static void inet2rf(char *);

static bool _igate_on = false;
static bool _igate_run = false; 
static uint32_t _icount = 0;
static uint32_t _rcvd = 0;
static uint32_t _tracker_icount = 0;


static FBQ rxqueue;           /* Frames from radio or tracker */
extern fbq_t* outframes;      /* Frames to be transmitted on radio */


#define TAG "igate"


bool igate_is_on() 
  { return _igate_on; }


uint32_t igate_icount()
  { return _icount; }
  
uint32_t igate_rxcount()
  { return _rcvd; }
  
uint32_t igate_tr_count()
  { return _tracker_icount; }
  
  
/********************************************
 * Radio thread
 * Listen for incoming packets from radio 
 * (or outgoing packets from tracker)
 ********************************************/

static void igate_radio(void* arg)
{
    sleepMs(100); 
    while(_igate_run) {
        FBUF frame = fbq_get(&rxqueue);
        if (fbuf_length(&frame) > 2) {
            _rcvd++;
            rf2inet(&frame);
        }   
        fbuf_release(&frame);
        sleepMs(100);
    }
    vTaskDelete(NULL);
}


/*******************************************
 * Igate main thread. 
 *  connect to aprs-is server
 *  listen for incoming data from server.
 *******************************************/

static void igate_main(void* arg)
{
    char host[INET_NAME_LENGTH]; 
    uint16_t port;
    char frame[FRAME_LEN];
    sleepMs(1000);
    ESP_LOGI(TAG, "Main thread started..");
    
    while (_igate_on) {
        /* connect-to-aprs-is */ 
        get_str_param("IGATE.HOST", host, INET_NAME_LENGTH, DFL_IGATE_HOST); 
        port = get_u16_param("IGATE.PORT", DFL_IGATE_PORT);

        int res = -1, tries = 0;
        wifi_enable(true);
        sleepMs(1000);
        while (!wifi_isConnected() 
                || (res = inet_open(host, port) != 0 && tries++ < 3))
            sleepMs(10000);
  
        if (_igate_on && res == 0) {
            /* Connected ok. Await welcome text */
            inet_read(frame, FRAME_LEN);
            ESP_LOGI(TAG, "Connected to %s:%d", host, port);
            beeps("--.  "); blipUp();
            _igate_run = true;
    
            // Login using username/passcode and (option) sende filter-string
            char uname[CRED_LENGTH];
            char filter[CRED_LENGTH];
            uint16_t pass;
            get_str_param("IGATE.USER", uname, CRED_LENGTH, DFL_IGATE_USER);
            pass = get_u16_param("IGATE.PASS", 0);
            get_str_param("IGATE.FILTER", filter, CRED_LENGTH, DFL_IGATE_FILTER);      
            igate_login(uname, pass, filter);

            
            /* Start child thread to listen for frames from radio or tracker */
            xTaskCreatePinnedToCore(&igate_radio, "Igate Radio", 
                STACK_IGATE_RADIO, NULL, NORMALPRIO, NULL, CORE_IGATE_RADIO);
            hdlc_subscribe_rx(&rxqueue, 2);
       
            /* Listen for data from APRS/IS server */
            while (_igate_on) {
                int len = inet_read(frame, FRAME_LEN); 
                if (len <= 0) 
                    break;
                if (frame[0] != '#')
                    inet2rf(frame);
            }
    
            /* Unsubscribe and terminate child thread */
            _igate_run = false; 
            fbq_signal(&rxqueue);
            sleepMs(10);
            hdlc_subscribe_rx(NULL, 2);
       
            /* Connection failure. Wait for 2 minutes */
            if (_igate_on) {
                ESP_LOGW(TAG, "Connection failed");
                beeps(" --. ..-.");
                for (int i=0; i<60 && _igate_on; i++)
                    sleepMs(2000);
            }  
        }
    }
    beeps("--.  "); blipDown();
    vTaskDelete(NULL);
}


/**********************
 *  igate init
 **********************/

void igate_init() {
    fbq_init(&rxqueue, HDLC_DECODER_QUEUE_SIZE);
    if (get_byte_param("IGATE.on", 0))
        igate_activate(true);
}



/***************************************************************
 * Activate the igate if argument is true
 * Deactivate if false
 ***************************************************************/

void igate_activate(bool m) 
{
    bool tstart = m && !_igate_on;
    bool tstop = !m && _igate_on;
  
    _igate_on = m;
    FBQ* mq = (_igate_on? &rxqueue : NULL);
   
    if (tstart) {
      /* Subscribe to RX (and tracker) packets and start treads */
        hdlc_subscribe_rx(mq, 2);
        tracker_setGate(mq);
        xTaskCreatePinnedToCore(&igate_main, "Igate Main", 
            STACK_IGATE, NULL, NORMALPRIO, NULL, CORE_IGATE);
        hlist_start();
    
        /* Turn on radio and decoder */
        radio_require();
        afsk_rx_enable();
    } 
    if (tstop) {
        /* Turn off radio and decoder */
        afsk_rx_disable();
        radio_release();
    
        /* Close internet connection */
        inet_close(); 
        sleepMs(100);

        hdlc_subscribe_rx(NULL, 2);
        tracker_setGate(NULL);
        _icount = _rcvd = _tracker_icount = 0;
    }
}




/***************************************************************************
 * Gate frame to internet
 ***************************************************************************/

static void rf2inet(FBUF *frame) 
{
    FBUF newHdr;
    char buf[FRAME_LEN];
    char mycall_s[10];
    addr_t from, to, mycall; 
    addr_t digis[7];
    uint8_t ctrl, pid;
    get_str_param("MYCALL", mycall_s, 10, DFL_MYCALL);
    str2addr(&mycall, mycall_s, false);
  
    fbuf_reset(frame);
    uint8_t ndigis =  ax25_decode_header(frame, &from, &to, digis, &ctrl, &pid);
    char type = fbuf_getChar(frame);
    bool own = addrCmp(&mycall, &from); 
  
    if (hlist_duplicate(&from, &to, frame, ndigis))
        return;
  
    static const char* nogate[8] = {"TCP", "NOGATE", "RFONLY", NULL};
    if ( type == '?' /* QUERY */ ||
        ( !own && ax25_search_digis( digis, ndigis, (char**) nogate)))
        return;
      
    beeps(". ");
      
    /* Write header in plain text -> newHdr */
    fbuf_new(&newHdr);
    fbuf_putstr(&newHdr, addr2str(buf, &from)); 
    fbuf_putstr(&newHdr, ">");
    fbuf_putstr(&newHdr, addr2str(buf, &to));
    
    if (ndigis > 0) {
        fbuf_putstr(&newHdr, ",");
        fbuf_putstr(&newHdr, digis2str(buf, ndigis, digis, false)); 
    }
    if (own && strncmp(buf, ",TCPIP", 5) == 0)
        fbuf_putstr(&newHdr, "*");
    else {
        fbuf_putstr(&newHdr, ",qAR,");
        fbuf_putstr(&newHdr, addr2str(buf, &mycall));  
    }
    fbuf_putstr(&newHdr, ":");
  
    /* Replace header in original packet with new header. 
     * Do this non-destructively: Just add rest of existing packet to new header 
     */
    fbuf_connect(&newHdr, frame, AX25_HDR_LEN(ndigis) );
  
    /* Send to internet server */
    int len = fbuf_read(&newHdr, FRAME_LEN, buf); 
    inet_write(buf, len);
    buf[len] = '\0';
    ESP_LOGI(TAG, "Frame gated to inet.."); 
    ESP_LOGD(TAG, "%s", buf);
    
    fbuf_release(&newHdr);
    if (own) _tracker_icount++; else _icount++;
}



/***************************************************************************
 * Gate frame to radio
 ***************************************************************************/

static void inet2rf(char *frame) {
    (void) frame;
    /* TBD */
}



/***********************************************
 * Log in to APRS/IS server. 
 * Assume that connection is established. 
 ***********************************************/

void igate_login(char* user, uint16_t pass, char* filter) 
{
    int n=0;
    char buf[128];
    n = sprintf(buf, "user %s pass %d vers Arctic-Tracker 0.1\r\n", user, pass);
    ESP_LOGD(TAG, "Login string: %s", buf);
    inet_write(buf, n);
    inet_read(buf, 128);
  
    if (strlen(filter) > 1) {
        n = sprintf(buf, "filter %s\r\n", filter);
        inet_write(buf, n);
    }
}

