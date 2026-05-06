/*
 * Self-signed TLS certificate generation and management.
 * By LA7ECA, ohanssen@acm.org
 */

#ifndef _CERT_H_
#define _CERT_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


/* Generate a new certificate */
bool cert_generate(void); 


/*
 * Load the TLS certificate and private key from NVS.  If no certificate is
 * found a new ECDSA P-256 self-signed certificate is generated and persisted
 * so it survives reboots.
 *
 * Returns true on success, false if generation failed.
 */
bool cert_init(void);

/* Return the PEM-encoded certificate (null-terminated, length includes '\0'). */
const uint8_t *cert_get_pem(size_t *len);

/* Return the PEM-encoded private key (null-terminated, length includes '\0'). */
const uint8_t *cert_get_key_pem(size_t *len);

#endif /* _CERT_H_ */
