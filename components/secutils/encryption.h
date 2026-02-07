/*
 * Encryption utilities
 */

#ifndef _ENCRYPTION_H
#define _ENCRYPTION_H

#include <stdint.h>


void sec_set_key(char* keyphrase);
uint8_t * sec_encrypt(uint8_t *buf, char* cleartext, char* nonce);
char * sec_encryptB64(char *res, size_t size, char* cleartext, char* nonce);
char * sec_encryptB91(char *res, size_t size, char* cleartext, char* nonce);
void sec_crypt_test(void);


#endif /* _ENCRYPTION_H */
