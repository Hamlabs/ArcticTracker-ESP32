/*
 * Settings and shell commands related to aprs
 * By LA7ECA, ohanssen@acm.org
 */


#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h" 
#include "freertos/task.h"
#include "defines.h"
#include "config.h"
#include "commands.h"
#include "ax25.h"
#include "afsk.h"
#include "hdlc.h"
#include "radio.h"
#include "tracker.h"


void   register_aprs(void);
   



/********************************************************************************
 * AFSK generator testing
 ********************************************************************************/

static int do_teston(int argc, char** argv)
{ 
    
    uint8_t ch;
    sscanf(argv[1], "%hhx", &ch);
    printf("***** AFSK generator (0x%hhX) *****\n", ch);
    radio_require();
    hdlc_test_on(ch);
    getchar();
    hdlc_test_off();
    sleepMs(1000); 
    radio_release();
    return 0;
}


/********************************************************************************
 * Send test aprs packet
 ********************************************************************************/

static int do_testpacket(int argc, char** argv)
{
    static FBUF packet;    
    fbq_t* outframes = hdlc_get_encoder_queue();
    char from[11], to[11];
    char *dbuf = malloc(71); 
  
    radio_require();    
    sleepMs(100);
    get_str_param("MYCALL", from, 10, "NOCALL");
    get_str_param("DEST", to, 10, "APAT20");       
    get_str_param("DIGIPATH", dbuf, 70, "");

    fbuf_new(&packet);
    ax25_aprs_header(&packet, from, to, dbuf);
    fbuf_putstr(&packet, "The lazy brown dog jumps over the quick fox 1234567890");                      
    printf("*** Sending (AX25 UI) test packet ***\r\n");       
    fbq_put(outframes, packet); 
    ax25_display_frame(&packet);
    printf("\n");
    sleepMs(10);
    radio_release();
    free(dbuf);
    return 0;
}



/****************************************************************************
 * Monitor radio communications
 ****************************************************************************/

static int do_listen(int argc, char* argv[])
{
  (void) argv;
  (void) argc; 
  
  printf("**** MONITOR RADIO CHANNEL ****\n");
  radio_require();
  mon_activate(true);
  getchar();
  sleepMs(1000);
  mon_activate(false);
  radio_release();
  sleepMs(100);
  return 0;
}

/*****************************************************************
 * Handlers for making actions after changing after some 
 * setting changes. 
 *****************************************************************/

void hdl_squelch(uint8_t sq) {
    radio_setSquelch(sq);
}



void hdl_radio(bool on) {
    if ((radio_is_on() && on) || (!radio_is_on() && !on))
        return;
    if (on) { 
        printf("*** Radio on ***\n");
        radio_require();
    }
    else {
        printf("*** Radio off ***\n");
        radio_release();
    }
}


void hdl_tracker(bool on) {
    if (on) 
        tracker_on();
    else
        tracker_off();
}



// Radio and APRS settings

CMD_USTR_SETTING (_param_mycall,     "MYCALL",      9,  DFL_MYCALL,       REGEX_AXADDR);
CMD_USTR_SETTING (_param_dest,       "DEST",        9,  DFL_DEST,         REGEX_AXADDR);
CMD_USTR_SETTING (_param_digipath,   "DIGIPATH",    70, DFL_DIGIPATH,     REGEX_DIGIPATH);
CMD_STR_SETTING  (_param_symbol,     "SYMBOL",      3,  DFL_SYMBOL,       REGEX_APRSSYM);
CMD_STR_SETTING  (_param_osym,       "OBJ.SYMBOL",  3,  DFL_OBJ_SYMBOL,   REGEX_APRSSYM);
CMD_STR_SETTING  (_param_oid,        "OBJ.ID",      10, DFL_OBJ_ID,       REGEX_AXADDR);
CMD_STR_SETTING  (_param_comment,    "REP.COMMENT", 40, DFL_REP_COMMENT,  NULL);
CMD_STR_SETTING  (_param_igate_host, "IGATE.HOST",  64, DFL_IGATE_HOST,   REGEX_HOSTNAME);
CMD_STR_SETTING  (_param_igate_user, "IGATE.USER",  9,  DFL_IGATE_USER,   REGEX_AXADDR);
CMD_STR_SETTING  (_param_igate_pass, "IGATE.PASS",  6,  NULL,   "[0-9]{2,5}");

CMD_BYTE_SETTING (_param_txdelay,    "TXDELAY",     DFL_TXDELAY,     0, 250, NULL);
CMD_BYTE_SETTING (_param_txtail,     "TXTAIL",      DFL_TXTAIL,      0, 250, NULL);
CMD_BYTE_SETTING (_param_maxframe,   "MAXFRAME",    DFL_MAXFRAME,    1, 7,   NULL);
CMD_BYTE_SETTING (_param_maxpause,   "MAXPAUSE",    DFL_MAXPAUSE,    0, 250, NULL);
CMD_BYTE_SETTING (_param_minpause,   "MINPAUSE",    DFL_MINPAUSE,    0, 250, NULL);
CMD_BYTE_SETTING (_param_mindist,    "MINDIST",     DFL_MINDIST,     0, 250, NULL);
CMD_BYTE_SETTING (_param_statustime, "STATUSTIME",  DFL_STATUSTIME,  1, 250, NULL);
CMD_BYTE_SETTING (_param_squelch,    "TRX_SQUELCH", DFL_TRX_SQUELCH, 1, 8,   hdl_squelch);
CMD_U16_SETTING  (_param_turnlimit,  "TURNLIMIT",   DFL_TURNLIMIT,   0, 360);
CMD_U16_SETTING  (_param_igate_port, "IGATE.PORT",  DFL_IGATE_PORT,  1, 65535);
CMD_I32_SETTING  (_param_txfreq,     "TXFREQ",      DFL_TXFREQ,      1440000, 1460000);
CMD_I32_SETTING  (_param_rxfreq,     "RXFREQ",      DFL_RXFREQ,      1440000, 1460000);

CMD_BOOL_SETTING (_param_radio_on,   "RADIO.on",       hdl_radio);
CMD_BOOL_SETTING (_param_tracker_on, "TRACKER.on",     hdl_tracker);
CMD_BOOL_SETTING (_param_timestamp,  "TIMESTAMP.on",   NULL);
CMD_BOOL_SETTING (_param_compress,   "COMPRESS.on",    NULL);
CMD_BOOL_SETTING (_param_altitude,   "ALTITUDE.on",    NULL);
CMD_BOOL_SETTING (_param_digipeater, "DIGIPEATER.on",  NULL); 
CMD_BOOL_SETTING (_param_igate,      "IGATE.on",       NULL);
CMD_BOOL_SETTING (_param_digi_wide1, "DIGI.WIDE1.on",  NULL);
CMD_BOOL_SETTING (_param_digi_sar,   "DIGI.SAR.on",    NULL);
CMD_BOOL_SETTING (_param_rbeep_on,   "REPORT.BEEP.on", NULL);
CMD_BOOL_SETTING (_param_xturn_on,   "EXTRATURN.on",   NULL);
CMD_BOOL_SETTING (_param_repeat_on,  "REPEAT.on",      NULL);
CMD_BOOL_SETTING (_param_igtrack_on, "IGATE.TRACK.on", NULL);
CMD_BOOL_SETTING (_param_txmon_on,   "TXMON.on",       NULL);



/********************************************************************************
 * Register command handlers
 ********************************************************************************/

void register_aprs()
{
    ADD_CMD("teston",     &do_teston,          "HDLC encoder test", "<byte>");
    ADD_CMD("testpacket", &do_testpacket,      "Send test APRS packet", "");
    ADD_CMD("listen",     &do_listen,          "Monitor radio channel", "");
         
    ADD_CMD("mycall",     &_param_mycall,      "My callsign", "[<callsign>]");
    ADD_CMD("dest",       &_param_dest,        "APRS destination address", "[<addr>]");
    ADD_CMD("digipath",   &_param_digipath,    "APRS Digipeater path", "[<addr>, ...]");
    ADD_CMD("symbol",     &_param_symbol,      "APRS symbol (sym-table symbol)", "[<T><S>]");
    ADD_CMD("osymbol",    &_param_osym,        "APRS symbol for objects (sym-table symbol)", "[<T><S>]");
    ADD_CMD("objectid",   &_param_oid,         "ID prefix for object reports", "<str>");
    ADD_CMD("comment",    &_param_comment,     "APRS posreport comment", "[<text>]");
    ADD_CMD("txdelay",    &_param_txdelay,     "APRS TXDELAY setting", "[<val>]");
    ADD_CMD("txtail",     &_param_txtail,      "APRS TXTAIL setting", "[<val>]");
       
    ADD_CMD("maxframe",   &_param_maxframe,    "APRS max frames in a transmission", "[<val>]");
    ADD_CMD("maxpause",   &_param_maxpause,    "Tracking max pause (10 sec units)", "[<val>]");
    ADD_CMD("minpause",   &_param_minpause,    "Tracking min pause (10 sec units)", "[<val>]");
    ADD_CMD("mindist",    &_param_mindist,     "Tracking min distance (meters)",    "[<val>]");
    ADD_CMD("statustime", &_param_statustime,  "Status report time (10 sec units)", "[<val>]");
    ADD_CMD("turnlimit",  &_param_turnlimit,   "Threshold for change of direction", "[<val>]");
    ADD_CMD("txfreq",     &_param_txfreq,      "TX frequency (100 Hz units)",       "[<val>]");
    ADD_CMD("rxfreq",     &_param_rxfreq,      "RX frequency (100 Hz units)",       "[<val>]");
    ADD_CMD("squelch",    &_param_squelch,     "Squelch setting (1-8)",             "[<val>]");
    
    ADD_CMD("timestamp",  &_param_timestamp,   "Timestamp setting",  "[on|off]");
    ADD_CMD("compress",   &_param_compress,    "Compress setting",  "[on|off]");
    ADD_CMD("altitude",   &_param_altitude,    "Altitude setting",  "[on|off]");
    
    ADD_CMD("digi",       &_param_digipeater,  "Digipeater setting", "[on|off]"); 
    ADD_CMD("igate",      &_param_igate,       "Igate setting", "[on|off]");
    ADD_CMD("digi-wide1", &_param_digi_wide1,  "Digipeater fill-in mode (WIDE1)", "[on|off]"); 
    ADD_CMD("digi-sar",   &_param_digi_sar,    "Digipeater preemption on 'SAR'", "[on|off]");
    ADD_CMD("igate-host", &_param_igate_host,  "Igate server host",  "[<hostname>]");
    ADD_CMD("igate-port", &_param_igate_port,  "Igate server port",  "[<portnr>]");
    ADD_CMD("igate-user", &_param_igate_user,  "Igate server user",  "[<callsign>]");
    ADD_CMD("igate-pass", &_param_igate_pass,  "Igate server passcode",  "[<code>]");
    
    ADD_CMD("radio",      &_param_radio_on,    "Radio module power", "[on|off]");
    ADD_CMD("tracker",    &_param_tracker_on,  "APRS tracker setting", "[on|off]");
    ADD_CMD("reportbeep", &_param_rbeep_on,    "Beep when report is sent", "[on|off]");
    ADD_CMD("extraturn",  &_param_xturn_on,    "Send extra posreport in turns", "[on|off]");
    ADD_CMD("repeat",     &_param_repeat_on,   "Repeat posreports (piggyback on later transmissions)", "[on|off]");
    ADD_CMD("igtrack",    &_param_igtrack_on,  "Send posreports directly to APRS/IS when available", "[on|off]");   
    ADD_CMD("txmon",      &_param_txmon_on,    "Tx monitor", "[on|off]");
}


