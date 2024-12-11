/*
 * Settings and shell commands related to aprs
 * By LA7ECA, ohanssen@acm.org
 */


#include <stdio.h>
#include <string.h>
#include "defines.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "system.h"
#include "config.h"
#include "commands.h"
#include "ax25.h"
#include "afsk.h"
#include "hdlc.h"
#include "radio.h"
#include "tracker.h"
#include "digipeater.h"
#include "igate.h"
#include "trackstore.h"
#include "tracklogger.h"
#include "lora1268.h"
#include "aprs.h"


void   register_aprs(void);
   


#if !defined(ARCTIC4_UHF)

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
#else

static int do_heard(int argc, char** argv)
{
    char buf[16]; 
    if (loraprs_last_rssi() == 0)
        printf("Nothing heard yet\n");
    else
        printf("Last heard: %s, RSSI: %d dBm, SNR: %d dB\n", 
            loraprs_last_heard(buf), loraprs_last_rssi(), loraprs_last_snr());
    return 0;
}


#endif

/********************************************************************************
 * Send test aprs packet
 ********************************************************************************/

static int do_testpacket(int argc, char** argv)
{
    FBUF packet;    
    fbq_t* outframes = APRS_GET_ENCODER_QUEUE();
    
    char from[11], to[11];
    char *dbuf = malloc(71); 
  
    radio_require();    
    sleepMs(100);
    get_str_param("MYCALL", from, 10, "NOCALL");
    get_str_param("DEST", to, 10, DFL_DEST);       
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
  
#if !defined(ARCTIC4_UHF)
  radio_require();
  afsk_rx_enable();
#endif
  mon_activate(true);
  getchar();
  sleepMs(1000);
  mon_activate(false);
#if !defined(ARCTIC4_UHF)
  afsk_rx_disable(); 
  radio_release();
#endif
  sleepMs(100);
  return 0;
}




/****************************************************************************
 * Get record from track log (for testing)
 ****************************************************************************/

static int do_trget(int argc, char* argv[])
{
    (void) argv;
    (void) argc; 
    posentry_t x;
    if (trackstore_getEnt(&x)==NULL)
        printf("*** Store is empty\n");
    else
        printf("Entry: time=%ld, %lu, %lu\n", x.time, x.lat, x.lng);
   
    return 0;
}


static int do_trput(int argc, char* argv[])
{
    (void) argv;
    (void) argc; 
    if (!gps_is_fixed()) {
        gps_current_pos.timestamp = getTime();
        gps_current_pos.latitude += 0.001;
    }
    trackstore_put(&gps_current_pos);
    return 0;
}


  
/*****************************************************************
 * Handlers for making actions after changing after some 
 * setting changes. 
 *****************************************************************/

#if defined(ARCTIC4_UHF)

void hdl_lora_sf(uint8_t sf) {
    uint8_t cr = get_byte_param("LORA_CR", DFL_LORA_CR);
    lora_SetModulationParams(sf, SX126X_LORA_BW_125_0, cr-4, (sf>=11 ? 1:0)); 
}


void hdl_lora_cr(uint8_t cr) {
    uint8_t sf = get_byte_param("LORA_SF", DFL_LORA_SF);
    lora_SetModulationParams(sf, SX126X_LORA_BW_125_0, cr-4, (sf>=11 ? 1:0)); 
}


void hdl_freq(int32_t freq) {
    printf("Please restart to let change take effect\n");
}

void hdl_txpower(uint8_t po) {
    lora_setTxPower(po);
}



#else

void hdl_squelch(uint8_t sq) {
    radio_setSquelch(sq);
    afsk_setSquelchOff(sq==0 ? true : false);
    if (sq==0)
        afsk_rx_nextFrame();
}

void hdl_softsq(int32_t sq) {
    afsk_setSoftSq((uint16_t) sq); 
}

void hdl_miclevel(uint8_t ml) {
    radio_setMicLevel(ml); 
}

void hdl_volume(uint8_t vol) {
    radio_setVolume(vol); 
}

void hdl_txlow(bool on) {
    radio_setLowTxPower(on);
}

void hdl_txfreq(int32_t freq) {
    radio_setFreq(freq, -1); 
}

void hdl_rxfreq(int32_t freq) {
    radio_setFreq(-1, freq); 
}


#endif


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



void hdl_tracklog(bool on) {
    if (on) 
        tracklog_on();
    else
        tracklog_off();
}

void hdl_tracker(bool on) {
    if (on) 
        tracker_on();
    else
        tracker_off();
}

void hdl_digipeater(bool on) {
    digipeater_activate(on); 
}

void hdl_igate(bool on) {
    igate_activate(on); 
}

void hdl_trkpost(bool on) {
    if (on)
        tracklog_post_start();
    else
        tracklog_post_stop();
}





// Radio and APRS settings

CMD_USTR_SETTING (_param_mycall,     "MYCALL",       10, DFL_MYCALL,       REGEX_AXADDR);
CMD_USTR_SETTING (_param_dest,       "DEST",         10, DFL_DEST,         REGEX_AXADDR);
CMD_USTR_SETTING (_param_digipath,   "DIGIPATH",     70, DFL_DIGIPATH,     REGEX_DIGIPATH);

CMD_STR_SETTING  (_param_trklogurl,  "TRKLOG.URL",   64, DFL_TRKLOG_URL,   REGEX_URL);
CMD_STR_SETTING  (_param_serverkey,  "TRKLOG.KEY",   128, "",              NULL);
CMD_STR_SETTING  (_param_symbol,     "SYMBOL",       3,  DFL_SYMBOL,       REGEX_APRSSYM);
CMD_STR_SETTING  (_param_osym,       "OBJ.SYMBOL",   3,  DFL_OBJ_SYMBOL,   REGEX_APRSSYM);
CMD_STR_SETTING  (_param_oid,        "OBJ.ID",       10, DFL_OBJ_ID,       REGEX_AXADDR);
CMD_STR_SETTING  (_param_comment,    "REP.COMMENT",  40, DFL_REP_COMMENT,  NULL);
CMD_STR_SETTING  (_param_igate_host, "IGATE.HOST",   64, DFL_IGATE_HOST,   REGEX_HOSTNAME);
CMD_STR_SETTING  (_param_igate_user, "IGATE.USER",   9,  DFL_IGATE_USER,   REGEX_AXADDR);
CMD_STR_SETTING  (_param_igate_filt, "IGATE.FILTER", 32, DFL_IGATE_FILTER, ".*");
CMD_BYTE_SETTING (_param_trklogint,  "TRKLOG.INT",   DFL_TRKLOG_INT,  0, 60,  NULL);
CMD_BYTE_SETTING (_param_trklogttl,  "TRKLOG.TTL",   DFL_TRKLOG_TTL,  0, 250, NULL);
CMD_BYTE_SETTING (_param_maxframe,   "MAXFRAME",     DFL_MAXFRAME,    1, 7,   NULL);
CMD_BYTE_SETTING (_param_maxpause,   "MAXPAUSE",     DFL_MAXPAUSE,    0, 250, NULL);
CMD_BYTE_SETTING (_param_minpause,   "MINPAUSE",     DFL_MINPAUSE,    0, 250, NULL);
CMD_BYTE_SETTING (_param_mindist,    "MINDIST",      DFL_MINDIST,     0, 250, NULL);
CMD_BYTE_SETTING (_param_statustime, "STATUSTIME",   DFL_STATUSTIME,  1, 250, NULL);
CMD_BYTE_SETTING (_param_repeat,     "REPEAT",       DFL_REPEAT,      0, 4,   NULL);
CMD_U16_SETTING  (_param_turnlimit,  "TURNLIMIT",    DFL_TURNLIMIT,   0, 360);
CMD_U16_SETTING  (_param_igate_port, "IGATE.PORT",   DFL_IGATE_PORT,  1, 65535);
CMD_U16_SETTING  (_param_igate_pass, "IGATE.PASS",   0,               0, 65535);

CMD_BOOL_SETTING (_param_tracklog_on,"TRKLOG.on",      hdl_tracklog);
CMD_BOOL_SETTING (_param_trkpost_on, "TRKLOG.POST.on", hdl_trkpost);
CMD_BOOL_SETTING (_param_tracker_on, "TRACKER.on",     hdl_tracker);
CMD_BOOL_SETTING (_param_timestamp,  "TIMESTAMP.on",   NULL);
CMD_BOOL_SETTING (_param_compress,   "COMPRESS.on",    NULL);
CMD_BOOL_SETTING (_param_altitude,   "ALTITUDE.on",    NULL);
CMD_BOOL_SETTING (_param_digipeater, "DIGIPEATER.on",  hdl_digipeater); 
CMD_BOOL_SETTING (_param_igate,      "IGATE.on",       hdl_igate);
CMD_BOOL_SETTING (_param_digi_wide1, "DIGI.WIDE1.on",  NULL);
CMD_BOOL_SETTING (_param_digi_sar,   "DIGI.SAR.on",    NULL);
CMD_BOOL_SETTING (_param_rbeep_on,   "REPORT.BEEP.on", NULL);
CMD_BOOL_SETTING (_param_xturn_on,   "EXTRATURN.on",   NULL);
CMD_BOOL_SETTING (_param_igtrack_on, "IGATE.TRACK.on", NULL);
CMD_BOOL_SETTING (_param_txmon_on,   "TXMON.on",       NULL);
CMD_BOOL_SETTING (_param_radio_on,   "RADIO.on",       hdl_radio);


#if defined(ARCTIC4_UHF)
CMD_I32_SETTING  (_param_freq,       "FREQ",           DFL_FREQ,           433000000, 436000000, hdl_freq);
CMD_BYTE_SETTING (_param_lora_sf,    "LORA_SF",        DFL_LORA_SF,        7, 12,  hdl_lora_sf);
CMD_BYTE_SETTING (_param_lora_cr,    "LORA_CR",        DFL_LORA_CR,        5, 8,   hdl_lora_cr);
CMD_BYTE_SETTING (_param_txpower,    "TXPOWER",        DFL_TXPOWER,        0, 6,   hdl_txpower);
#else

CMD_I32_SETTING  (_param_txfreq,     "TXFREQ",       DFL_TXFREQ,      1440000, 1460000, hdl_txfreq);
CMD_I32_SETTING  (_param_rxfreq,     "RXFREQ",       DFL_RXFREQ,      1440000, 1460000, hdl_rxfreq);
CMD_I32_SETTING  (_param_softsq,     "SOFTSQ",       DFL_SOFTSQ,      0, 4000,  hdl_softsq);
CMD_BYTE_SETTING (_param_squelch,    "TRX_SQUELCH",  DFL_TRX_SQUELCH, 0, 8,   hdl_squelch);
CMD_BYTE_SETTING (_param_miclevel,   "TRX_MICLEVEL", DFL_TRX_MICLEVEL,1, 8,   hdl_miclevel);
CMD_BYTE_SETTING (_param_volume,     "TRX_VOLUME",   DFL_TRX_VOLUME,  1, 8,   hdl_volume);
CMD_BYTE_SETTING (_param_txdelay,    "TXDELAY",      DFL_TXDELAY,     0, 250, NULL);
CMD_BYTE_SETTING (_param_txtail,     "TXTAIL",       DFL_TXTAIL,      0, 250, NULL);
CMD_BOOL_SETTING (_param_txlow_on,   "TXLOW.on",     hdl_txlow);

#endif



/********************************************************************************
 * Register command handlers
 ********************************************************************************/

void register_aprs()
{

    ADD_CMD("listen",     &do_listen,          "Monitor radio channel", "");
    ADD_CMD("trklog-get", &do_trget,           "Get tracklog record", "");      
    ADD_CMD("trklog-put", &do_trput,           "Put tracklog record", "");  
    
    ADD_CMD("mycall",     &_param_mycall,      "My callsign", "[<callsign>]");
//    ADD_CMD("dest",       &_param_dest,        "APRS destination address", "[<addr>]");
    ADD_CMD("digipath",   &_param_digipath,    "APRS Digipeater path", "[<addr>, ...]");
    ADD_CMD("symbol",     &_param_symbol,      "APRS symbol (sym-table symbol)", "[<T><S>]");
    ADD_CMD("osymbol",    &_param_osym,        "APRS symbol for objects (sym-table symbol)", "[<T><S>]");
    ADD_CMD("objectid",   &_param_oid,         "ID prefix for object reports", "<str>");
    ADD_CMD("comment",    &_param_comment,     "APRS posreport comment", "[<text>]");
    ADD_CMD("repeat",     &_param_repeat,      "# Times to repeat posreports (0-3)", "[val]");           
    ADD_CMD("trklog-int", &_param_trklogint,   "Interval for track logging (seconds)", "[<val>]");
    ADD_CMD("trklog-ttl", &_param_trklogttl,   "Max time to keep tracklog entries (hours)", "[<val>]");
    ADD_CMD("trklog-key", &_param_serverkey,   "KEY for authenticating tracklog-messages to Polaric Server", "[<key>]");
    ADD_CMD("trklog-url", &_param_trklogurl,   "URL for posting tracklog updates to Polaric Server", "[<url>]");
    ADD_CMD("maxframe",   &_param_maxframe,    "APRS max frames in a transmission", "[<val>]");
    ADD_CMD("maxpause",   &_param_maxpause,    "Tracking max pause (10 sec units)", "[<val>]");
    ADD_CMD("minpause",   &_param_minpause,    "Tracking min pause (10 sec units)", "[<val>]");
    ADD_CMD("mindist",    &_param_mindist,     "Tracking min distance (meters)",    "[<val>]");
    ADD_CMD("statustime", &_param_statustime,  "Status report time (10 sec units)", "[<val>]");
    ADD_CMD("turnlimit",  &_param_turnlimit,   "Threshold for change of direction", "[<val>]");
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
    ADD_CMD("tracklog",   &_param_tracklog_on, "Track logging", "[on|off]"); 
    ADD_CMD("trklog-post",&_param_trkpost_on,  "Track log automatic post to server", "[on|off]");
    ADD_CMD("radio",      &_param_radio_on,    "Radio module power", "[on|off]");
    ADD_CMD("tracker",    &_param_tracker_on,  "APRS tracker setting", "[on|off]");
    ADD_CMD("reportbeep", &_param_rbeep_on,    "Beep when report is sent", "[on|off]");
    ADD_CMD("extraturn",  &_param_xturn_on,    "Send extra posreport when changing direction", "[on|off]");
    ADD_CMD("igtrack",    &_param_igtrack_on,  "Send posreports directly to APRS/IS when available", "[on|off]");   
    ADD_CMD("txmon",      &_param_txmon_on,    "Tx monitor (show TX packets)", "[on|off]");
    ADD_CMD("testpacket", &do_testpacket,      "Send test APRS packet", "");
    
#if !defined(ARCTIC4_UHF)
    ADD_CMD("teston",     &do_teston,          "HDLC encoder test", "<byte>");    
    ADD_CMD("txdelay",    &_param_txdelay,     "APRS TXDELAY setting", "[<val>]");
    ADD_CMD("txtail",     &_param_txtail,      "APRS TXTAIL setting", "[<val>]");
    ADD_CMD("squelch",    &_param_squelch,     "Squelch setting (1-8)",             "[<val>]");
    ADD_CMD("softsq",     &_param_softsq,      "Soft Squelch setting",              "[<val>]");
    ADD_CMD("volume",     &_param_volume,      "RX audio level setting (1-8)",      "[<val>]");
    ADD_CMD("txlow",      &_param_txlow_on,    "Tx power low", "[on|off]");
    ADD_CMD("txfreq",     &_param_txfreq,      "TX frequency (100 Hz units)",       "[<val>]");
    ADD_CMD("rxfreq",     &_param_rxfreq,      "RX frequency (100 Hz units)",       "[<val>]");
#else
    ADD_CMD("lora-sf",    &_param_lora_sf,     "LoRa spreading factor (7-12)",      "[<val>]");
    ADD_CMD("lora-cr",    &_param_lora_cr,     "LoRa coding rate (5-8)",            "[<val>]");
    ADD_CMD("txpower",    &_param_txpower,     "Tx power (1-6)",                    "[<val>]");
    ADD_CMD("freq",       &_param_freq,        "TX/RX frequency (Hz)",              "[<val>]");
    ADD_CMD("heard",      &do_heard,           "Last heard packet",                 "");
#endif
}


