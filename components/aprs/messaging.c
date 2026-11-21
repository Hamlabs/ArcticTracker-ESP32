
/* 
 * Copyright (C) 2026 Øyvind Hanssen, LA7ECA
 * 
 * Arctic Tracker - Messaging
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



#include <stdlib.h>
#include "defines.h"
#include "config.h"
#include "ax25.h"
#include "hdlc.h"
#include "system.h"
#include "aprs.h"
#include "fbuf.h"
#include "gui.h"
#include "dupcheck.h"


   
#define TYPE_MSG ':'
#define TYPE_3RDPARTY '}'
#define MAX_MSG_LEN 67

#define STACK_MSG 3700
#define TAG "messaging"
   
static FBQ msg;
static bool msg_on = false;
static uint8_t subscription;

static void msg_receive (void *arg);

static dupcheck_t *msgids_acked, *msgids_rejected; 




void msg_init()
{
    fbq_init(&msg, HDLC_DECODER_QUEUE_SIZE);
    msg_activate(true);
    msgids_acked = dupcheck_create(100);
    msgids_rejected = dupcheck_create(100);
}





/******************************************************************************
 * Activate/deactivate messaging
 ******************************************************************************/

void msg_activate(bool m)
{ 
    /* Start if not on already */
    bool tstart = m && !msg_on;
   
    /* Stop if not stopped already */
    bool tstop = !m && msg_on;
   
    msg_on = m;
   
    if (tstart) {
        FBQ* mq = (msg_on? &msg : NULL);
        
        subscription = APRS_SUBSCRIBE_RX(mq);
        xTaskCreate(&msg_receive, "Message receiver", 
            STACK_MSG, NULL, NORMALPRIO, NULL);
    }
    
    if (tstop) {
        fbq_signal(&msg, SRC_RX);
        APRS_UNSUBSCRIBE_RX(subscription);
    }
}



/******************************************************************************
 * Send ack or rej message
 ******************************************************************************/

void msg_sendack(char* to, char* msgid, bool accept) {
    FBUF packet;    
    char from[16], dest[16], dbuf[75];
    fbq_t* outframes = APRS_GET_ENCODER_QUEUE();

    sleepMs(100);
    get_str_param("MYCALL", from, 10, "NOCALL");
    get_str_param("DEST", dest, 10, DFL_DEST);       
    get_str_param("DIGIPATH", dbuf, 70, "");

    fbuf_new(&packet, SRC_USER);
    ax25_aprs_header(&packet, from, dest, dbuf);
    char buf[16];
    sprintf(buf, ":%-9s:%s%s", to, (accept ? "ack" : "rej"), msgid); 
    fbuf_putstr(&packet, buf );                            
    fbq_put(outframes, packet); 
    sleepMs(10);
}



/********************************************************************************
 * Read a text from a fbuf that ends with 'end' or 'end2'. 
 * May be moved to fbuf.c 
 ********************************************************************************/

static char readText(FBUF* frame, char end, char end2, size_t limit, char *buf) {
    buf[0] = '\0';
    int i=0;
    char c = '\0';
    for (i=0; i<limit&&!fbuf_eof(frame); i++) {
        c = fbuf_getChar(frame);
        if (c==end || c==end2)
            break;
        buf[i] = c;
    }
    buf[i] = '\0';
    return c;
}



/***********************************************************************************
 * Read recipient callsign 
 ***********************************************************************************/

static void get_recipient(char* msgto, FBUF* frame) {
    readText(frame, ':', ':', 9, msgto);
    
    /* Replace padding with null */
    for (uint8_t i = strlen(msgto)-1; msgto[i] == ' '; i--)
        msgto[i] = '\0';
}



/***********************************************************************************
 * Handle authenticated message
 ***********************************************************************************/

static bool handle_authenticated(char* from, char* msgid, char* mac, char* content) {
    /*
     * Generate a MD5 checksum and compare with the mac given
     * Handle the content of message. This can be a command, e.g. "MSG"
     */
    ESP_LOGI(TAG, "Authenticated msg: %s: %s mac=%s", from, msgid, mac);
    msg_show(from, content);
    return true;
}



/******************************************************************************
 *  Handle acked incoming message
 ******************************************************************************/

static void handle_acked(FBUF* frame, char* from, char* msgid, char* content) {
    /* Use 2 dupcheckers. One for acked and one for rejected messages */
    if (dupcheck_exists(msgids_acked, msgid)) {
        msg_sendack(from, msgid, true);
        return;
    }
    if (dupcheck_exists(msgids_rejected, msgid)) {
        msg_sendack(from, msgid, false);
        return;
    }
    
    int len = strlen(content);
    bool accept = true;
    
    if (content[len-9] == '#') {
        /* MD5 MAC authentication */
        char* mac = content + len - 8;
        content[len-9] = '\0';
        handle_authenticated(from, msgid, mac, content);
    }
    /* Send and register the ack/rej response */
    msg_sendack(from, msgid, accept);
    dupcheck_add( (accept? msgids_acked : msgids_rejected), msgid);
    
}



/******************************************************************************
 *  Handle incoming message
 ******************************************************************************/

static void handle_messsage(FBUF* frame, char* from) {
    char msgto[10];
    char content[MAX_MSG_LEN+1]; 
    char seqno[6];
    char mycall[11];
    get_str_param("MYCALL", mycall, 10, DFL_MYCALL); 
    
    /* Return if recipient is not MYCALL */
    get_recipient(msgto, frame);
    if (strcmp(msgto, mycall) != 0)
        return;
    
    fbuf_getChar(frame); 
    readText(frame, '{', 0, MAX_MSG_LEN, content);
    readText(frame, 0, 0, 6, seqno);
    if (strlen(seqno) > 0) 
        handle_acked(frame, from, seqno, content);
    else {
        ESP_LOGI(TAG, "Message received from: %s: %s", from, content);
        msg_show(from, content);
    }
}
  


/******************************************************************************
 * handle incoming 3rd party APRS packet
 ******************************************************************************/

static void handle_3drparty(FBUF* frame) {
    char from[16]; 
    char to[16];
    char digis[100];
    to[0] = 0;
    digis[0] = 0;
    
    if (readText(frame, '>', ':', 16, from) == '>')
        if (readText(frame, ',', ':', 16, to) == ',')
            if (readText(frame, ':', 0, 100, digis) == ':')
    if (to[0] == 0)
        return;
    
    /* Is content of packet a message? */
    char c = fbuf_getChar(frame);
    if (c == TYPE_MSG) 
        handle_messsage(frame, from);
}



/******************************************************************************
 *  Message receiver thread
 ******************************************************************************/

static void msg_receive (void *arg)
{
    (void) arg; 
    ESP_LOGI(TAG, "Starting message receive thread");
    while (msg_on)
    {
        /* Wait for frame and then to AFSK decoder/encoder 
         * is not running. 
         */
        FBUF frame = fbq_get(&msg);
        if (!fbuf_empty(&frame)) {
            fbuf_reset(&frame);
            addr_t to, from;
            addr_t digis[7];
            uint8_t ctrl;
            uint8_t pid;
            uint8_t ndigis = ax25_decode_header(&frame, &from, &to, digis, &ctrl, &pid);

            if (ctrl==FTYPE_UI) {
                char fromstr[16];
                char c = fbuf_getChar(&frame);
                addr2str(fromstr, &from);
                if (c == TYPE_MSG) 
                    handle_messsage(&frame, fromstr);
                else if (c == TYPE_3RDPARTY)
                    handle_3drparty(&frame);
            }
        }
    
        /* And dispose the frame. Note that also an empty frame should be disposed! */
        fbuf_release(&frame);    
    }
  
    sleepMs(100);
    vTaskDelete(NULL);
}


