/*
 * Self-signed TLS certificate generation and management.
 *
 * On first call to cert_init() the module checks NVS for a previously
 * generated certificate.  If none is found it generates a fresh ECDSA P-256
 * self-signed certificate using mbedTLS, stores both the certificate and the
 * private key in NVS so they survive reboots, and makes them available through
 * cert_get_pem() / cert_get_key_pem() for the HTTPS server.
 *
 * ECC P-256 is chosen over RSA-2048 because key generation is orders of
 * magnitude faster on the ESP32 (milliseconds vs. tens of seconds), and the
 * resulting PEM blobs are much smaller.
 *
 * By LA7ECA, ohanssen@acm.org
 */

#include <string.h>
#include "defines.h"
#include "esp_log.h"
#include "config.h"
#include "cert.h"
#include "networking.h"
#include "mbedtls/pk.h"
#include "mbedtls/ecp.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
#include "mbedtls/version.h"


#define TAG             "cert"

/* PEM output buffer sizes – ECC P-256 cert is ~500 B, key is ~200 B. */
#define CERT_PEM_BUF_SIZE   1024
#define KEY_PEM_BUF_SIZE     512

/* NVS keys (max 15 chars). */
#define NVS_KEY_CERT    "TLS.CERT"
#define NVS_KEY_KEY     "TLS.KEY"
#define NVS_CERT_VER    "TLS.CERT.VER"


static uint8_t _cert_pem[CERT_PEM_BUF_SIZE];
static uint8_t _key_pem[KEY_PEM_BUF_SIZE];
static size_t  _cert_len    = 0;
static size_t  _key_len     = 0;
static bool    _initialized = false;


const uint8_t *cert_get_pem(size_t *len)
{
    if (len)
        *len = _cert_len;
    return _cert_pem;
}


const uint8_t *cert_get_key_pem(size_t *len)
{
    if (len)
        *len = _key_len;
    return _key_pem;
}



/*****************************************************************************
 * Generate a new ECDSA P-256 self-signed certificate and persist it to NVS.
 * All mbedTLS contexts are heap-allocated to avoid large stack frames. 
 *****************************************************************************/

static int _generate(void)
{
    int ret = -1;

    mbedtls_pk_context       *key      = NULL;
    mbedtls_entropy_context  *entropy  = NULL;
    mbedtls_ctr_drbg_context *ctr_drbg = NULL;
    mbedtls_x509write_cert   *cert     = NULL;

    key      = malloc(sizeof(mbedtls_pk_context));
    entropy  = malloc(sizeof(mbedtls_entropy_context));
    ctr_drbg = malloc(sizeof(mbedtls_ctr_drbg_context));
    cert     = malloc(sizeof(mbedtls_x509write_cert));

    if (!key || !entropy || !ctr_drbg || !cert) {
        ESP_LOGE(TAG, "Out of memory while allocating mbedTLS contexts");
        goto cleanup;
    }

    mbedtls_pk_init(key);
    mbedtls_entropy_init(entropy);
    mbedtls_ctr_drbg_init(ctr_drbg);
    mbedtls_x509write_crt_init(cert);

    /* Seed the deterministic RNG from the hardware entropy source. */
    unsigned char pbuf[16];
    const char *pers = mac2str(pbuf);
    ret = mbedtls_ctr_drbg_seed(ctr_drbg, mbedtls_entropy_func, entropy,
                                 (const unsigned char *)pers, strlen(pers));
    if (ret != 0) {
        ESP_LOGE(TAG, "ctr_drbg_seed failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    /* Generate the ECC P-256 key pair. */
    ESP_LOGI(TAG, "Generating ECC P-256 key pair...");
    ret = mbedtls_pk_setup(key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) {
        ESP_LOGE(TAG, "pk_setup failed: -0x%04x", -ret);
        goto cleanup;
    }

    ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(*key),
                               mbedtls_ctr_drbg_random, ctr_drbg);
    if (ret != 0) {
        ESP_LOGE(TAG, "ecp_gen_key failed: -0x%04x", -ret);
        goto cleanup;
    }

    /* Configure the self-signed certificate. */
    mbedtls_x509write_crt_set_version(cert, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(cert, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(cert, key);
    mbedtls_x509write_crt_set_issuer_key(cert, key);

    /* Subject name */
    char dname[128];
    char mycall[11]; 
    get_str_param("MYCALL", mycall, 10, DFL_MYCALL);
    sprintf(dname, "CN=ArcticTracker %lX,OU=%s", chipId(), mycall);
    ret = mbedtls_x509write_crt_set_subject_name(cert, dname);
    
    if (ret != 0) {
        ESP_LOGE(TAG, "set_subject_name failed: -0x%04x", -ret);
        goto cleanup;
    }

    /* Issuer name */
    ret = mbedtls_x509write_crt_set_issuer_name(cert, dname);
    if (ret != 0) {
        ESP_LOGE(TAG, "set_issuer_name failed: -0x%04x", -ret);
        goto cleanup;
    }

    /* Serial number. Incremented each time a new cert is generated */
    uint32_t serial = get_u32_param("TLS.CERT.SER", 0);
    serial++;
    set_u32_param("TLS.CERT.SER", serial);
    
    int i=0;
    unsigned char sbuf[4];
    sbuf[0] = (serial >> 24) & 0xFF; 
    sbuf[1] = (serial >> 16) & 0xFF; 
    sbuf[2] = (serial >> 8) & 0xFF; 
    sbuf[3] = serial & 0xFF;
    if (sbuf[0] == 0x00 && !(sbuf[1] & 0x80)) i++;
    if (sbuf[1] == 0x00 && !(sbuf[2] & 0x80)) i++;
    if (sbuf[2] == 0x00 && !(sbuf[3] & 0x80)) i++;
    
    ret = mbedtls_x509write_crt_set_serial_raw(cert, ((uint8_t*) sbuf)+i, 4-i);
    if (ret != 0) {
        ESP_LOGE(TAG, "set_serial_raw failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    /* Validity */
    ret = mbedtls_x509write_crt_set_validity(cert, CERT_NOT_BEFORE, CERT_NOT_AFTER);
    if (ret != 0) {
        ESP_LOGE(TAG, "set_validity failed: -0x%04x", -ret);
        goto cleanup;
    }

    /* Export private key as PEM (null-terminated). */
    ret = mbedtls_pk_write_key_pem(key, _key_pem, KEY_PEM_BUF_SIZE);
    if (ret != 0) {
        ESP_LOGE(TAG, "pk_write_key_pem failed: -0x%04x", -ret);
        goto cleanup;
    }
    _key_len = strlen((char *)_key_pem) + 1;

    /* Export certificate as PEM (null-terminated). */
    ret = mbedtls_x509write_crt_pem(cert, _cert_pem, CERT_PEM_BUF_SIZE,
                                    mbedtls_ctr_drbg_random, ctr_drbg);
    if (ret != 0) {
        ESP_LOGE(TAG, "x509write_crt_pem failed: -0x%04x", -ret);
        goto cleanup;
    }
    _cert_len = strlen((char *)_cert_pem) + 1;

    /* Persist to NVS so the same certificate survives reboots. */
    set_bin_param(NVS_KEY_CERT, _cert_pem, _cert_len);
    set_bin_param(NVS_KEY_KEY,  _key_pem,  _key_len);
    set_str_param(NVS_CERT_VER, VERSION_SSTRING);

    ESP_LOGI(TAG, "Self-signed certificate generated and stored "
             "(cert=%u bytes, key=%u bytes, %s)",
             (unsigned)_cert_len, (unsigned)_key_len, dname);

cleanup:
    if (cert) {
        mbedtls_x509write_crt_free(cert);
        free(cert);
    }
    if (key) {
        mbedtls_pk_free(key);
        free(key);
    }
    if (ctr_drbg) {
        mbedtls_ctr_drbg_free(ctr_drbg);
        free(ctr_drbg);
    }
    if (entropy) {
        mbedtls_entropy_free(entropy);
        free(entropy);
    }
    return ret;
}


/********************************************************************************
 * Generate a new certificate 
 ********************************************************************************/

bool cert_generate(void) {
    return _generate() == 0;
}



/*******************************************************************************
 * Init certificate. Try to get it from NVS. If not there, generate a new
 * certificate. 
 *******************************************************************************/

bool cert_init(void)
{
    if (_initialized)
        return true;

    /* Try to load an existing certificate from NVS. */
    int clen = get_bin_param(NVS_KEY_CERT, _cert_pem, CERT_PEM_BUF_SIZE, NULL);
    int klen = get_bin_param(NVS_KEY_KEY,  _key_pem,  KEY_PEM_BUF_SIZE,  NULL);

    if (clen > 0 && klen > 0) {
        _cert_len = (size_t)clen;
        _key_len  = (size_t)klen;
        ESP_LOGI(TAG, "Certificate loaded from NVS "
                 "(cert=%u bytes, key=%u bytes)",
                 (unsigned)_cert_len, (unsigned)_key_len);
        _initialized = true;
        return true;
    }

    /* Nothing in NVS – generate a fresh self-signed certificate. */
    ESP_LOGI(TAG, "No certificate found in NVS, generating self-signed certificate...");
    if (_generate() != 0) {
        ESP_LOGE(TAG, "Failed to generate self-signed certificate");
        return false;
    }

    _initialized = true;
    return true;
}
