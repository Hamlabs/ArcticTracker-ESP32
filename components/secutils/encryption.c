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
static uint8_t * gcm_siv_encrypt(uint8_t *buf, uint8_t* key, char* cleartext, char* nonce) 
{
    uint8_t ivect[12], aad[1]; 
    memset(ivect, 0, 12);
    strncpy((char*) ivect, nonce, strlen(nonce));
    
    GCM_SIV_encrypt ( 
            key, ivect,
            (void*) aad, 0,                 
            (void*) cleartext, strlen(cleartext),  
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
uint8_t * sec_encrypt(uint8_t *buf, char* cleartext, char* nonce) {
    return gcm_siv_encrypt(buf, _key, cleartext, nonce);
}


/**
 *  Encrypt a string using AES-GCM-SIV. Encode with Base64.
 */
char * sec_encryptB64(char *res, size_t size, char* cleartext, char* nonce) {
    size_t csize = strlen(cleartext) + 16;
    size_t olen;
    uint8_t *buf = (uint8_t*) malloc(csize);
    gcm_siv_encrypt(buf, _key, cleartext, nonce);
    mbedtls_base64_encode((unsigned char*) res, size, &olen, buf, csize);
    free(buf);
    return res;
}



/**
 *  Encrypt a string using AES-GCM-SIV. Encode with Base91
 */
char * sec_encryptB91(char *res, size_t size, char* cleartext, char* nonce) {
    size_t csize = strlen(cleartext) + 16;
    size_t olen;
    uint8_t *buf = (uint8_t*) malloc(csize);
    gcm_siv_encrypt(buf, _key, cleartext, nonce);
    size_t len = encodeBase91(buf, res, csize);
    res[len] = '\0';
    free(buf);
    return res;
}




void sec_crypt_test() {
    char res[128];
    char* key   = "Hackandcrack"; 
    char* clear = "The quick brown fox jumps over whatever other animals";
    sec_set_key(key);
    sec_encryptB64(res, 128, clear, "NONCE");
    printf("CIPHERTEXT: %s\n", res);
    sec_encryptB91(res, 128, clear, "NONCE");
    printf("CIPHERTEXT: %s\n", res);
}




