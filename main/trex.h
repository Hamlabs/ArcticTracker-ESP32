/*
 * Regular expression headers. Code is part of the esp-idf. 
 */

#ifndef _TREX_H_
#define _TREX_H_

#include <stdint.h>

#define MAX_CHAR 0xFF

#ifndef TREX_API
#define TREX_API extern
#endif

#define TRex_True 1
#define TRex_False 0

typedef unsigned int TRexBool;
typedef struct TRex TRex;

typedef struct {
    const char *begin;
    int len;
} TRexMatch;

TREX_API TRex *trex_compile(const char *pattern);
TREX_API void trex_free(TRex *exp);
TREX_API TRexBool trex_match(TRex* exp, const char* text);
TREX_API uint8_t trex_error(TRex* exp);
TREX_API TRexBool trex_search(TRex* exp,const char* text, const char** out_begin, const char** out_end);
TREX_API TRexBool trex_searchrange(TRex* exp,char* text_begin,const char* text_end,const char** out_begin, const char** out_end);
TREX_API int trex_getsubexpcount(TRex* exp);
TREX_API TRexBool trex_getsubexp(TRex* exp, int n, TRexMatch *subexp);

#endif
