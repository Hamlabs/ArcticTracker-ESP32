#include "system.h"
#include "config.h"
#include "gps.h"
#include "trackstore.h"
#include "tracker.h"
#include "networking.h"
#include "tracklogger.h"


#define TAG "tracklog"


static TaskHandle_t tracklogt = NULL; 
static TaskHandle_t trackpostt = NULL; 
static bool trackpost_running = false;

static void tracklog(void* arg);
static void post_loop(); 
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
 *  Start task for automatic posting
 ********************************************************/

void tracklog_post_start() 
{
    if (!trackpost_running && wifi_isConnected())
        xTaskCreatePinnedToCore(&post_loop, "Trklog POSTer",
            STACK_TRACKLOGPOST, NULL, NORMALPRIO, &trackpostt, CORE_TRACKLOGPOST);
}




/********************************************************
 *  Main thread of tracklogger. 
 *  It saves GPS position at the given interval. 
 ********************************************************/

static void tracklog(void* arg) {
    sleepMs(10000);    
    ESP_LOGI(TAG, "Starting tracklog task");
    gps_on();  
    while (get_byte_param("TRKLOG.on", 0)) {
        uint8_t interv = get_byte_param("TRKLOG.INT", DFL_TRKLOG_INT); 
        if (interv==0) interv=10;
        sleepMs(interv * 1000);
        gps_fake();
        if (true || gps_is_fixed())
            trackstore_put(&gps_current_pos);
        remove_old(); 
    }
    gps_off(); 
    ESP_LOGI(TAG, "Stopping tracklog task");
    tracklogt = NULL;
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
    char* buf = malloc(JS_CHUNK_SIZE * JS_RECORD_SIZE + JS_HEAD +1);
    char host[48], path[48];
    uint16_t port; 
    int len = 0, i=0;
    posentry_t pd; 
    
    /* If empty, just return */
    if (trackstore_getEnt(&pd) == NULL)
        return 0;
    
    /* Get settings */
    GET_STR_PARAM("MYCALL", call, 10);
    port = get_u16_param("TRKLOG.PORT", DFL_TRKLOG_PORT);
    GET_STR_PARAM("TRKLOG.HOST", host, 48);
    GET_STR_PARAM("TRKLOG.PATH", path, 48);
    
    /* Serialise as JSON: 
     *    (call, list of (call, time, lat, lng))
     */
    len = sprintf(buf, "{\"call\":\"%s\", \"pos\":[\n", call);
    for (;;) {
        len += sprintf(buf+len, "{\"time\":%u, \"lat\":%u, \"lng\":%u}", pd.time, pd.lat, pd.lng);
        if (++i >= JS_CHUNK_SIZE)
            break;
        if (trackstore_getEnt(&pd) == NULL)
            break;
        len += sprintf(buf+len, ",\n");
    }
    len += sprintf(buf+len, "]}");
    
    /* Post it */
    if (http_post(host, port, "text/json", path, buf, len) == 200)
        ESP_LOGI(TAG, "Posted track-log (%d bytes/%d entries) to %s:%d", len, i, host, port);
//        printf("POST:\n%s\n", buf);
    else
        ESP_LOGE(TAG, "Couldn't post track-log");
    free(buf);
    return i;
}



static void post_loop() 
{       
    sleepMs(3000);    
    ESP_LOGI(TAG, "Starting trklog POST task");
    trackpost_running = true;
    while (wifi_isConnected() && GET_BYTE_PARAM("TRKLOG.POST.on")) {
        int n = tracklog_post();
        if (n == 0)
            sleepMs(1000 * 120);
        else if (n < 16) 
            sleepMs(1000 * 60);
        else
            sleepMs(1000 * 20);
    }
    trackpost_running = false;
    ESP_LOGI(TAG, "Stopping trklog POST task");   
    vTaskDelete(NULL);
}





