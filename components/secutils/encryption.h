/*
 * Encryption utilities
 */

#ifndef _ENCRYPTION_H
#define _ENCRYPTION_H

#include <stdint.h>

void sec_init(void);
void sec_set_key(char* keyphrase);

size_t sec_encryptB64(char *res, size_t dsize, char* cleartext, size_t size, char* nonce);
size_t sec_encryptB91(char *res, size_t dsize, char* cleartext, size_t size, char* nonce);
void sec_crypt_test(void);


#endif /* _ENCRYPTION_H */
