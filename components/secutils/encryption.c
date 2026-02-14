/*
 * Encryption utilities
 */

#include <string.h>
#include "encryption.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/md.h"
#include "micro_aes.h"
#include "mbedtls/base64.h"
#include "base91.h"
#include "config.h"
// #include "ax25.h"


#define DERIVED_KEY_LENGTH 32
#define PBKDF2_ITERATIONS 16384
#define SALT_APRSPOS "(9L73)_GHS^:(he#Ob6~C$eQ,yDdxZXx"

uint8_t _key[DERIVED_KEY_LENGTH]; 


#define TAG "secutils"




/**
 * Derive a cryptographic key from a password and salt using PBKDF2
 * 
 * @param buf    Buffer to write the derived key to (must be at least 32 bytes and non-NULL)
 * @param passwd Password string (null-terminated)
 * @param salt   Salt string (null-terminated)
 * @return       Pointer to buf containing the derived key, or NULL on error
 */
static uint8_t * derive_key(uint8_t* buf, const char* passwd, const char* salt)
{
    /* Derive 256-bit AES key from password using PBKDF2 with HMAC-SHA256 */
    /* Validate input parameters */
    if (buf == NULL || passwd == NULL || salt == NULL) 
        return NULL;
    
    /* Derive the key using PBKDF2 */
    int ret = mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
            (const unsigned char *)passwd, strlen(passwd),
            (const unsigned char *)salt, strlen(salt),
            PBKDF2_ITERATIONS,
            DERIVED_KEY_LENGTH,
            buf);
    
    if (ret != 0) 
        return NULL;
    return buf;
}




/**
 * Encrypt a text using a key, and a nonce. 
 */
static uint8_t * gcm_siv_encrypt(uint8_t *buf, uint8_t* key, char* cleartext, size_t size, char* nonce) 
{
    uint8_t ivect[12], aad[1]; 
    memset(ivect, 0, 12);
    strncpy((char*) ivect, nonce, strlen(nonce));
    
    GCM_SIV_encrypt ( 
            key, ivect,
            (void*) aad, 0,                 
            (void*) cleartext, size,  
            buf );    
    return buf;
}



/**
 * Set a key to be used for encryption. 
 * The key is derived from a passphrase using PBKDF2. 
 */ 
void sec_set_key(char* keyphrase) {
    derive_key(_key, keyphrase, SALT_APRSPOS);
}



/**
 *  Encrypt a string using AES-GCM-SIV.
 */
uint8_t * sec_encrypt(uint8_t *buf, char* cleartext, size_t size, char* nonce) {
    return gcm_siv_encrypt(buf, _key, cleartext, size, nonce);
}


/**
 *  Encrypt a string using AES-GCM-SIV. Encode with Base64.
 */
size_t sec_encryptB64(char *res, size_t dsize, char* cleartext, size_t size, char* nonce) {
    size_t csize = size + 16;
    size_t olen;
    uint8_t *buf = (uint8_t*) malloc(csize);
    gcm_siv_encrypt(buf, _key, cleartext, size, nonce);
    
    mbedtls_base64_encode((unsigned char*) res, dsize, &olen, buf, csize);
    free(buf);
    return olen;
}



/**
 *  Encrypt a string using AES-GCM-SIV. Encode with Base91
 */
size_t sec_encryptB91(char *res, size_t dsize, char* cleartext, size_t size, char* nonce) {
    size_t csize = size + 16;
    uint8_t *buf = (uint8_t*) malloc(csize);
    gcm_siv_encrypt(buf, _key, cleartext, size, nonce);
    
    size_t len = encodeBase91(buf, res, csize);
    free(buf);
    return len;
}


bool sec_isEncrypted() {
    return GET_BOOL_PARAM("CRYPTO.on", DFL_CRYPTO_ON);
}


void sec_init() {
    char k[129];
    get_str_param("CRYPTO.KEY", k, 128, DFL_CRYPTO_KEY);
    sec_set_key(k);
}






