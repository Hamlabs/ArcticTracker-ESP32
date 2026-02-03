/*
 * Encryption utilities
 */

#include "encryption.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/md.h"
#include <string.h>

#define DERIVED_KEY_LENGTH 32
#define PBKDF2_ITERATIONS 10000

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
    /* Derive 256-bit AES key from password using PBKDF2 with HMAC-SHA256 */
    mbedtls_md_context_t md_ctx;
    const mbedtls_md_info_t *md_info;
    int ret;
    
    /* Validate input parameters */
    if (buf == NULL || passwd == NULL || salt == NULL) {
        return NULL;
    }
    
    /* Initialize the MD context */
    mbedtls_md_init(&md_ctx);
    
    /* Get MD info for SHA256 */
    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL) {
        mbedtls_md_free(&md_ctx);
        return NULL;
    }
    
    /* Setup the MD context */
    ret = mbedtls_md_setup(&md_ctx, md_info, 1);
    if (ret != 0) {
        mbedtls_md_free(&md_ctx);
        return NULL;
    }
    
    /* Derive the key using PBKDF2 */
    ret = mbedtls_pkcs5_pbkdf2_hmac(&md_ctx,
                                     (const unsigned char *)passwd, strlen(passwd),
                                     (const unsigned char *)salt, strlen(salt),
                                     PBKDF2_ITERATIONS,
                                     DERIVED_KEY_LENGTH,
                                     buf);
    
    /* Free the MD context */
    mbedtls_md_free(&md_ctx);
    
    if (ret != 0) {
        return NULL;
    }
    
    return buf;
}
