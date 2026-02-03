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
 * @param buf    Buffer to write the derived key to (must be at least 32 bytes)
 * @param passwd Password string
 * @param salt   Salt string
 * @return       Pointer to buf containing the derived key
 */
uint8_t * crypt_derive_key(uint8_t* buf, char* passwd, char* salt)
{
    /* Derive 256 bit AES key from password using 
     * Use PBKDF2 With HmacSHA256
     */
    mbedtls_md_context_t md_ctx;
    const mbedtls_md_info_t *md_info;
    
    /* Initialize the MD context */
    mbedtls_md_init(&md_ctx);
    
    /* Get MD info for SHA256 */
    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    /* Setup the MD context */
    mbedtls_md_setup(&md_ctx, md_info, 1);
    
    /* Derive the key using PBKDF2 */
    mbedtls_pkcs5_pbkdf2_hmac(&md_ctx,
                               (const unsigned char *)passwd, strlen(passwd),
                               (const unsigned char *)salt, strlen(salt),
                               PBKDF2_ITERATIONS,
                               DERIVED_KEY_LENGTH,
                               buf);
    
    /* Free the MD context */
    mbedtls_md_free(&md_ctx);
    
    return buf;
}
