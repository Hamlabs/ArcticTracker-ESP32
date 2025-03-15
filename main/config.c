/*
 * Config parameters. Store in NVS memory (flash). 
 * By LA7ECA, ohanssen@acm.org
 */

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>
#include "config.h"
#include "esp_system.h"
#include "trex.h"
#include "esp_mac.h"


static nvs_handle nvs; 
static uint8_t _nvs_init = 0;

#define TAG "config"


uint32_t chipId() {
    uint8_t chipid[6];
    esp_efuse_mac_get_default(chipid);
    uint32_t cid = chipid[3] << 16 | chipid[4] << 8 | chipid[5]; 
    return cid; 
}



/********************************************************************************
 * Initialize nvs 
 ********************************************************************************/

void nvs_init()
{
    if (_nvs_init)
        return;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    _nvs_init = 1;
}


/********************************************************************************
 * Open/close NVS storage for config
 ********************************************************************************/

int config_open() {
    nvs_init();
    if (nvs_open("CONFIG", NVS_READWRITE, &nvs) != ESP_OK) {
            ESP_LOGE(TAG, "Cannot open NVS");
            return 1;
        }    
    return 0;
}

void config_close() {
    nvs_close(nvs);
}


/********************************************************************************
 * Delete entry/entries
 ********************************************************************************/

void delete_param(const char* key) {
    esp_err_t err = nvs_erase_key(nvs, key); 
    if (err == ESP_ERR_NVS_NOT_FOUND)
        ESP_LOGI(TAG, "Key not found");
    else
        ESP_ERROR_CHECK(err);
}

void delete_all_param() {
    ESP_ERROR_CHECK(nvs_erase_all(nvs));
}


/********************************************************************************
 * Commit changes
 ********************************************************************************/

void commit_param() {
    ESP_ERROR_CHECK(nvs_commit(nvs));
}



/********************************************************************************
 * STRING SETTING 
 ******************************************************************************** 
 * Generic getter/setter command handler for string settings
 ********************************************************************************/

int param_setting_str (int argc, char** argv,
                const char* key, const int size, char* dfl, const char* pattern )
{
    char* data = malloc(size+1);
    char buf[64];
    if (argc < 2) {
        get_str_param(key, data, size, dfl);
        printf("'%s'\n", data);
    }
    else {
        int n = strlen(argv[1]);
        if ((dfl != NULL && n==0) || strcasecmp(argv[1], "reset")==0) {
            delete_param(key);
            printf("OK\n");
        }
        else 
            printf("%s\n", param_parseStr(key, argv[1], size, pattern, buf));
    }
    commit_param();
    free(data);
    return 0;
}


/********************************************************************************
 * String setting. Automatically convert to UPPER CASE
 ********************************************************************************/

int param_setting_ustr(int argc, char** argv,
                const char* key, const int size, char* dfl, const char* pattern )
{
    if (argc >= 2)
        strupr(argv[1]);
    return param_setting_str(argc, argv, key, size, dfl, pattern);
}


/********************************************************************************
 * Input a string to flash and check format, etc.. 
 ********************************************************************************/

char* param_parseStr(const char* key, char* val, const int size, const char* pattern, char* buf)
{
    if (regexMatch(val, pattern)) {
        int n = strlen(val);
        if (n>size)
            val[size] = '\0'; 
        if (n==0)
            delete_param(key);
        else 
            set_str_param(key, val);
        sprintf(buf, "OK");
    }
    else 
        sprintf(buf, "ERROR: Input didn't match required format");
    
    return buf;
}



/********************************************************************************
 * Regular expression matching
 ********************************************************************************/

bool regexMatch(char* str, const char* pattern)
{
    if (pattern != NULL) {
        struct TRex *x = trex_compile(pattern);
        if (!x) 
            ESP_LOGE(TAG, "Regex compile error: '%s'", pattern); 
        else
            if (!trex_match(x, str)) 
                return false; 
        trex_free(x);
    }
    return true; 
}



/************************************************************************
 * BYTE (NUMERIC) SETTING (unsigned 8 bit)
 ************************************************************************
 * Parse and set byte setting (numeric) with upper and lower bounds. 
 ************************************************************************/

int param_setting_byte(int argc, char** argv, const char* key, uint8_t dfl, uint8_t llimit, uint8_t ulimit)
{
    char buf[64];
    if (argc < 2)
        printf("%hu\n", get_byte_param(key, dfl));
    else if (strcasecmp(argv[1], "reset")==0) {
            delete_param(key);
            printf("OK\n");
    } 
    else
        printf("%s\n", param_parseByte(key, argv[1], llimit, ulimit, buf));
    commit_param();
    return 0;
}



char* param_parseByte(const char* key, char* val, uint8_t llimit, uint8_t ulimit, char *buf )
{
    uint8_t n = 0;
    if (sscanf(val, "%hhu", &n) == 1) {
        if (n < llimit)
            sprintf(buf, "ERROR: Value must be more than %hu", llimit);
        else if (n > ulimit)
            sprintf(buf, "ERROR. Value must be less than %hu", ulimit);
        else { 
            sprintf(buf, "OK");
            set_byte_param(key, n);
        }
    }
    else
        sprintf(buf, "ERROR: Input didn't match required format (numeric)");
   
    return buf;
}



/************************************************************************
 * U16 (NUMERIC) SETTING (unsigned 16 bit)
 ************************************************************************
 * Parse and set integer setting (numeric) with upper and lower bounds. 
 ************************************************************************/

int param_setting_u16(int argc, char** argv, const char* key, uint16_t dfl, uint16_t llimit, uint16_t ulimit)
{
    char buf[64];
    if (argc < 2)
        printf("%u\n", get_u16_param(key, dfl));
    else if (strcasecmp(argv[1], "reset")==0) {
        delete_param(key);
        printf("OK\n");
    } 
    else 
        printf("%s\n", param_parseU16(key, argv[1], llimit, ulimit, buf));
    commit_param();
    return 0;
}



char* param_parseU16(const char* key, char* val, uint16_t llimit, uint16_t ulimit, char *buf )
{
    uint16_t n = 0;
    if (sscanf(val, "%hu", &n) == 1) {
        if (n < llimit)
            sprintf(buf, "ERROR: Value must be more than %u", llimit);
        else if (n > ulimit)
            sprintf(buf, "ERROR. Value must be less than %u", ulimit);
        else { 
            sprintf(buf, "OK");
            set_u16_param(key, n);
        }
    }
    else
        sprintf(buf, "ERROR: Input didn't match required format (numeric)");
   
    return buf;
}



/************************************************************************
 * I32 (NUMERIC) SETTING (signed 32 bit)
 ************************************************************************
 * Parse and set integer setting (numeric) with upper and lower bounds. 
 ************************************************************************/

int param_setting_i32(int argc, char** argv, const char* key, int32_t dfl, int32_t llimit, int32_t ulimit)
{
    char buf[64];
    if (argc < 2)
        printf("%ld\n", get_i32_param(key, dfl));
    else if (strcasecmp(argv[1], "reset")==0) {
        delete_param(key);
        printf("OK\n");
    } 
    else 
        printf("%s\n", param_parseI32(key, argv[1], llimit, ulimit, buf));
    commit_param();
    return 0;
}



char* param_parseI32(const char* key, char* val, int32_t llimit, int32_t ulimit, char *buf )
{
    int32_t n = 0;
    if (sscanf(val, "%ld", &n) == 1) {
        if (n < llimit)
            sprintf(buf, "ERROR: Value must be more than or equal to %ld", llimit);
        else if (n > ulimit)
            sprintf(buf, "ERROR. Value must be less than or equal to %ld", ulimit);
        else { 
            sprintf(buf, "OK");
            set_i32_param(key, n);
        }
    }
    else
        sprintf(buf, "ERROR: Input didn't match required format (numeric)");
   
    return buf;
}



/********************************************************************************
 * BOOLEAN (ON/OFF) SETTING
 ******************************************************************************** 
 * Generic getter/setter command handler for boolean settings 
 ********************************************************************************/

int param_setting_bool(int argc, char** argv, const char* key, bool dfl)
{
    char buf[64];
    if (argc < 2)
        printf("%s\n", param_printBool(key, dfl, buf));
    else 
        printf("%s\n", param_parseBool(key, argv[1], buf));
    return 0;
}




/**********************************************************************
 * Produce and return string representation of boolean setting 
 **********************************************************************/

char* param_printBool(const char* key, bool dfl, char* buf) 
{
    if (get_byte_param(key, dfl))
        sprintf(buf, "ON");
    else 
        sprintf(buf, "OFF");
    return buf;
}


/*********************************************************************************
 * Parse and set boolean (on/off) setting 
 *   key - key of setting in nvs_flash
 *   val - text representation of value to be set
 *   buf - buffer for result message
 * 
 * returns buf
 *********************************************************************************/

char* param_parseBool(const char* key, char* val, char* buf)
{
    if (strncasecmp("on", val, 2) == 0 || strncasecmp("true", val, 1) == 0) {
        sprintf(buf, "OK");
        set_byte_param(key, (uint8_t) 1);
    }  
    else if (strncasecmp("off", val, 2) == 0 || strncasecmp("false", val, 1) == 0) {
        sprintf(buf, "OK");
        set_byte_param(key, (uint8_t) 0);
    }
    else 
        sprintf(buf, "ERROR: Input didn't match required format (ON/OFF)");
    commit_param();
    return buf;
}




/********************************************************************************
 * GETTERS/SETTERS
 ********************************************************************************
 * Set entry of various types
 ********************************************************************************/

void set_byte_param(const char* key, uint8_t val) {
    ESP_ERROR_CHECK(nvs_set_u8(nvs, key, val));
}

void set_u16_param(const char* key, uint16_t val) {
    ESP_ERROR_CHECK(nvs_set_u16(nvs, key, val));
}

void set_i32_param(const char* key, int32_t val) {
    ESP_ERROR_CHECK(nvs_set_i32(nvs, key, val));
}

void set_str_param(const char* key, char* val) {
    ESP_ERROR_CHECK(nvs_set_str(nvs, key, val));
}

void set_bin_param(const char* key, const void* val, size_t len) {
    ESP_ERROR_CHECK(nvs_set_blob(nvs, key, val, len));
}




/********************************************************************************
 * get entry of numeric types
 ********************************************************************************/

#define _CHECK(err, key) \
    if (err ==  ESP_ERR_NVS_NOT_FOUND) \
        ESP_LOGI(TAG, "Key '%s' not found. Using default.", key); \
    else \
        ESP_ERROR_CHECK(err);
    
    
uint8_t get_byte_param(const char* key, const uint8_t dfl) {
    uint8_t val = dfl;
    esp_err_t err = nvs_get_u8(nvs, key, &val); 
    _CHECK(err, key);
    return val;
}

uint16_t get_u16_param(const char* key, const uint16_t dfl) {
    uint16_t val = dfl;
    esp_err_t err = nvs_get_u16(nvs, key, &val); 
    _CHECK(err, key);
    return val;
}

int32_t get_i32_param(const char* key, const int32_t dfl) {
    int32_t val = dfl;
    esp_err_t err = nvs_get_i32(nvs, key, &val); 
    _CHECK(err, key);
    return val;
}



/********************************************************************************
 * get entry of string and binary types
 ********************************************************************************/

int get_str_param(const char* key, char* buf, size_t size, const char* dfl) {
    size_t len = size;
    esp_err_t err = nvs_get_str(nvs, key, buf, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Key '%s' not found.", key);
        if (dfl == NULL) {
            buf[0] = '\0';
            return 0;
        }
        strncpy(buf, dfl, (strlen(dfl)+1 > len ? len : strlen(dfl)+1));
    }
    else
        ESP_ERROR_CHECK(err);
    return len;
}


int get_bin_param(const char* key, void* buf, size_t size, const void* dfl) {
    size_t len = size;
    esp_err_t err = nvs_get_blob(nvs, key, buf, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Key '%s' not found.", key);
        if (dfl == NULL)
            // FIXME: what happens if dfl is NULL? 
            return 0;
        memcpy(buf, dfl, size);
    }
    else
        ESP_ERROR_CHECK(err);
    return len;
}


