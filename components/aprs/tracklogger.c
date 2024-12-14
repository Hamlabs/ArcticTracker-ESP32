#include <stdio.h>
#include "system.h"
#include "config.h"
#include "gps.h"
#include "trackstore.h"
#include "tracker.h"
#include "networking.h"
#include "tracklogger.h"
#include "restapi.h"


#define TAG "tracklog"


static TaskHandle_t tracklogt = NULL; 
static TaskHandle_t trackpostt = NULL; 
static bool tracklog_running = false; 
static bool trackpost_running = false;
static char statusmsg[32];
static int  posted=0;

static void tracklog(void* arg);
static void post_loop(); 
static void remove_old();



/********************************************************
 *  Init tracklogger
 ********************************************************/

void tracklog_init()
{
    if (GET_BOOL_PARAM("TRKLOG.on", DFL_TRKLOG_ON))
        tracklog_on();
}



/********************************************************
 *  Turn on or off tracklogger 
 ********************************************************/

void tracklog_on() {
    if (tracklog_running)
        return;
    xTaskCreatePinnedToCore(&tracklog, "Track logger", 
        STACK_TRACKLOG, NULL, NORMALPRIO, &tracklogt, CORE_TRACKLOG);
}


void tracklog_off() {
}



/********************************************************
 *  short status text (for display)
 ********************************************************/

char* tracklog_status() {
    return statusmsg;
}


int tracklog_nPosted() {
    return posted;
}


/********************************************************
 *  Start or stop task for automatic posting
 ********************************************************/

void tracklog_post_start() 
{  
    if (trackpost_running)
        return;
    if (GET_BOOL_PARAM("TRKLOG.POST.on", DFL_TRKLOG_POST_ON) && !trackpost_running && wifi_isConnected())        
        xTaskCreatePinnedToCore(&post_loop, "Trklog POSTer",
            STACK_TRACKLOGPOST, NULL, NORMALPRIO, &trackpostt, CORE_TRACKLOGPOST);
}


 
void tracklog_post_stop() 
{
}




/********************************************************
 *  Main thread of tracklogger. 
 *  It saves GPS position at the given interval. 
 ********************************************************/

static void tracklog(void* arg) {
    sleepMs(10000);    
    ESP_LOGI(TAG, "Starting tracklog task");
    tracklog_running = true;
    gps_on();  
    while (GET_BOOL_PARAM("TRKLOG.on", DFL_TRKLOG_ON)) {
        uint8_t interv = get_byte_param("TRKLOG.INT", DFL_TRKLOG_INT); 
        if (interv==0) interv=10;
        sleepMs(interv * 1000);
        if (gps_is_fixed())
            trackstore_put(&gps_current_pos);
        remove_old(); 
    }
    gps_off(); 
    ESP_LOGI(TAG, "Stopping tracklog task");
    tracklog_running = false; 
    vTaskDelete(NULL);
}




/***********************************************************
 *  Remove old entries. 
 *  Removes the oldest entry if it is older than TRKLOG.TTL 
 *  which is the max storage time in hours. 
 ***********************************************************/

static void remove_old() {
    posentry_t entry; 
    uint32_t now = (uint32_t) getTime();
    if (now==0)
        return;
    uint32_t ttl = get_byte_param("TRKLOG.TTL", DFL_TRKLOG_TTL);
    uint32_t tdiff = ttl * 60 * 60;
    
    if (trackstore_peek(&entry) != NULL) 
        if ((entry.time + tdiff) < now) {
            ESP_LOGI(TAG, "Removing oldest entry");
            trackstore_getEnt(&entry);
        }
}


/********************************************************
 *  Send positions to server using a HTTP POST call
 *  and JSON.
 ********************************************************/

int tracklog_post() {
    char call[10];
    char* buf = malloc(JS_CHUNK_SIZE * JS_RECORD_SIZE + JS_HEAD +50);
    char url[64]; 
    int len = 0, i=0;
    posentry_t pd; 

    /* If empty, just return */
    if (trackstore_getEnt(&pd) == NULL)
        return 0;
    
    /* Get settings */
    GET_STR_PARAM("MYCALL", call, 10);
    get_str_param("TRKLOG.URL", url, 64, DFL_TRKLOG_URL);

    /* 
     * Serialise as JSON: 
     *   (call, list-of (call, time, lat, lng))
     */
    len += sprintf(buf+len, "{\"call\":\"%s\", \"pos\":[\n", call);
    for (;;) {
        len += sprintf(buf+len, "{\"time\":%lu, \"lat\":%lu, \"lng\":%lu}", pd.time, pd.lat, pd.lng);
        if (++i >= JS_CHUNK_SIZE)
            break;
        if (trackstore_getEnt(&pd) == NULL)
            break;
        len += sprintf(buf+len, ",\n");
    }
    len += sprintf(buf+len, "]}");
    
    
    
    /* Post it */
    for (int j=0; j<3; j++) {
        int status = rest_post(url, "arctic", buf, len, "TRKLOG.KEY");
    
        if (status == 200) {
            ESP_LOGI(TAG, "Posted track-log (%d bytes/%d entries) to %s", len, i, url);
            sprintf(statusmsg, "Posted %d reports OK", i);
            posted += i;
            break;
        }
        else {
            ESP_LOGW(TAG, "Post of track-log failed. Status=%d", status);
            sprintf(statusmsg, "Post failed. code=%d", status);
            /* Wait one minute */
            sleepMs(1000 * 60);
        }
    }
    free(buf);
    return i;
}





static void post_loop() 
{       
    sleepMs(5000);    
    ESP_LOGI(TAG, "Starting trklog POST task");
    sprintf(statusmsg, "POST task running");
    trackpost_running = true;
    while (wifi_isConnected() && GET_BOOL_PARAM("TRKLOG.POST.on", DFL_TRKLOG_POST_ON)) {
        int n = tracklog_post();
        if (n == 0)
            sleepMs(1000 * 240);
        else if (n < 24) 
            sleepMs(1000 * 90);
        else
            sleepMs(1000 * 20);
    }
    trackpost_running = false;
    ESP_LOGI(TAG, "Stopping trklog POST task");   
    sprintf(statusmsg, "POST task stopped");   
    vTaskDelete(NULL);
}



