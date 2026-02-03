/*
 * Encryption utilities
 */

#ifndef _ENCRYPTION_H
#define _ENCRYPTION_H

#include <stdint.h>

/**
 * Derive a cryptographic key from a password and salt using PBKDF2
 * 
 * @param buf    Buffer to write the derived key to (must be at least 32 bytes)
 * @param passwd Password string
 * @param salt   Salt string
 * @return       Pointer to buf containing the derived key, or NULL on error
 */
uint8_t * crypt_derive_key(uint8_t* buf, const char* passwd, const char* salt);

#endif /* _ENCRYPTION_H */
