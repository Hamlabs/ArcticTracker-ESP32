#include "system.h"
#include "config.h"
#include "gps.h"
#include "trackstore.h"
#include "tracker.h"
#include "networking.h"

#define TAG "tracklog"


static TaskHandle_t tracklogt = NULL; 
static void tracklog(void* arg);
static void remove_old();



/********************************************************
 *  Init tracklogger
 ********************************************************/

void tracklog_init()
{
    if (GET_BYTE_PARAM("TRKLOG.on"))
        tracklog_on();
}



/********************************************************
 *  Turn on tracklogger 
 ********************************************************/

void tracklog_on() {
    set_byte_param("TRKLOG.on", 1);
    if (tracklogt == NULL)
        xTaskCreatePinnedToCore(&tracklog, "Track logger", 
            STACK_TRACKLOG, NULL, NORMALPRIO, &tracklogt, CORE_TRACKLOG);
}



/********************************************************
 *  Turn off tracklogger 
 ********************************************************/

void tracklog_off() {
    set_byte_param("TRKLOG.on", 0);
}




/********************************************************
 *  Main thread of tracklogger 
 ********************************************************/

static void tracklog(void* arg) {
    sleepMs(3000);    
    ESP_LOGI(TAG, "Starting tracklog task");
    gps_on();  
    while (GET_BYTE_PARAM("TRKLOG.on")) {
        uint8_t interv = GET_BYTE_PARAM("TRKLOG.INT"); 
        if (interv==0) interv=10;
        sleepMs(interv * 1000);
        if (gps_is_fixed())
            trackstore_put(&gps_current_pos);
        remove_old(); 
    }
    gps_off(); 
    ESP_LOGI(TAG, "Stopping tracklog task");
    tracklogt = NULL;
    vTaskDelete(NULL);
}




/********************************************************
 *  Remove old entries
 ********************************************************/

static void remove_old() {
    posentry_t entry; 
    uint32_t now = (uint32_t) getTime();
    uint32_t tdiff = GET_BYTE_PARAM("TRKLOG.TTL") * 60 * 60;
    
    if (trackstore_peek(&entry) != NULL)
        if (entry.time + tdiff < now)
            trackstore_getEnt(&entry);
}



/* 796 byte buffer */
#define JS_CHUNK_SIZE 128
#define JS_RECORD_SIZE 64
#define JS_HEAD 28

/********************************************************
 *  Send positions to server using a HTTP POST call
 *  and JSON.
 ********************************************************/

static void post_server() {
    char call[10];
    char* buf = malloc(JS_CHUNK_SIZE * JS_RECORD_SIZE + JS_HEAD +1);
    char host[48], path[48];
    uint16_t port; 
    int len = 0, i=0;
    posentry_t pd; 
    
    /* If empty, just return */
    if (trackstore_getEnt(&pd) == NULL)
        return;
    
    /* Get settings */
    GET_STR_PARAM("MYCALL", call, 10);
    GET_STR_PARAM("TRKLOG.HOST", host, 48);
    GET_STR_PARAM("TRKLOG.PATH", path, 48);
    
    /* Serialise as JSON: 
     *    (call, list of (call, time, lat, lng))
     */
    len += sprintf(buf, "{\"call\":\"%s\", \"pos\":[", call);
    for (;;) {
        len += sprintf(buf, "{\"time\":%u, \"lat\":%u, \"lng\":%u}", pd.time, pd.lat, pd.lng);
        if (trackstore_getEnt(&pd) == NULL || ++i >= JS_CHUNK_SIZE)
            break;
        len += sprintf(buf, ",");
    }
    len += sprintf(buf, "]}");
    
    /* Post it */
    if (http_post(host, port, "text/json", path, buf, len) == 200)
        ESP_LOGI(TAG, "Posted track-log (%d bytes/%d entries) to %s:%d", len, i, host, port);
    else
        ESP_LOGE(TAG, "Couldn't post track-log");
    free(buf);
}





