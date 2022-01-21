/*
 * Web server
 * By LA7ECA, ohanssen@acm.org
 * Uses https://github.com/chmorgan/libesphttpd
 */
#include "defines.h"
#include <libesphttpd/esp.h>
#include "libesphttpd/httpd.h"
#include "libesphttpd/httpd-espfs.h"
#include "libesphttpd/cgiwifi.h"
#include "libesphttpd/cgiflash.h"
#include "libesphttpd/auth.h"
#include "libesphttpd/captdns.h"
#include "libesphttpd/cgiwebsocket.h"
#include "libesphttpd/httpd-freertos.h"
#include "libesphttpd/route.h"
#include "espfs_image.h"
#include "networking.h"
#include "config.h"
#include "system.h"
#include "digipeater.h"
#include "igate.h"


#define LISTEN_PORT     80u
#define MAX_CONNECTIONS 16u



#define METHOD_GET(cd) ((cd)->requestType==HTTPD_METHOD_GET)
#define METHOD_POST(cd) ((cd)->requestType==HTTPD_METHOD_POST)

#define CGIFUNC CgiStatus ICACHE_FLASH_ATTR

#define TEST_CHECKED(buf, x) sprintf(buf, "%s", (get_byte_param(x, 0) ? "checked" : ""))

#define POST_ARG(cd, arg, buf, siz) httpdFindArg(cd->post.buff, arg, buf, siz)

#define RESPOND_CODE(cdata, code) { \
   httpdStartResponse((cdata), (code)); \
   httpdEndHeaders((cdata)); \
   return HTTPD_CGI_DONE; \
}

#define TPL_HEAD(token, cdata) \
    if (strcmp(token, "head")==0) { \
        head(cdata); \
        return HTTPD_CGI_DONE; \
    } 
    
static void updateStrField(HttpdConnData *cdata, const char* key, const char* pparm, char* pattern, bool up);
static void updateApAlt(HttpdConnData *cdata, const int index);
static void startResp(HttpdConnData *cdata, int code, char* ctype);
static void head(HttpdConnData *cdata);
static char connectionMemory[sizeof(RtosConnType) * MAX_CONNECTIONS];
static HttpdFreertosInstance httpdFreertosInstance;

CGIFUNC tpl_aprs(HttpdConnData *con, char *token, void **arg);
CGIFUNC tpl_digi(HttpdConnData *con, char *token, void **arg);
CGIFUNC tpl_trklog(HttpdConnData *con, char *token, void **arg);
CGIFUNC tpl_sysInfo(HttpdConnData *con, char *token, void **arg); 
CGIFUNC tpl_wifi(HttpdConnData *con, char *token, void **arg);
CGIFUNC tpl_fw(HttpdConnData *con, char *token, void **arg);
CGIFUNC cgi_updateWifi(HttpdConnData *cdata);
CGIFUNC cgi_updateDigi(HttpdConnData *cdata);
CGIFUNC cgi_updateTrklog(HttpdConnData *cdata);
CGIFUNC cgi_updateAprs(HttpdConnData *cdata);
CGIFUNC cgi_updateFw(HttpdConnData *cdata);

#define TAG "httpd"



int myPassFn(HttpdConnData *connData, int no, char *user, int userLen, char *pass, int passLen) {
	if (no==0) {
        get_str_param("HTTPD.USR", user, 32, HTTPD_DEFAULT_USR);
        get_str_param("HTTPD.PWD", pass, 64, HTTPD_DEFAULT_PWD);
		return 1;
	}
	return 0;
}




/**************************************************** 
 * Configure routes/paths of webserver here 
 ****************************************************/

HttpdBuiltInUrl builtInUrls[] = {
    {"/*", authBasic, myPassFn, NULL}, 
	ROUTE_REDIRECT("/", "/index.html"),
	
	ROUTE_TPL("/sysinfo.tpl", tpl_sysInfo),
    ROUTE_REDIRECT("/sysinfo", "/sysinfo.tpl"),
    
	ROUTE_TPL("/wifi.tpl", tpl_wifi),
	ROUTE_CGI("/wifiupdate", cgi_updateWifi),
	ROUTE_REDIRECT("/wifi", "/wifi.tpl"),
    
	ROUTE_TPL("/aprs.tpl", tpl_aprs),
    ROUTE_CGI("/aprsupdate", cgi_updateAprs),
	ROUTE_REDIRECT("/aprs", "/aprs.tpl"),
	
	ROUTE_TPL("/digi.tpl", tpl_digi),
	ROUTE_CGI("/digiupdate", cgi_updateDigi),
    ROUTE_REDIRECT("/digi", "/digi.tpl"),
		
    ROUTE_TPL("/trklog.tpl", tpl_trklog),
	ROUTE_CGI("/trklogupdate", cgi_updateTrklog),
    ROUTE_REDIRECT("/trklog", "/trklog.tpl"),
    
    ROUTE_TPL("/firmware.tpl", tpl_fw),
    ROUTE_CGI("/fwupdate", cgi_updateFw),
    ROUTE_REDIRECT("/fw", "/firmware.tpl"),
    
	ROUTE_FILESYSTEM(),
	ROUTE_END()
};



/********************************************************
 * Various helper functions...
 ********************************************************/

inline static void startResp(HttpdConnData *cdata, int code, char* ctype) {
    httpdStartResponse(cdata, code); 
	httpdHeader(cdata, "Content-Type", ctype);
	httpdEndHeaders(cdata);
}    
    
inline static void head(HttpdConnData *cdata) {
    httpdSend(cdata, "<head><title>Artig Tracker</title>", -1); 
    httpdSend(cdata, "<link rel=\"stylesheet\" href=\"style.css\" type=\"text/css\"></head>", -1);
}
   

static void updateStrField(HttpdConnData *cdata, const char* key, const char* pparm, char* pattern, bool upper) 
{
    char val[64], msg[64], buf[128];
    POST_ARG(cdata, pparm, val, 64);
    if (upper)
        strupr(val);
    int n = sprintf(buf, "Update %s: %s<br>", key, param_parseStr(key, val, strlen(val), pattern, msg));
    httpdSend(cdata, buf, n);
}


static void updateBigStrField(HttpdConnData *cdata, const char* key, const char* pparm, char* pattern) 
{
    char msg[64], buf[128];
    char* bbuf = malloc(BBUF_SIZE+1);
    POST_ARG(cdata, pparm, bbuf, BBUF_SIZE);
    int bytes = strlen(bbuf);
    int n = sprintf(buf, "Update %s: %s<br>", key, param_parseStr(key, bbuf, bytes, pattern, msg));
    httpdSend(cdata, buf, n);
    free(bbuf);
}


static void updateBoolField(HttpdConnData *cdata, const char* key, const char* pparm, BoolHandler bh)
{
    char val[32], msg[64], buf[128];
    strcpy(val, "false");
    POST_ARG(cdata, pparm, val, 32);
    int n = sprintf(buf, "Update %s: %s<br>", key, param_parseBool(key, val, msg));
    if (bh != NULL)
        (*bh)(GET_BYTE_PARAM(key));
    httpdSend(cdata, buf, n);
}


static void updateI32Field(HttpdConnData *cdata, const char* key, const char* pparm, 
        int32_t llimit, int32_t ulimit)
{
    char val[16], msg[64], buf[128];
    POST_ARG(cdata, pparm, val, 16);
    int n = sprintf(buf, "Update %s: %s<br>", key, 
                    param_parseI32(key, val, llimit, ulimit, msg ) );
    httpdSend(cdata, buf, n);
}

static void updateU16Field(HttpdConnData *cdata, const char* key, const char* pparm, 
        uint16_t llimit, uint16_t ulimit)
{
    char val[16], msg[64], buf[128];
    POST_ARG(cdata, pparm, val, 16);
    int n = sprintf(buf, "Update %s: %s<br>", key, 
                    param_parseU16(key, val, llimit, ulimit, msg ) );
    httpdSend(cdata, buf, n);
}

static void updateByteField(HttpdConnData *cdata, const char* key, const char* pparm, 
        uint8_t llimit, uint8_t ulimit)
{
    char val[16], msg[64], buf[128];
    POST_ARG(cdata, pparm, val, 16);
    int n = sprintf(buf, "Update %s: %s<br>", key, 
                    param_parseByte(key, val, llimit, ulimit, msg ) );
    httpdSend(cdata, buf, n);
}



static void updateApAlt(HttpdConnData *cdata, const int index) 
{
    char pparm[16]; 
    char buf[32];
    wifiAp_t ap; 
    
    sprintf(pparm, "wifiap%d_ssid", index);
    POST_ARG(cdata, pparm, ap.ssid, 32);
    sprintf(pparm, "wifiap%d_pwd", index);
    POST_ARG(cdata, pparm, ap.passwd, 32);
    ESP_LOGI(TAG, "Update AP alt %d: ssid=%s, passwd=%s", index, ap.ssid, ap.passwd);
    wifi_setApAlt(index, &ap);
    int n = sprintf(buf, "Update AP alternative %d.<br>", index);
    httpdSend(cdata, buf, n);
}


static void hdl_digipeater(bool on) {
    digipeater_activate(on); 
}

static void hdl_igate(bool on) {
    igate_activate(on); 
}



/***********************************************************************
 * prefix for update CGI functions
 *  return 0 of ok to continue.
 ***********************************************************************/
static CgiStatus begin_updateResp(HttpdConnData *cdata, char* text) {
    if (cdata->isConnectionClosed) 
		return HTTPD_CGI_DONE;

	if (!METHOD_POST(cdata))
		RESPOND_CODE(cdata, 406);
	
    startResp(cdata, 200, "text/html");
    httpdSend(cdata, "<html>", -1); 
    head(cdata); 
    httpdSend(cdata, "<body><h2>", -1); 
    httpdSend(cdata, text, -1); 
    httpdSend(cdata, "</h2><fieldset>", -1);
    return 0;
}

#define BEGIN_UPDATERESP(cdata, text) \
    { CgiStatus st = begin_updateResp((cdata), (text)); \
        if (st != 0) \
            return st; \
    }

#define END_UPDATERESP(cdata) \
       httpdSend((cdata), "</fieldset></body></html>", -1);

       
       
       
/**************************************************** 
 * CGI function for updating WIFI settings
 ****************************************************/

CGIFUNC cgi_updateWifi(HttpdConnData *cdata) {
    BEGIN_UPDATERESP(cdata, "Update WIFI settings..."); 
    updateStrField(cdata, "WIFIAP.AUTH", "appass", ".*", false);
    updateStrField(cdata, "HTTPD.USR", "htuser", ".*", false);
    updateStrField(cdata, "HTTPD.PWD", "htpass", ".*", false);
    
    for (int i=0; i<6; i++)
        updateApAlt(cdata, i);
     // FIXME: Sanitize input fields
    
    END_UPDATERESP(cdata);
    return HTTPD_CGI_DONE;
}



/**************************************************** 
 * CGI function for updating APRS settings
 ****************************************************/

CGIFUNC cgi_updateAprs(HttpdConnData *cdata) {
    BEGIN_UPDATERESP(cdata, "Update APRS settings...");
   
    updateBoolField(cdata, "TIMESTAMP.on", "timestamp_on", NULL);
    updateBoolField(cdata ,"COMPRESS.on",  "compress_on",  NULL);
    updateBoolField(cdata, "ALTITUDE.on",  "altitude_on",  NULL);
    updateBoolField(cdata, "EXTRATURN.on", "xonturn_on", NULL);
    
    updateI32Field (cdata, "TXFREQ",       "tx_freq",     1440000, 1460000);
    updateI32Field (cdata, "RXFREQ",       "rx_freq",     1440000, 1460000);    
    updateU16Field (cdata, "TURNLIMIT",    "turnlimit",   0, 360);
    updateByteField(cdata, "MAXPAUSE",     "maxpause",    0, 250);
    updateByteField(cdata, "MINPAUSE",     "minpause",    0, 250);    
    updateByteField(cdata, "MINDIST",      "mindist",     0, 250);
    updateByteField(cdata, "REPEAT",       "redundancy",  0, 4);
   
    updateStrField(cdata, "MYCALL",        "mycall",    REGEX_AXADDR, true);
    updateStrField(cdata, "SYMBOL",        "symbol",    REGEX_APRSSYM, false);
    updateStrField(cdata, "DIGIPATH",      "digis",     REGEX_DIGIPATH, true);    
    updateStrField(cdata, "REP_COMMENT",   "rcomment",  ".*", false);  
    
    END_UPDATERESP(cdata);
    return HTTPD_CGI_DONE;
}



/**************************************************** 
 * CGI function for updating digi/igate settings
 ****************************************************/

CGIFUNC cgi_updateDigi(HttpdConnData *cdata) {
    BEGIN_UPDATERESP(cdata, "Update digi/igate settings...");

    updateBoolField(cdata,"DIGIPEATER.on", "digi_on",  hdl_digipeater);
    updateBoolField(cdata,"IGATE.on",      "igate_on", hdl_igate);
    updateBoolField(cdata,"DIGI.WIDE1.on", "wide1_on", NULL);
    updateBoolField(cdata,"DIGI.SAR.on",   "sar_on",   NULL);
    updateStrField(cdata, "IGATE.HOST",    "ig_host", REGEX_HOSTNAME, false);
    updateStrField(cdata, "IGATE.USER",    "ig_user", REGEX_AXADDR, false);
    updateU16Field(cdata, "IGATE.PASS",    "ig_pass", 0, 65535);
        
    END_UPDATERESP(cdata);

    return HTTPD_CGI_DONE;
}



CGIFUNC cgi_updateTrklog(HttpdConnData *cdata) {
    BEGIN_UPDATERESP(cdata, "Update traclog settings...");
    END_UPDATERESP(cdata);
    return HTTPD_CGI_DONE;
}



/****************************************************** 
 * CGI function for updating firmware update settings
 ******************************************************/

CGIFUNC cgi_updateFw(HttpdConnData *cdata) {
    BEGIN_UPDATERESP(cdata, "Update firmware update settings...");
    updateStrField(cdata, "FW.URL",  "fw_url", ".*", false);
    updateBigStrField(cdata, "FW.CERT", "fw_cert", ".*");
    END_UPDATERESP(cdata);
    return HTTPD_CGI_DONE;
}


/*****************************************************
 * Template replacer for APRS tracker
 *****************************************************/

CGIFUNC tpl_aprs(HttpdConnData *con, char *token, void **arg) {
	char buf[64];
	if (token==NULL) 
        return HTTPD_CGI_DONE;
    TPL_HEAD(token, con); 
    
    if (strcmp(token, "mycall")==0)
        get_str_param("MYCALL", buf, 10, DFL_MYCALL);
    else if (strcmp(token, "symbol")==0)
        get_str_param("SYMBOL", buf, 64, DFL_SYMBOL);
    else if (strcmp(token, "digipath")==0)
        get_str_param("DIGIPATH", buf, 64, DFL_DIGIPATH);
    else if (strcmp(token, "comment")==0)
        get_str_param("REP.COMMENT", buf, 64, DFL_REP_COMMENT);
    else if (strcmp(token, "maxpause")==0)
        sprintf(buf, "%hu", get_byte_param("MAXPAUSE", DFL_MAXPAUSE));
    else if (strcmp(token, "minpause")==0)
        sprintf(buf, "%hu", get_byte_param("MINPAUSE", DFL_MINPAUSE));
    else if (strcmp(token, "mindist")==0)
        sprintf(buf, "%hu", get_byte_param("MINDIST", DFL_MINDIST));
    else if (strcmp(token, "redundancy")==0)
        sprintf(buf, "%hu", get_byte_param("REPEAT", DFL_REPEAT)); 
    else if (strcmp(token, "turnlimit")==0)
        sprintf(buf, "%u",  get_u16_param("TURNLIMIT", DFL_TURNLIMIT));
    else if (strcmp(token, "txfreq")==0)
        sprintf(buf, "%d",  get_i32_param("TXFREQ", DFL_TXFREQ));
    else if (strcmp(token, "rxfreq")==0)
        sprintf(buf, "%d",  get_i32_param("RXFREQ", DFL_RXFREQ));
    else if (strcmp(token, "timestamp_on")==0)
        TEST_CHECKED(buf, "TIMESTAMP.on");
    else if (strcmp(token, "compress_on")==0)
        TEST_CHECKED(buf, "COMPRESS.on");
    else if (strcmp(token, "altitude_on")==0)
        TEST_CHECKED(buf, "ALTITUDE.on");
    else if (strcmp(token, "xonturn_on")==0)
        TEST_CHECKED(buf, "EXTRATURN.on");
    
    else sprintf(buf, "ERROR");
    httpdSend(con, buf, -1);
	return HTTPD_CGI_DONE;
}



/*****************************************************
 * Template replacer for digipeater/igate config
 *****************************************************/

CGIFUNC tpl_digi(HttpdConnData *con, char *token, void **arg) {
	char buf[64];
	if (token==NULL) 
        return HTTPD_CGI_DONE;
    TPL_HEAD(token, con); 

    if (strcmp(token, "digi_on")==0)
        TEST_CHECKED(buf, "DIGIPEATER.on");
    else if (strcmp(token, "igate_on")==0)
        TEST_CHECKED(buf, "IGATE.on");
    else if (strcmp(token, "wide1_on")==0)
        TEST_CHECKED(buf, "DIGI.WIDE1.on");
    else if (strcmp(token, "sar_on")==0)
        TEST_CHECKED(buf, "DIGI.SAR.on");
    else if (strcmp(token, "igate_host")==0)
        get_str_param("IGATE.HOST", buf, 64, DFL_IGATE_HOST);
    else if (strcmp(token, "igate_port")==0)
        sprintf(buf, "%u", get_u16_param("IGATE.PORT", DFL_IGATE_PORT));
    else if (strcmp(token, "igate_user")==0)
        get_str_param("IGATE.USER", buf, 32, DFL_IGATE_USER);
    else if (strcmp(token, "igate_pass")==0)
        get_str_param("IGATE.PASS", buf, 6, "");
    else sprintf(buf, "ERROR");
    httpdSend(con, buf, -1);
	return HTTPD_CGI_DONE;
}


CGIFUNC tpl_trklog(HttpdConnData *con, char *token, void **arg) {
	char buf[64];
	if (token==NULL) 
        return HTTPD_CGI_DONE;
    TPL_HEAD(token, con); 
        
    if (strcmp(token, "tlog_int")==0)
        sprintf(buf, "%u", get_byte_param("TRKLOG.INT", DFL_TRKLOG_INT));
    else if (strcmp(token, "tlog_ttl")==0)
        sprintf(buf, "%u", get_byte_param("TRKLOG.TTL", DFL_TRKLOG_TTL));
    else if (strcmp(token, "trklog_on")==0)
        TEST_CHECKED(buf, "TRKLOG.on");
    else if (strcmp(token, "tlog_host")==0)
        get_str_param("TRKLOG.HOST", buf, 64, DFL_TRKLOG_HOST);
    else if (strcmp(token, "tlog_port")==0)
        sprintf(buf, "%u", get_u16_param("TRKLOG.PORT", DFL_TRKLOG_PORT));
    else if (strcmp(token, "tlog_path")==0)
        get_str_param("TRKLOG.PATH", buf, 32, DFL_TRKLOG_PATH);
    
    httpdSend(con, buf, -1);
	return HTTPD_CGI_DONE;
}



/*****************************************************
 * Template replacer for system info
 *****************************************************/

CGIFUNC tpl_sysInfo(HttpdConnData *con, char *token, void **arg) {
	char buf[32];
	if (token==NULL) return HTTPD_CGI_DONE;
    TPL_HEAD(token, con); 
    
	if (strcmp(token, "freeHeap")==0) {
		sprintf(buf, "%d", esp_get_free_heap_size());
	}
	else if (strcmp(token, "flashSize")==0) {
		sprintf(buf, "%d", spi_flash_get_chip_size());
	}
    else if (strcmp(token, "ipAddr")==0) {
        wifi_getIpAddr(buf);
	}
    else if (strcmp(token, "macAddr")==0) {
        uint8_t mac[6];
        ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
		sprintf(buf, "%s", mac2str(mac));
	}
	else if (strcmp(token, "apSsid")==0) {
        wifi_getConnectedAp(buf);
    }
    else if (strcmp(token, "vbatt")==0) {
        sprintf(buf, "%1.02f", ((double) adc_batt()) / 1000);
    }
    else if (strcmp(token, "sbatt")==0) {
        char st1[16], st2[16];
        adc_batt_status(st1, st2);
        sprintf(buf, "%s %s", st1, st2);
    }
    
    
    else sprintf(buf, "ERROR");
        
	httpdSend(con, buf, -1);
	return HTTPD_CGI_DONE;
}



/*****************************************************
 * Template replacer for wifi configuration
 *****************************************************/

CGIFUNC tpl_wifi(HttpdConnData *con, char *token, void **arg) {
	char buf[64];
	if (token==NULL) return HTTPD_CGI_DONE;
    TPL_HEAD(token, con); 
    
    if (strncmp(token, "ssid", 4)==0) {
        int index = atoi(token+4);
        wifiAp_t res; 
        if (wifi_getApAlt(index, &res))
            sprintf(buf, "%s", res.ssid);
        else
            buf[0] = '\0';
    }
    else if (strncmp(token, "passwd", 6)==0) {
        int index = atoi(token+6);
        wifiAp_t res; 
        if (wifi_getApAlt(index, &res))
            sprintf(buf, "%s", res.passwd);
        else
            buf[0] = '\0';
    }
    else if (strcmp(token, "apssid")==0) 
        get_str_param("WIFIAP.SSID", buf, 32, default_ssid);
    else if (strcmp(token, "appass")==0)
        get_str_param("WIFIAP.AUTH", buf, 64, AP_DEFAULT_PASSWD);
    
    else if (strcmp(token, "htuser")==0) 
        get_str_param("HTTPD.USR", buf, 32, HTTPD_DEFAULT_USR);
    else if (strcmp(token, "htpass")==0)
        get_str_param("HTTPD.PWD", buf, 64, HTTPD_DEFAULT_PWD);
    
    httpdSend(con, buf, -1);
	return HTTPD_CGI_DONE;
}



/*****************************************************
 * Template replacer for firmware update config
 *****************************************************/

CGIFUNC tpl_fw(HttpdConnData *con, char *token, void **arg) {
    if (token==NULL) return HTTPD_CGI_DONE;
    TPL_HEAD(token, con); 
    
    char* bbuf = malloc(BBUF_SIZE+1);
    if (strcmp(token, "fw_url")==0)
        get_str_param("FW.URL", bbuf, 64, "");
    else if (strcmp(token, "fw_cert")==0) 
        get_str_param("FW.CERT", bbuf, BBUF_SIZE, "");
  
    httpdSend(con, bbuf, -1);
    free(bbuf);
    return HTTPD_CGI_DONE;
}
    

    
/*****************************************************
 * Init and start webserver
 *****************************************************/
EspFsConfig espfs_conf = {
    .memAddr = espfs_image_bin,
};

static bool httpd_on = false;
void httpd_enable(bool on) {
    if (on && !httpd_on) {
        EspFs* fs = espFsInit(&espfs_conf);
        httpdRegisterEspfs(fs);
        
        httpdFreertosInit(&httpdFreertosInstance,
            builtInUrls,
            LISTEN_PORT,
            connectionMemory,
            MAX_CONNECTIONS,
            HTTPD_FLAG_NONE);
        httpdFreertosStart(&httpdFreertosInstance);
        httpd_on = true; 
        ESP_LOGI(TAG, "HTTP Server Ready");
    }
    if (!on && httpd_on) {
#ifdef CONFIG_ESPHTTPD_SHUTDOWN_SUPPORT
        httpdShutdown(&httpdFreertosInstance.httpdInstance); 
        httpd_on = false; 
        ESP_LOGI(TAG, "HTTP Server Shut down");
#endif
    }
}

