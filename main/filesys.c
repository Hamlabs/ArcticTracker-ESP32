 
/*
 * File system setup
 * By LA7ECA, ohanssen@acm.org
 */

#include "defines.h" 
#include "system.h"
#include "esp_flash.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "esp_vfs_dev.h"
#include <dirent.h>
       
#define TAG "system"
 
 
  
 
/**************************************************
 *  FATFS FILESYSTEM 
 **************************************************/

#define FATFS_LABEL "storage"
#define FATFS_LABEL2 "webapp"

#define FATFS_PATH "/files"
#define FATFS_PATH2 "/webapp"

static char workingdir[263];
wl_handle_t whandle;



esp_vfs_fat_mount_config_t fatconf = {
        .max_files = 4,
        .format_if_mount_failed = true,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
    };
    
    
/**************************************************
 *  Mount filesystems: Main fs and webapp
 **************************************************/
    
void fatfs_init() {    
    strcpy(workingdir, FATFS_PATH);
    
    uint64_t total, free;
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(FATFS_PATH, FATFS_LABEL, &fatconf, &whandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
    err =  esp_vfs_fat_info(FATFS_PATH, &total, &free);
    ESP_LOGI(TAG, "FAT fs: '%s', %lld bytes, %lld free", FATFS_LABEL, total, free);
    
    
    err = esp_vfs_fat_spiflash_mount_ro(FATFS_PATH2, FATFS_LABEL2, &fatconf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
    err =  esp_vfs_fat_info(FATFS_PATH2, &total, &free);
    ESP_LOGI(TAG, "FAT fs: '%s', %lld bytes, %lld free", FATFS_LABEL2, total, free);
}


/**************************************************
 *  Reformat main fs
 **************************************************/

void fatfs_format() {
   esp_err_t  err = esp_vfs_fat_spiflash_format_rw_wl(FATFS_PATH, FATFS_LABEL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format FATFS (%s)", esp_err_to_name(err));
        return;
    }
}
    
    
/**************************************************
 *  Get the size of main fs
 **************************************************/

size_t fatfs_size() {
    uint64_t total, free;
    esp_vfs_fat_info(FATFS_PATH, &total, &free);
    return (size_t) total;
}


/**************************************************
 *  Get the free space of main fs
 **************************************************/

size_t fatfs_free() {
    uint64_t total, free;
    esp_vfs_fat_info(FATFS_PATH, &total, &free);
    return (size_t) free;
}
    
    
/**************************************************
 *  Change the working directory
 **************************************************/
    
bool changeWD(char* wd) {
    char newwd[263];
    strcpy(newwd, workingdir);    
    if (wd == NULL || strcmp(wd, "/")==0)
        strcpy(newwd, FATFS_PATH);
    else if (strcmp(wd, "..")==0 && strlen(newwd) > strlen("FATFS_PATH") ) {
        int i;
        for (i = strlen(workingdir); workingdir[i-1] != '/'; i--)
            newwd[i-1] = '\0';
        if (newwd[i-2] == '/')
            newwd[i-2] = '\0';
    }
    else if (wd[0] == '/')
        strcpy(newwd, wd);
    else
        sprintf(newwd+strlen(newwd),"/%s", wd);
    
    DIR *dp = opendir(newwd);
    if (dp==NULL) {
        return false;
    }
    (void) closedir (dp);
    strcpy(workingdir, newwd);
    return true;
    
}


/**************************************************
 *  Get the full path for a file
 **************************************************/

void getPath(char* path, char* fname, bool allowroot) {
    if (allowroot && *fname == '/')
        sprintf(path, "%s", fname);
    else if (strncmp(fname, FATFS_PATH, 7) == 0)
        sprintf(path, fname);
    else
        sprintf(path, "%s/%s", workingdir, fname);
}



