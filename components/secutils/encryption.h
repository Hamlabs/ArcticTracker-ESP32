/*
 * Encryption utilities
 */

#ifndef _ENCRYPTION_H
#define _ENCRYPTION_H

#include <stdint.h>
#include <stdbool.h>


void sec_init(void);
void sec_set_key(char* keyphrase);

uint8_t * sec_derive_key(uint8_t* buf, const char* passwd, const char* salt);
size_t sec_encryptB64(char *res, size_t dsize, char* cleartext, size_t size, char* nonce);
size_t sec_encryptB91(char *res, size_t dsize, char* cleartext, size_t size, char* nonce);

char* sec_sha256_b64(char* res, uint8_t *data, int len);
char* sec_hmac_api(char* res, int hlen, uint8_t* data1, int len1, uint8_t* data2, int len2); 
char* sec_hmac_sapi(char* res, int hlen, uint8_t* data1, int len1, uint8_t* data2, int len2); 
char* sec_hmac(const uint8_t* key, int keylen, char* res, int hlen, uint8_t* data1, int len1, uint8_t* data2, int len2);

bool sec_isEncrypted(void);

#endif /* _ENCRYPTION_H */
