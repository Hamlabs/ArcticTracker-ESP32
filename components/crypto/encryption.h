/*
 * Encryption utilities
 */

#ifndef _ENCRYPTION_H
#define _ENCRYPTION_H

#include <stdint.h>
#include <stddef.h>

/**
 * Derive a cryptographic key from a password and salt using PBKDF2
 * 
 * @param buf    Buffer to write the derived key to (must be at least 32 bytes and non-NULL)
 * @param passwd Password string (null-terminated)
 * @param salt   Salt string (null-terminated)
 * @return       Pointer to buf containing the derived key, or NULL on error
 */
uint8_t * crypt_derive_key(uint8_t* buf, const char* passwd, const char* salt);

/**
 * Encrypt data using AES-256-GCM authenticated encryption
 * 
 * This function provides authenticated encryption using AES-256 in Galois/Counter Mode.
 * While the interface follows AES-GCM-SIV (RFC 8452) conventions for compatibility,
 * the implementation uses standard AES-GCM as mbedtls does not provide native AES-GCM-SIV support.
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
            const uint8_t* plaintext, size_t plaintext_len);

#endif /* _ENCRYPTION_H */
