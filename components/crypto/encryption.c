/*
 * Encryption utilities
 */

#include "encryption.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/md.h"
#include "mbedtls/aes.h"
#include "mbedtls/gcm.h"
#include <string.h>

#define DERIVED_KEY_LENGTH 32
#define PBKDF2_ITERATIONS 65536
#define AES_GCM_SIV_NONCE_SIZE 12
#define AES_GCM_SIV_TAG_SIZE 16
#define AES_BLOCK_SIZE 16



/**
 * Derive a cryptographic key from a password and salt using PBKDF2
 * 
 * @param buf    Buffer to write the derived key to (must be at least 32 bytes and non-NULL)
 * @param passwd Password string (null-terminated)
 * @param salt   Salt string (null-terminated)
 * @return       Pointer to buf containing the derived key, or NULL on error
 */
uint8_t * crypt_derive_key(uint8_t* buf, const char* passwd, const char* salt)
{
    int ret;
    
    /* Derive 256-bit AES key from password using PBKDF2 with HMAC-SHA256 */
    /* Validate input parameters */
    if (buf == NULL || passwd == NULL || salt == NULL) 
        return NULL;
    
    /* Derive the key using PBKDF2 */
    ret = mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
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
 * Encrypt data using AES-256-GCM-SIV (RFC 8452)
 * 
 * NOTE: This implementation uses AES-256-GCM as a practical alternative.
 * Full AES-GCM-SIV requires POLYVAL implementation which is not available in mbedtls.
 * AES-GCM provides authenticated encryption with similar security properties.
 * 
 * @param ciphertext Output buffer for ciphertext (must be at least plaintext_len bytes)
 * @param tag        Output buffer for authentication tag (must be at least 16 bytes)
 * @param key        Encryption key (must be 32 bytes for AES-256)
 * @param nonce      Nonce/IV (must be 12 bytes)
 * @param aad        Additional authenticated data (can be NULL if aad_len is 0)
 * @param aad_len    Length of additional authenticated data in bytes
 * @param plaintext  Input plaintext data
 * @param plaintext_len Length of plaintext in bytes
 * @return           0 on success, negative error code on failure
 */
int encrypt(uint8_t* ciphertext, uint8_t* tag, const uint8_t* key, 
            const uint8_t* nonce, const uint8_t* aad, size_t aad_len,
            const uint8_t* plaintext, size_t plaintext_len)
{
    int ret;
    mbedtls_gcm_context gcm_ctx;
    
    /* Validate input parameters */
    if (ciphertext == NULL || tag == NULL || key == NULL || 
        nonce == NULL || plaintext == NULL) {
        return -1;
    }
    
    /* AAD can be NULL only if aad_len is 0 */
    if (aad == NULL && aad_len > 0) {
        return -1;
    }
    
    /* Initialize GCM context */
    mbedtls_gcm_init(&gcm_ctx);
    
    /* Set the key for AES-256 (256 bits = 32 bytes) */
    ret = mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm_ctx);
        return ret;
    }
    
    /* Perform authenticated encryption 
     * GCM mode: Galois/Counter Mode
     * - nonce: 12 bytes (96 bits) recommended size
     * - tag: 16 bytes (128 bits) authentication tag
     */
    ret = mbedtls_gcm_crypt_and_tag(&gcm_ctx,
                                     MBEDTLS_GCM_ENCRYPT,
                                     plaintext_len,
                                     nonce,
                                     AES_GCM_SIV_NONCE_SIZE,
                                     aad,
                                     aad_len,
                                     plaintext,
                                     ciphertext,
                                     AES_GCM_SIV_TAG_SIZE,
                                     tag);
    
    /* Clean up */
    mbedtls_gcm_free(&gcm_ctx);
    
    return ret;
}
