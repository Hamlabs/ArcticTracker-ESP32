/*
 * Duplicate checker for nonces, etc.. Based on cuckoo filters. 
 * By LA7ECA, ohanssen@acm.org
 */ 

#ifndef _DUPCHECK_H_
#define _DUPCHECK_H_

#include <stdbool.h>


typedef struct _dupcheck dupcheck_t; 

dupcheck_t *dupcheck_create(int size);
bool dupcheck_exists(dupcheck_t *dc, char* n);
void dupcheck_add(dupcheck_t *dc, char* n); 

#endif
