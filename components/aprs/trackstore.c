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
static posentry_t prev;
static FILE *lastfile=NULL, *firstfile=NULL;

static bool check_rblock();
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
    prev.time = prev.lat = prev.lng = prev.altitude = 0;
    
    /* Get metadata from nvs */
    get_bin_param("tracks.META", &meta, sizeof(ts_meta_t), NULL);
    ESP_LOGD(TAG, "start: first=%d, last=%d, firstblk=%d, lastblk=%d, nblocks=%d" , 
             meta.first, meta.last, meta.firstblk, meta.lastblk, meta.nblocks);
    
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
    meta.nblocks = 1;
    lastfile = firstfile = open_block(meta.lastblk, "a+");
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
    ESP_LOGD(TAG, "put - first=%d, last=%d, firstblk=%d, lastblk=%d, nblocks=%d ",
             meta.first, meta.last, meta.firstblk, meta.lastblk, meta.nblocks );
    /* 
     * How to be sure that the timestamp is correctly 
     * represented as a unsigned 32 bit number?
     */
    entry.time = (uint32_t) x->timestamp;
    entry.lat = x->latitude * POS_RESOLUTION;
    entry.lng = x->longitude * POS_RESOLUTION;
    entry.altitude = x->altitude;
    
    /* Drop it if no change in position in the last 60 seconds */
    if (entry.lat == prev.lat && entry.lng == prev.lng && 
        entry.time < prev.time+60)
            return; 
    prev = entry;
    
    /* If empty */
    if (meta.firstblk == meta.lastblk &&
        meta.first == meta.last ) 
          reset_empty(); 
           
    mutex_lock(mutex); 
    if (meta.last >= BLOCK_SIZE) {
        if (meta.nblocks == MAX_BLOCKS) {
            ESP_LOGE(TAG, "put - store is full");
            mutex_unlock(mutex);
            return;
        }
        /* if block is full, add a new one */
        ESP_LOGD(TAG, "put - switch block");
        if (meta.firstblk != meta.lastblk)
            fclose(lastfile);
        meta.lastblk = (meta.lastblk + 1) % MAX_UINT16;
        meta.nblocks++;
        lastfile = open_block(meta.lastblk, "a+");
        meta.last = 0;
    } 
    
    /* Append the entry to the file */
    meta.last++;
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
    
    if (!check_rblock()) 
        { mutex_unlock(mutex); return NULL; }
        
    read_entry(pbuf, firstfile, meta.first);
    meta.first++;
    set_bin_param("tracks.META", &meta, sizeof(ts_meta_t));
    mutex_unlock(mutex); 
    return pbuf;
}


/****************************************************** 
 * Get and remove a record from the store. 
 * Convert it to a posdata_t datatype. 
 ******************************************************/

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



/************************************************************ 
 * Get the next record from the store without removing it 
 ************************************************************/

posentry_t* trackstore_peek(posentry_t* pbuf) {    
    mutex_lock(mutex);
    
    /* If empty */
    if ((meta.firstblk == meta.lastblk &&
         meta.first == meta.last ) || (!check_rblock())) 
        { mutex_unlock(mutex); return NULL; }

    read_entry(pbuf, firstfile, meta.first);
    mutex_unlock(mutex);
    return pbuf;
}



/******************************************************
 * If we are at the end of the block reading, move
 * to next block. Return false if empty.  
 ******************************************************/

static bool check_rblock() { 
    if (meta.first >= BLOCK_SIZE) {
        if (meta.firstblk == meta.lastblk)
            { reset_empty(); return false; }
        
        /* Move to a newer block */
        ESP_LOGD(TAG, "read - switch block");
        delete_block(meta.firstblk);
        meta.nblocks--;
        meta.firstblk = (meta.firstblk + 1) % MAX_UINT16;
        meta.first = 0;
        fclose(firstfile);
        
        /* Open file or get already open file */
        if (meta.firstblk == meta.lastblk && lastfile!=NULL)
            firstfile=lastfile;
        else
            firstfile = open_block(meta.firstblk, "a+");
        set_bin_param("tracks.META", &meta, sizeof(ts_meta_t));
    }
    return true; 
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
        meta.nblocks = 1;
        firstfile = lastfile = open_block(meta.firstblk, "a+");
        set_bin_param("tracks.META", &meta, sizeof(ts_meta_t));
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
 * Write new entry to the end of a file
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




