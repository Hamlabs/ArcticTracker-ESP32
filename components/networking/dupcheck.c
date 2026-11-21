/* 
 * Copyright (C) 2026 Øyvind Hanssen, LA7ECA
 * 
 * Arctic Tracker - duplicate checker (for nonces, etc..)
 *
 * Arctic Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details: 
 * <https://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include "dupcheck.h"
#include "cuckoo_filter.h"




/*******************************************************************************************
 * Use cuckoo filter to check for duplicates
 *******************************************************************************************/

struct _dupcheck {
    cuckoo_filter_t *nonces1; 
    cuckoo_filter_t *nonces2;
    uint16_t size;
};


dupcheck_t *dupcheck_create(int size) {
    dupcheck_t *dc = malloc(sizeof(struct _dupcheck));
    dc->size = size;
    cuckoo_filter_new(&dc->nonces1, size, 10, (uint32_t) (time(NULL) & 0xffffffff));
    dc->nonces2 = NULL;
    return dc;
}


bool dupcheck_exists(dupcheck_t *dc, char* n) {
    return ( 
        cuckoo_filter_contains(dc->nonces1, n, strlen(n) ) == CUCKOO_FILTER_OK ||
        (dc->nonces2 != NULL && cuckoo_filter_contains(dc->nonces2, n, strlen(n) ) == CUCKOO_FILTER_OK)
    );
}

void dupcheck_add(dupcheck_t *dc, char* n) {
    if (cuckoo_filter_add(dc->nonces1, n, strlen(n)) == CUCKOO_FILTER_FULL) {
        if (dc->nonces2 != NULL)
            cuckoo_filter_free(&dc->nonces2);
        dc->nonces2 = dc->nonces1;
        cuckoo_filter_new(&dc->nonces1, dc->size, 10, (uint32_t) (time(NULL) & 0xffffffff));
        cuckoo_filter_add(dc->nonces1, n, strlen(n));
    }
}    

