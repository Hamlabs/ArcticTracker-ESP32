 

#define DFL_MYCALL        "NOCALL"
#define DFL_TRKLOG_URL    "https://localhost/trklog"
#define DFL_DEST          "APAR40"
#define DFL_DIGIPATH      "WIDE1-1"
#define DFL_SYMBOL        "/["
#define DFL_OBJ_SYMBOL    "/["
#define DFL_OBJ_ID        "MARK-"
#define DFL_REP_COMMENT   "Arctic Tracker"
#define DFL_IGATE_HOST    "aprs.no"
#define DFL_IGATE_USER    "NOCALL"
#define DFL_IGATE_FILTER  "m/10"
#define DFL_TIMEZONE      ""
#define DFL_FW_URL        ""


#define DFL_CRYPTO_KEY     "123456789"
#define DFL_SOFTAP_PASSWD  "123456789"
#define DFL_SOFTAP_IP      "192.168.0.1"
#define DFL_API_KEY        "123456789"
#define DFL_API_ORIGINS    ".*"
 
#define DFL_LORA_SF         12
#define DFL_LORA_CR          5
#define DFL_LORA_ALT_SF      5
#define DFL_LORA_ALT_CR      6
#define DFL_REPEAT           0
#define DFL_TRKLOG_INT       5
#define DFL_TRKLOG_TTL      24
#define DFL_ADC_REF       1100
#define DFL_TXDELAY         10
#define DFL_TXTAIL          10
#define DFL_MAXFRAME         2
#define DFL_MAXPAUSE       120
#define DFL_MINPAUSE        20
#define DFL_MINDIST        100
#define DFL_STATUSTIME      30

#if DEVICE == T_TWR
#define DFL_TRX_VOLUME       6
#define DFL_SOFTSQ          17
#else
#define DFL_TRX_VOLUME       7
#define DFL_SOFTSQ         110
#endif

#define DFL_TRX_SQUELCH      1
#define DFL_TRX_MICLEVEL     6
#define DFL_TURNLIMIT       35
#define DFL_IGATE_PORT   14580 
#define DFL_TXFREQ     1448000
#define DFL_RXFREQ     1448000
#define DFL_FREQ     433775000
#define DFL_TXPOWER          4

#define DFL_TRKLOG_ON      false
#define DFL_TRKLOG_POST_ON false
#define DFL_TRACKER_ON     true
#define DFL_TIMESTAMP_ON   true
#define DFL_COMPRESS_ON    true
#define DFL_ALTITUDE_ON    false
#define DFL_DIGIPEATER_ON  false
#define DFL_DIGI_WIDE1_ON  true
#define DFL_DIGI_SAR_ON    false
#define DFL_IGATE_ON       false
#define DFL_IGATE_TRACK_ON false
#define DFL_REPORT_BEEP_ON false
#define DFL_EXTRATURN_ON   false
#define DFL_TXMON_ON       true
#define DFL_RADIO_ON       true
#define DFL_TXLOW_ON       false

#define DFL_WIFI_ON        false
#define DFL_SOFTAP_ON      false

#define DFL_LORA_ALT_ON    false
