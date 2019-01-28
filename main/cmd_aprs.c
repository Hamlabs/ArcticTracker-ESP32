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
#include "defines.h"
#include "config.h"
#include "commands.h"

void   register_aprs(void);
   
CMD_USTR_SETTING (_param_mycall,     "MYCALL",      9,  "NOCALL",  REGEX_AXADDR);
CMD_USTR_SETTING (_param_dest,       "DEST",        9,  "APAT20",  REGEX_AXADDR);
CMD_USTR_SETTING (_param_digipath,   "DIGIPATH",    50, "",        REGEX_DIGIPATH);
CMD_STR_SETTING  (_param_symbol,     "SYMBOL",      3,  "/]",      REGEX_APRSSYM);
CMD_STR_SETTING  (_param_comment,    "REP_COMMENT", 40, "Arctic Tracker", NULL);
CMD_STR_SETTING  (_param_igate_host, "IGATE.HOST",  64, "aprs.no", REGEX_HOSTNAME);
CMD_STR_SETTING  (_param_igate_user, "IGATE.USER",  9,  "NOCALL",  REGEX_AXADDR);
CMD_STR_SETTING  (_param_igate_pass, "IGATE.PASS",  6,  NULL,   "[0-9]{2,5}");

CMD_BYTE_SETTING (_param_txdelay,    "TXDELAY",     10, 0, 100);
CMD_BYTE_SETTING (_param_txtail,     "TXTAIL",      10, 0, 100);
CMD_BYTE_SETTING (_param_maxframe,   "MAXFRAME",     2, 1, 7);
CMD_BYTE_SETTING (_param_maxpause,   "MAXPAUSE",   120, 0, 250);
CMD_BYTE_SETTING (_param_minpause,   "MINPAUSE",    20, 0, 250);
CMD_BYTE_SETTING (_param_mindist,    "MINDIST",    100, 0, 250);
CMD_BYTE_SETTING (_param_statustime, "STATUSTIME",  30, 1, 250);
CMD_U16_SETTING  (_param_turnlimit,  "TURNLIMIT",   35, 0, 360);
CMD_U16_SETTING  (_param_igate_port, "IGATE.PORT",  14580, 1, 65535);
CMD_I32_SETTING  (_param_txfreq,     "TXFREQ",     1448000, 1440000, 1460000);
CMD_I32_SETTING  (_param_rxfreq,     "RXFREQ",     1448000, 1440000, 1460000);

CMD_BOOL_SETTING (_param_timestamp,  "TIMESTAMP.on",  NULL);
CMD_BOOL_SETTING (_param_compress,   "COMPRESS.on",   NULL);
CMD_BOOL_SETTING (_param_altitude,   "ALTITUDE.on",   NULL);
CMD_BOOL_SETTING (_param_digipeater, "DIGIPEATER.on", NULL); 
CMD_BOOL_SETTING (_param_igate,      "IGATE.on",      NULL);
CMD_BOOL_SETTING (_param_digi_wide1, "DIGI.WIDE1.on", NULL);
CMD_BOOL_SETTING (_param_digi_sar,   "DIGI.SAR.on",   NULL);



/********************************************************************************
 * Register command handlers
 ********************************************************************************/

void register_aprs()
{
    ADD_CMD("mycall",    &_param_mycall,   "My callsign", "[<callsign>]");
    ADD_CMD("dest",      &_param_dest,     "APRS destination address", "[<addr>]");
    ADD_CMD("digipath",  &_param_digipath, "APRS Digipeater path", "[<addr>, ...]");
    ADD_CMD("symbol",    &_param_symbol,   "APRS symbol (sym-table symbol)", "[<T><S>]");
    ADD_CMD("comment",   &_param_comment,  "APRS posreport comment", "[<text>]");
    ADD_CMD("txdelay",   &_param_txdelay,  "APRS TXDELAY setting", "[<val>]");
    ADD_CMD("txtail",    &_param_txtail,   "APRS TXTAIL setting", "[<val>]");
       
    ADD_CMD("maxframe",  &_param_maxframe,   "APRS max frames in a transmission", "[<val>]");
    ADD_CMD("maxpause",  &_param_maxpause,   "Tracking max pause (10 sec units)", "[<val>]");
    ADD_CMD("minpause",  &_param_minpause,   "Tracking min pause (10 sec units)", "[<val>]");
    ADD_CMD("mindist",   &_param_mindist,    "Tracking min distance (meters)",    "[<val>]");
    ADD_CMD("statustime",&_param_statustime, "Status report time (10 sec units)", "[<val>]");
    ADD_CMD("turnlimit", &_param_turnlimit,  "Threshold for change of direction", "[<val>]");
    ADD_CMD("txfreq",    &_param_txfreq,     "TX frequency (100 Hz units)",       "[<val>]");
    ADD_CMD("rxfreq",    &_param_rxfreq,     "RX frequency (100 Hz units)",       "[<val>]");
    
    ADD_CMD("timestamp", &_param_timestamp,   "Timestamp setting",  "[on|off]");
    ADD_CMD("compress",  &_param_compress,    "Compress setting",  "[on|off]");
    ADD_CMD("altitude",  &_param_altitude,    "Altitude setting",  "[on|off]");
    
    ADD_CMD("digi",       &_param_digipeater, "Digipeater setting", "[on|off]"); 
    ADD_CMD("igate",      &_param_igate,      "Igate setting", "[on|off]");
    ADD_CMD("digi-wide1", &_param_digi_wide1, "Digipeater fill-in mode (WIDE1)", "[on|off]"); 
    ADD_CMD("digi-sar",   &_param_digi_sar,   "Digipeater preemption on 'SAR'", "[on|off]");
    ADD_CMD("igate-host", &_param_igate_host, "Igate server host",  "[<hostname>]");
    ADD_CMD("igate-port", &_param_igate_port, "Igate server port",  "[<portnr>]");
    ADD_CMD("igate-user", &_param_igate_user, "Igate server user",  "[<callsign>]");
    ADD_CMD("igate-pass", &_param_igate_pass, "Igate server passcode",  "[<code>]");
        
}


