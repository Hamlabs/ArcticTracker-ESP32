#include "system.h"
#include "config.h"
#include "gps.h"
#include "trackstore.h"
#include "tracker.h"

#define TAG "tracklog"


static TaskHandle_t tracklogt = NULL; 
static void tracklog(void* arg);
static void remove_old();



/********************************************************
 *  Init tracklogger
 ********************************************************/

void tracklog_init()
{
    if (GET_BYTE_PARAM("TRACKLOG.on"))
        tracklog_on();
}



/********************************************************
 *  Turn on tracklogger 
 ********************************************************/

void tracklog_on() {
    if (tracklogt == NULL)
        xTaskCreatePinnedToCore(&tracklog, "Track logger", 
            STACK_TRACKLOG, NULL, NORMALPRIO, &tracklogt, CORE_TRACKLOG);
}



/********************************************************
 *  Main thread of tracklogger 
 ********************************************************/

void tracklog_off() {
}




/********************************************************
 *  Main thread of tracklogger 
 ********************************************************/

static void tracklog(void* arg) {
    sleepMs(3000);    
    ESP_LOGI(TAG, "Starting tracklog task");
    gps_on();  
    while (GET_BYTE_PARAM("TRACKLOG.on")) {
        uint8_t interv = GET_BYTE_PARAM("TRACKLOGINT"); 
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
    uint32_t tdiff = GET_BYTE_PARAM("TRACKLOG.TTL") * 60 * 60;
    
    if (trackstore_peek(&entry) != NULL)
        if (entry.time + tdiff < now)
            trackstore_getEnt(&entry);
}
