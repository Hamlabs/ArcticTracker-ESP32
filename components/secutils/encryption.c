/*
 * Encryption utilities
 */

#include <string.h>
#include "encryption.h"
#include "esp_log.h"
#include "micro_aes.h"
#include "mbedtls/base64.h"
#include "base91.h"
#include "config.h"
#include "esp_random.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#include "mbedtls/md.h"
#include "mbedtls/platform.h"


#define SHA256_SIZE 32
#define SHA256_B64_SIZE 44
#define HMAC_SHA256_SIZE 32
#define HMAC_KEY_SIZE 64
#define HMAC_B64_SIZE 45

#define DERIVED_KEY_LENGTH 32
#define PBKDF2_ITERATIONS 16384
#define SALT_APRSPOS "(9L73)_GHS^:(he#Ob6~C$eQ,yDdxZXx"
#define SALT_SAPIKEY "*E^o2Zse@!_rQp:kL%{4qL.~!v[n&HS)"
#define SALT_APIKEY  "Qcb_N56Z9e@A1.8),&#++ekwR]?xc<y_"



static uint8_t _cryptkey[DERIVED_KEY_LENGTH]; 
static uint8_t _apikey[DERIVED_KEY_LENGTH]; 
static uint8_t _sapikey[DERIVED_KEY_LENGTH]; 

#define TAG "secutils"




/**
 * Derive a cryptographic key from a password and salt using PBKDF2
 * 
 * @param buf    Buffer to write the derived key to (must be at least 32 bytes and non-NULL)
 * @param passwd Password string (null-terminated)
 * @param salt   Salt string (null-terminated)
 * @return       Pointer to buf containing the derived key, or NULL on error
 */
uint8_t * sec_derive_key(uint8_t* buf, const char* passwd, const char* salt)
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
 * Set a key to be used for API authentication. 
 * The key is derived from a passphrase using PBKDF2. 
 */ 
void sec_set_apikey(char* keyphrase) {
    sec_derive_key(_apikey, keyphrase, SALT_APIKEY);
}

/**
 * Set a key to be used for encryption. 
 * The key is derived from a passphrase using PBKDF2. 
 */ 
void sec_set_cryptkey(char* keyphrase) {
    sec_derive_key(_cryptkey, keyphrase, SALT_APRSPOS);
}


/**
 * Set a key to be used for server API authentication. 
 * The key is derived from a passphrase using PBKDF2. 
 */ 
void sec_set_sapikey(char* keyphrase) {
    sec_derive_key(_sapikey, keyphrase, SALT_SAPIKEY);
}



/**
 *  Encrypt a string using AES-GCM-SIV.
 */
uint8_t * sec_encrypt(uint8_t *buf, char* cleartext, size_t size, char* nonce) {
    return gcm_siv_encrypt(buf, _cryptkey, cleartext, size, nonce);
}


/**
 *  Encrypt a string using AES-GCM-SIV. Encode with Base64.
 */
size_t sec_encryptB64(char *res, size_t dsize, char* cleartext, size_t size, char* nonce) {
    size_t csize = size + 16;
    size_t olen;
    uint8_t *buf = (uint8_t*) malloc(csize);
    gcm_siv_encrypt(buf, _cryptkey, cleartext, size, nonce);
    
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
    gcm_siv_encrypt(buf, _cryptkey, cleartext, size, nonce);
    
    size_t len = encodeBase91(buf, res, csize);
    free(buf);
    return len;
}


/*******************************************************************************************
 * Generate sha256 hash
 *******************************************************************************************/

char* sec_sha256_b64(char* res, uint8_t *data, int len) 
{
    uint8_t hash[SHA256_SIZE];
    
    mbedtls_sha256_context ss;
    mbedtls_sha256_init (&ss);
    mbedtls_sha256_starts (&ss, 0);
    if (len > 0)
        mbedtls_sha256_update (&ss, (uint8_t*) data, len);
    mbedtls_sha256_finish (&ss, hash);
    
    size_t olen;
    mbedtls_base64_encode((unsigned char*) res, SHA256_B64_SIZE+1, &olen, hash, SHA256_SIZE);
    return res;
}




/********************************************************************************************
 * HMAC based on SHA256 
 * data contains (some representation of) the content. It should not be repeated in a 
 * predictable way. It may be a good idea to include a nonce or timestamp.
 * The hash is converted to a base64 format and returned in the res buffer.
 * 
 * HMAC(H, K) == H(K ^ opad, H(K ^ ipad, text))
 * Implementation adapted from Adrian Perez
 * https://github.com/aperezdc/hmac-sha256/blob/master/hmac-sha256.c
 ********************************************************************************************/


#define I_PAD 0x36
#define O_PAD 0x5C


/* Use a key derived from the API.KEY passphrase */
char* sec_hmac_api(char* res, int hlen, uint8_t* data1, int len1, uint8_t* data2, int len2) 
{
    return sec_hmac(_apikey, DERIVED_KEY_LENGTH, res, hlen, data1, len1, data2, len2);
}


/* Use a key derived from the API.KEY passphrase */
char* sec_hmac_sapi(char* res, int hlen, uint8_t* data1, int len1, uint8_t* data2, int len2) 
{
    return sec_hmac(_sapikey, DERIVED_KEY_LENGTH, res, hlen, data1, len1, data2, len2);
}


/* Use any key that is provided */
char* sec_hmac(const uint8_t* key, int keylen, char* res, int hlen, uint8_t* data1, int len1, uint8_t* data2, int len2)
{      
    char b64hash[HMAC_B64_SIZE+1];
    uint8_t hash[HMAC_SHA256_SIZE];
    
    /* IF key length is more than HMAC_KEY_SIZE, use a hash of it instead */
    if (keylen > HMAC_KEY_SIZE) {
        mbedtls_sha256((unsigned char*) key, keylen, hash, 0);
        memcpy(key, hash, HMAC_SHA256_SIZE);
        keylen = HMAC_SHA256_SIZE;
    }
    /* Consider if this and the padding can be done once and stored. */
    
   /*
     * (1) append zeros to the end of K to create a HMAC_KEY_SIZE byte string
     * (2) XOR (bitwise exclusive-OR) the string computed in step
     *     (1) with ipad
     */
    uint8_t kx[HMAC_KEY_SIZE];
    for (size_t i = 0; i < keylen; i++) kx[i] = I_PAD ^ key[i];
    for (size_t i = keylen; i < HMAC_KEY_SIZE; i++) kx[i] = I_PAD ^ 0;


    /*
     * (3) append the stream of data 'text' to the string resulting
     *     from step (2)
     * (4) apply H to the stream generated in step (3)
     */  
    mbedtls_sha256_context ss;
    mbedtls_sha256_init (&ss);
    mbedtls_sha256_starts (&ss, 0);
    mbedtls_sha256_update (&ss, kx, HMAC_KEY_SIZE);
    if (len1 > 0)
        mbedtls_sha256_update (&ss, (uint8_t*) data1, len1);
    if (len2 > 0)
        mbedtls_sha256_update (&ss, (uint8_t*) data2, len2);
    
    mbedtls_sha256_finish (&ss, hash);
    
    /*
     * (5) XOR (bitwise exclusive-OR) the 64 byte string computed in
     *     step (1) with opad
     */
    for (size_t i = 0; i < keylen; i++) kx[i] = O_PAD ^ key[i];
    for (size_t i = keylen; i < HMAC_KEY_SIZE; i++) kx[i] = O_PAD ^ 0;
    
    /*
     * (6) append the H result from step (4) to the 64 byte string
     *     resulting from step (5)
     * (7) apply H to the stream generated in step (6)
     */
    mbedtls_sha256_init (&ss);
    mbedtls_sha256_starts (&ss, 0);
    mbedtls_sha256_update (&ss, kx, HMAC_KEY_SIZE);
    mbedtls_sha256_update (&ss, hash, HMAC_SHA256_SIZE);
    mbedtls_sha256_finish (&ss, hash);

    size_t olen;
    mbedtls_base64_encode((unsigned char*) b64hash, HMAC_B64_SIZE, &olen, hash, HMAC_SHA256_SIZE  );
    if (hlen >= 0) 
        b64hash[hlen] = 0;
    strcpy(res, b64hash);
    return res;
}


bool sec_isEncrypted() {
    return GET_BOOL_PARAM("CRYPTO.on", DFL_CRYPTO_ON);
}



void sec_init() {
    char k[129];
    get_str_param("CRYPTO.KEY", k, 128, DFL_CRYPTO_KEY);
    sec_set_cryptkey(k);
    get_str_param("API.KEY", k, 128, DFL_API_KEY);
    sec_set_apikey(k);
    get_str_param("TRKLOG.KEY", k, 128, DFL_TRKLOG_KEY);
    sec_set_sapikey(k);
}






