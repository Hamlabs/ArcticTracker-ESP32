/*
 * Config parameters. Store in NVS memory (flash). 
 * By LA7ECA, ohanssen@acm.org
 */

#if !defined __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>
#include <stdbool.h>

uint32_t chipId(); 
void nvs_init();
int  config_open();
void config_close();
void delete_param(const char* key);
void delete_all_param();
void commit_param();


int   param_setting_str(int argc, char** argv, 
                const char* key, const int size, char* dfl, const char* regex );
int   param_setting_ustr(int argc, char** argv,
                const char* key, const int size, char* dfl, const char* regex );
int   param_setting_byte(int argc, char** argv, 
                const char* key, uint8_t dfl, uint8_t llimit, uint8_t ulimit );
int   param_setting_u16(int argc, char** argv, 
                const char* key, uint16_t dfl, uint16_t llimit, uint16_t ulimit );
int   param_setting_i32(int argc, char** argv, 
                const char* key, int32_t dfl, int32_t llimit, int32_t ulimit );
int   param_setting_bool(int argc, char** argv, const char* key);

char* param_parseByte(const char* key, char* val, uint8_t llimit, uint8_t ulimit, char* buf );
char* param_parseU16(const char* key, char* val, uint16_t llimit, uint16_t ulimit, char *buf );
char* param_parseI32(const char* key, char* val, int32_t llimit, int32_t ulimit, char *buf );
char* param_printBool(const char* key, char* buf);
char* param_parseBool(const char* key, char* val, char* buf);
char* param_parseStr(const char* key, char* val, const int size, const char* pattern, char* buf);

void     set_byte_param(const char* key, uint8_t val);
void     set_u16_param(const char* key, uint16_t val);
void     set_i32_param(const char* key, int32_t val);
void     set_str_param(const char* key, char* val);
void     set_bin_param(const char* key, const void* val, size_t len); 

uint8_t  get_byte_param(const char* key, const uint8_t dfl);
uint16_t get_u16_param(const char* key, const uint16_t dfl);
int32_t  get_i32_param(const char* key, const int32_t dfl);
int      get_str_param(const char* key, char* buf, size_t size, const char* dfl);
int      get_bin_param(const char* key, void* buf, size_t size, const void* dfl);

bool regexMatch(char* str, const char* pattern);


typedef void (*BoolHandler)(bool val);
typedef void (*ByteHandler)(uint8_t val);

#define GET_BYTE_PARAM(key) get_byte_param(key, 0)
#define GET_U16_PARAM(key)  get_u16_param(key, 0)
#define GET_I32_PARAM(key)  get_i32_param(key, 0)

#define GET_STR_PARAM(key, buf, size) get_str_param(key, buf, size, NULL)
#define GET_BIN_PARAM(key, buf, size) get_bin_param(key, buf, size, NULL)

/* NOTE: This is quite like creating a closure function */
#define CMD_BOOL_SETTING(f, key, bh) \
    inline static int f(int argc, char** argv) { \
        int r = param_setting_bool(argc, argv, key); \
        BoolHandler bhh = bh; \
        if (bhh != NULL) \
            (*bhh)(GET_BYTE_PARAM(key)); \
        return r; \
    } 

#define CMD_BYTE_SETTING(f, key, dfl, llimit, ulimit, bh) \
    inline static int f(int argc, char** argv) { \
        int r = param_setting_byte(argc, argv, key, dfl, llimit, ulimit); \
        ByteHandler bhh = bh; \
        if (bhh != NULL) \
            (*bhh)(GET_BYTE_PARAM(key)); \
        return r; \
    }

#define CMD_U16_SETTING(f, key, dfl, llimit, ulimit) \
    inline static int f(int argc, char** argv) { \
        return param_setting_u16(argc, argv, key, dfl, llimit, ulimit); \
    }

#define CMD_I32_SETTING(f, key, dfl, llimit, ulimit) \
    inline static int f(int argc, char** argv) { \
        return param_setting_i32(argc, argv, key, dfl, llimit, ulimit); \
    }
    
#define CMD_STR_SETTING(f, key, size, dfl, pattern)  \
    inline static int f(int argc, char** argv) { \
        return param_setting_str(argc, argv, key, size, dfl, pattern); \
    }
    
#define CMD_USTR_SETTING(f, key, size, dfl, pattern)  \
    inline static int f(int argc, char** argv) { \
        return param_setting_ustr(argc, argv, key, size, dfl, pattern); \
    }


#endif
