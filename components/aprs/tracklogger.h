

/* 796 byte buffer */
#define JS_CHUNK_SIZE 128
#define JS_RECORD_SIZE 64
#define JS_HEAD 28


 
void tracklog_init(void);
void tracklog_on(void);
void tracklog_off(void);
void tracklog_post_start(void);
int tracklog_post(void);
