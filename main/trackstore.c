#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include "system.h"
#include "gps.h"
#include "config.h"
#include "trackstore.h"
#include "errno.h"

static mutex_t mutex;
static ts_meta_t meta;
static uint32_t prev_lat=0, prev_lng=0;
static FILE *lastfile=NULL, *firstfile=NULL;


static FILE* open_block(blkno_t blk, char* perm);
static void delete_block(blkno_t blk);
static void write_entry(posentry_t* entr, FILE* f);
static void read_entry(posentry_t* entr, FILE* f, uint16_t pos);
static void reset_empty();

#define TAG "trackstore"



/******************************************************
 * start service
 ******************************************************/

void trackstore_start() {
    mutex = mutex_create();
    
    mutex_lock(mutex); 
    meta.first = meta.last = 0; 
    meta.lastblk = meta.firstblk = 0;
    meta.nblocks = 1;
    
    /* Get metadata from nvs */
    get_bin_param("tracks.META", &meta, sizeof(ts_meta_t), NULL);
    ESP_LOGI(TAG, "start: first=%d, last=%d, firstblk=%d, lastblk=%d", 
             meta.first, meta.last, meta.firstblk, meta.lastblk);
    
    /* Open file(s) */
    firstfile = open_block(meta.firstblk, "a+");
    if (meta.firstblk == meta.lastblk)
        lastfile = firstfile;
    else
        lastfile = open_block(meta.lastblk, "a+");
    mutex_unlock(mutex);
}



/******************************************************
 * cleanup
 ******************************************************/

void trackstore_reset() {
    mutex_lock(mutex); 
    /* Remove all files */
    uint16_t i = meta.firstblk;
    while (i != meta.lastblk) {
        delete_block(i);
        i = (i+1)%MAX_UINT16;
    }
    delete_block(i);
    
    /* Reset metainfo */
    meta.first = meta.last = 0; 
    meta.lastblk = meta.firstblk = 0;
    set_bin_param("tracks.META", &meta, sizeof(ts_meta_t));
    mutex_unlock(mutex);
}



/******************************************************
 * stop service
 ******************************************************/

void trackstore_stop() {
    mutex_lock(mutex); 
    fclose(lastfile); 
    if (meta.firstblk != meta.lastblk)
        fclose(firstfile);
    mutex_unlock(mutex); 
}



/******************************************************
 * Add a posdata record
 ******************************************************/

void trackstore_put(posdata_t *x) {
    posentry_t entry; 
    ESP_LOGD(TAG, "put - first=%d, last=%d, firstblk=%d, lastblk=%d ",
             meta.first, meta.last, meta.firstblk, meta.lastblk );
    /* 
     * How to be sure that the timestamp is correctly 
     * represented as a unsigned 32 bit number?
     */
    entry.time = (uint32_t) x->timestamp;
    entry.lat = x->latitude * POS_RESOLUTION;
    entry.lng = x->longitude * POS_RESOLUTION;
    entry.altitude = x->altitude;
    
    /* Drop it if no change in position */
    if (entry.lat == prev_lat && entry.lng == prev.lng)
        return; 
    prev_lat = entry.lat; 
    prev_lng = entry.lng;
    
    mutex_lock(mutex); 
    if (++meta.last > BLOCK_SIZE) {
        if (nblocks == MAX_BLOCKS) {
            ESP_LOGE(TAG, "put - store is full");
            mutex_unlock(mutex);
            return;
        }
        /* if block is full, add a new one */
        ESP_LOGD(TAG, "put - switch block");
        if (meta.firstblk != meta.lastblk)
            fclose(lastfile);
        meta.lastblk = (meta.lastblk + 1) % MAX_UINT16;
        lastfile = open_block(meta.lastblk, "a+");
        meta.last = 1;
    } 
    
    /* Append the entry to the file */
    write_entry(&entry, lastfile);
    set_bin_param("tracks.META", &meta, sizeof(ts_meta_t));
    mutex_unlock(mutex); 
}



/****************************************************** 
 * Get and remove a record from the store 
 ******************************************************/

posentry_t* trackstore_getEnt(posentry_t* pbuf) {
    ESP_LOGD(TAG, "get - first=%d, last=%d, firstblk=%d, lastblk=%d ", 
             meta.first, meta.last, meta.firstblk, meta.lastblk  );
    
    mutex_lock(mutex); 
    /* If empty */
    if (meta.firstblk == meta.lastblk &&
        meta.first == meta.last ) 
        {  reset_empty(); 
           mutex_unlock(mutex); return NULL; }
    
    if (meta.first >= BLOCK_SIZE) {
        if (meta.firstblk == meta.lastblk)
            { reset_empty();
              mutex_unlock(mutex); return NULL; }
        
        /* Move to a newer block */
        ESP_LOGD(TAG, "get - switch block");
        delete_block(meta.firstblk);
        meta.firstblk = (meta.firstblk + 1) % MAX_UINT16;
        meta.first = 0;
        fclose(firstfile);
        
        /* Open file or get already open file */
        if (meta.firstblk == meta.lastblk && lastfile!=NULL)
            firstfile=lastfile;
        else
            firstfile = open_block(meta.firstblk, "a+");
    }
    
    read_entry(pbuf, firstfile, meta.first);
    meta.first++;
    set_bin_param("tracks.META", &meta, sizeof(ts_meta_t));
    mutex_unlock(mutex); 
    return pbuf;
}


posdata_t* trackstore_get(posdata_t* pbuf) {
    posentry_t entry;
    if (trackstore_getEnt(&entry) == NULL)
        return NULL;
    
    /* 
     * time_t is a signed number and with a 32 bit timestamp, we have a Y2K38 problem. 
     * We should therefore consider using a 64 bit number. The posentry_t use a unsigned 
     * 32 bit number to save space. 
     */
    pbuf->timestamp = (time_t) entry.time; 
    pbuf->latitude = (float) entry.lat / POS_RESOLUTION;
    pbuf->longitude = (float) entry.lng / POS_RESOLUTION;
    pbuf->altitude = entry.altitude;
    return pbuf;
}




/******************************************************
 * If store is empty, we can call this to reset the
 * whole thing..
 ******************************************************/

static void reset_empty() {
    if (meta.first > 0 || meta.last > 0) {
        ESP_LOGI(TAG, "Empty store - resetting");
        fclose(firstfile); 
        delete_block(meta.firstblk); 
        meta.first = meta.last = 0;
        meta.firstblk = meta.lastblk = 0;
        firstfile = lastfile = open_block(meta.firstblk, "a+");
    }
}



/******************************************************
 * Open file
 ******************************************************/

static FILE* open_block(blkno_t blk, char* perm) {
    char fname[64];
    sprintf(fname, "/files/tracks_blk%u.bin", blk);
    FILE* f = fopen(fname, perm);
    if (f==NULL)
        ESP_LOGW(TAG, "Couldn't open file %s: %s", fname, strerror(errno));
    nblocks++;
    return f;
}



/******************************************************
 * Remove file
 ******************************************************/

static void delete_block(blkno_t blk) {
    char fname[64];
    sprintf(fname, "/files/tracks_blk%u.bin", blk);
    unlink(fname);
    meta.nblocks--;
}



/******************************************************
 * Write new entry to the end of the file
 ******************************************************/

static void write_entry(posentry_t* entr, FILE* f) {
    fwrite(entr, sizeof(posentry_t), 1, f);
}



/******************************************************
 * Read entry from file at specified position
 ******************************************************/

static void read_entry(posentry_t* entr, FILE* f, uint16_t pos) {
    memset(entr, 0, sizeof(posentry_t));
    if ( fseek(f, pos*sizeof(posentry_t), SEEK_SET) == -1) {
        ESP_LOGE(TAG, "read_entry - file seek error: pos=%d. %s", pos, strerror(errno));
        return;
    }
    fread(entr, sizeof(posentry_t), 1, f); 
}




