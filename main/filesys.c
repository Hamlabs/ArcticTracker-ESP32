 
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
#include "wear_levelling.h"
#include <string.h>
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
wl_handle_t whandle = WL_INVALID_HANDLE;
wl_handle_t whandle2 = WL_INVALID_HANDLE;



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
        ESP_LOGE(TAG, "Failed to mount FATFS '%s' (%s)", FATFS_LABEL, esp_err_to_name(err));
    } else {
        err = esp_vfs_fat_info(FATFS_PATH, &total, &free);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "FAT fs: '%s' mounted at '%s', %lld bytes, %lld free", FATFS_LABEL, FATFS_PATH, total, free);
        } else {
            ESP_LOGE(TAG, "Failed to get info for FATFS '%s' (%s)", FATFS_LABEL, esp_err_to_name(err));
        }
    }
    
    err = esp_vfs_fat_spiflash_mount_ro(FATFS_PATH2, FATFS_LABEL2, &fatconf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS '%s' (%s)", FATFS_LABEL2, esp_err_to_name(err));
    } else {
        err = esp_vfs_fat_info(FATFS_PATH2, &total, &free);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "FAT fs: '%s' mounted at '%s', %lld bytes, %lld free", FATFS_LABEL2, FATFS_PATH2, total, free);
        } else {
            ESP_LOGE(TAG, "Failed to get info for FATFS '%s' (%s)", FATFS_LABEL2, esp_err_to_name(err));
        }
    }
}


/**************************************************
 *  Reformat main fs
 **************************************************/

void fatfs_format() {
   esp_err_t  err = esp_vfs_fat_spiflash_format_rw_wl(FATFS_PATH, FATFS_LABEL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format FATFS '%s' (%s)", FATFS_LABEL, esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "FATFS '%s' formatted successfully", FATFS_LABEL);
}
    
    
/**************************************************
 *  Get the size of main fs
 **************************************************/

size_t fatfs_size() {
    uint64_t total, free;
    esp_err_t err = esp_vfs_fat_info(FATFS_PATH, &total, &free);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get FATFS size (%s)", esp_err_to_name(err));
        return 0;
    }
    return (size_t) total;
}


/**************************************************
 *  Get the free space of main fs
 **************************************************/

size_t fatfs_free() {
    uint64_t total, free;
    esp_err_t err = esp_vfs_fat_info(FATFS_PATH, &total, &free);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get FATFS free space (%s)", esp_err_to_name(err));
        return 0;
    }
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
    else if (strcmp(wd, "..")==0 && strlen(newwd) > strlen(FATFS_PATH) ) {
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
    else if (strncmp(fname, FATFS_PATH, strlen(FATFS_PATH)) == 0)
        sprintf(path, fname);
    else
        sprintf(path, "%s/%s", workingdir, fname);
}



