
#include "defines.h"
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "espfs_image.h"
#include "cJSON.h"
#include "system.h"
#include "networking.h"
#include "config.h"
#include "sdkconfig.h"
#include "esp_spi_flash.h"
#include "restapi.h"

#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#include "mbedtls/md.h"
#include "mbedtls/platform.h"
#include "cuckoo_filter.h"


#define SCRATCH_BUFSIZE (10240)
#define TAG "rest"


typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;


static rest_server_context_t *context; 
static httpd_handle_t server = NULL;

static esp_err_t is_auth(httpd_req_t *req, char* payload, int plsize);
static void nonce_init(void);




void rest_cors_enable(httpd_req_t *req) {
    // FIXME
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "http://localhost");   
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, PUT, POST, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Credentials", "true");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Arctic-Nonce, Arctic-Hmac");
}


esp_err_t rest_options_handler(httpd_req_t *req) {
    // FIXME
    rest_cors_enable(req);
    httpd_resp_set_hdr(req, "Allow", "GET, PUT, POST, DELETE");
    httpd_resp_sendstr(req, "");
    return ESP_OK;
}


/*******************************************************************************************
 * Serialize and send JSON with the response. 
 *******************************************************************************************/

esp_err_t rest_JSON_send(httpd_req_t *req, cJSON *root) {
    const char *sys_info = cJSON_Print(root);    
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}



/*******************************************************************************************
 * Register REST API method with uri and implementation
 * Typically used through macros (see restapi.h). 
 *******************************************************************************************/

void rest_register(char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r) ) 
{      
    httpd_uri_t system_info_get_uri = {
        .uri = uri,
        .method = method,
        .handler = handler,
        .user_ctx = context
    };
    esp_err_t err = httpd_register_uri_handler(server, &system_info_get_uri);
    if (err == ESP_ERR_INVALID_ARG)
        ESP_LOGE(TAG, "Cannot register method for %s. Null arg.", uri);
    else if (err == ESP_ERR_INVALID_ARG)
        ESP_LOGE(TAG, "Cannot register method for %s. Handler not found.", uri);
}



/*******************************************************************************************
 * Check and get input from HTTP payload. 
 * Typically used through macro (see restapi.h)
 *******************************************************************************************/

esp_err_t rest_get_input(httpd_req_t *req,  char **buf, int *size)
{
    int total_len = req->content_len;
    int cur_len = 0;
    *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, *buf + cur_len, total_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    (*buf)[total_len] = '\0';
    *size = total_len;
    return ESP_OK;
}



/*******************************************************************************************
 * Get, check and parse JSON input from HTTP payload. 
 * Typically used through macro (see restapi.h)
 *******************************************************************************************/

esp_err_t rest_AUTH(httpd_req_t *req) {
    if (is_auth(req, "", 0) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Authorisation failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}



/*******************************************************************************************
 * Get, check and parse JSON input from HTTP payload. 
 * Typically used through macro (see restapi.h)
 *******************************************************************************************/

esp_err_t rest_JSON_input(httpd_req_t *req,  cJSON **json)
{
    char *buf;
    int size;
    
    if (rest_get_input(req, &buf, &size) == ESP_FAIL)
        return ESP_FAIL;
    
    if (is_auth(req, buf, size) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Authorisation failed");
        return ESP_FAIL;
    } 
    
    *json = cJSON_Parse(buf);
    if (*json==NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to parse JSON payload");
        return ESP_FAIL;
    }
    return ESP_OK;
}




/*******************************************************************************************
 * Start http server supporting REST APIs. 
 *******************************************************************************************/

extern void register_api_test(void);

void rest_start(int port, const char *path) 
{
    nonce_init();
    
    /* Allocate context struct */
    context = calloc(1, sizeof(rest_server_context_t));
    strcpy(context->base_path, path);
    
    /* Set up and start HTTP server */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = STACK_HTTPD;
    config.server_port = port;
    config.max_uri_handlers = 32;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting REST HTTP Server on port %d", port);
    httpd_start(&server, &config);
    
    register_api_test();
}



void rest_stop() {
}



#define KEY_SIZE 128
#define HMAC_TRUNC 24
#define HMAC_SHA256_SIZE 32
#define HMAC_KEY_SIZE 64
#define HMAC_B64_SIZE 45
#define NONCE_SIZE 12



static cuckoo_filter_t *nonces1, *nonces2;; 


static void nonce_init() {
    cuckoo_filter_new(&nonces1, 2000, 10, (uint32_t) (time(NULL) & 0xffffffff));
    nonces2 = NULL;
}


static bool nonce_exists(char* n) {
    return ( 
        cuckoo_filter_contains(nonces1, n, strlen(n) ) == CUCKOO_FILTER_OK ||
        (nonces2 != NULL && cuckoo_filter_contains(nonces2, n, strlen(n) ) == CUCKOO_FILTER_OK)
    );
}

static void nonce_add(char* n) {
    if (cuckoo_filter_add(nonces1, n, strlen(n)) == CUCKOO_FILTER_FULL) {
        if (nonces2 != NULL)
            cuckoo_filter_free(&nonces2);
        nonces2 = nonces1;
        cuckoo_filter_new(&nonces1, 5000, 10, (uint32_t) (time(NULL) & 0xffffffff));
        cuckoo_filter_add(nonces1, n, strlen(n));
    }
}    



static esp_err_t is_auth(httpd_req_t *req, char* payload, int plsize) {
    
    char nonce[NONCE_SIZE+1];
    if (httpd_req_get_hdr_value_str(req, "Arctic-Nonce", nonce, NONCE_SIZE+1 ) != ESP_OK) {
        ESP_LOGI(TAG, "Couldn't get header Arctic-Nonce");
        return ESP_FAIL;
    }
    /* In addition to the nonce we should add method, uri, a timestamp, etc... */

    
    /* 
     * Verify HMAC. Get HMAC from header and compute one from 
     * content. Compare. 
     */
    char rhmac[HMAC_B64_SIZE+1];
    char hmac[HMAC_B64_SIZE+1];
    if (httpd_req_get_hdr_value_str(req, "Arctic-Hmac", rhmac, HMAC_B64_SIZE+1) != ESP_OK) {
        ESP_LOGI(TAG, "Couldn't get header Arctic-Hmac");
        return ESP_FAIL;
    }

    compute_hmac("API.KEY", hmac, HMAC_B64_SIZE, (uint8_t*) nonce, NONCE_SIZE, (uint8_t*) payload, plsize);
    if (strncmp(hmac, rhmac, HMAC_TRUNC) != 0) {
        ESP_LOGI(TAG, "HMAC signature doesn't match");
        return ESP_FAIL;
    }
    
    if (nonce_exists(nonce)) {
        ESP_LOGI(TAG, "Duplicate request");
        return ESP_FAIL;
    }
    nonce_add(nonce);
    ESP_LOGI(TAG, "Authorization ok");
    return ESP_OK;
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

char* compute_hmac(const char* keyid, char* res, int hlen, uint8_t* data1, int len1, uint8_t* data2, int len2)
{      
    char key[KEY_SIZE];
    char b64hash[HMAC_B64_SIZE+1];
    uint8_t hash[HMAC_SHA256_SIZE];

    int keylen = GET_STR_PARAM(keyid, key, KEY_SIZE) -1;
    
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
