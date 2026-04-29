 
/* 
 * Security for REST API. 
 * Implement HMAC signatures and nonces 
 */

#include <stddef.h>
#include "defines.h"
#include "config.h"
#include "system.h"
#include "cuckoo_filter.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#include "mbedtls/md.h"
#include "mbedtls/platform.h"
#include "restapi.h"
#include "encryption.h"



#define KEY_SIZE 128
#define SHA256_B64_SIZE 44
#define HMAC_TRUNC 24
#define HMAC_SHA256_SIZE 32
#define HMAC_KEY_SIZE 64
#define HMAC_B64_SIZE 45
#define NONCE_SIZE 12
#define NONCE_BIN_SIZE 8
#define HTTPAUTH_SIZE 128


#define TAG "rest"


/*******************************************************************************************
 * Use cuckoo filter to check for duplicate nonces.
 *******************************************************************************************/

static cuckoo_filter_t *nonces1, *nonces2;; 


void nonce_init() {
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
        cuckoo_filter_new(&nonces1, 2000, 10, (uint32_t) (time(NULL) & 0xffffffff));
        cuckoo_filter_add(nonces1, n, strlen(n));
    }
}    



/*******************************************************************************************
 * Add auth headers to client request
 *   - client - request 
 *   - service - name of the service or user 
 *   - data - content
 *   - dlen - length of the content to use for the hmac
 *   - key - storage-key for the key to be used for the hmac
 *******************************************************************************************/

void rest_setSecHdrs(esp_http_client_handle_t client, char* service, char* data, int dlen, char* key)
{
    char hmac[HMAC_B64_SIZE+1];
    uint8_t bnonce[NONCE_BIN_SIZE+1];
    char nonce[NONCE_SIZE+1];
    size_t olen;
    char httpauth[HTTPAUTH_SIZE+1];
    char chash[SHA256_B64_SIZE+1];
    
    /* Create nonce */
    esp_fill_random(bnonce, NONCE_BIN_SIZE);
    mbedtls_base64_encode((unsigned char*) nonce, NONCE_SIZE+1, &olen, bnonce, NONCE_BIN_SIZE  );
    nonce[NONCE_SIZE] = 0;
    
    /* Create a SHA256 hash of the content */
    if (dlen > 0)
        sec_sha256_b64(chash, (uint8_t*) data, dlen ); 
    
    ESP_LOGI(TAG, "CHASH: %s", chash);

    /* Create hmac */
    sec_hmac_sapi(hmac, HMAC_B64_SIZE, 
        (uint8_t*) nonce, NONCE_SIZE, (uint8_t*) chash , (dlen>0 ? SHA256_B64_SIZE : 0));

    /* Set header */
    sprintf(httpauth, "Arctic-Hmac %s;%s;%s", service, nonce, hmac);
    ESP_LOGI(TAG, "HTTP AUTH: %s", httpauth);
    esp_http_client_set_header(client, "Authorization", httpauth);
    
} 



/*******************************************************************************************
 * Authenticate a HTTP request using hmac scheme. 
 * Assume that the request contains two headers: 
 *   Arctic-Nonce - a number which is different each time a request is made. 
 *   Arctic-Hmac  - a HMAC computed from the content and a secret key. 
 *
 * We compute a HMAC using the nonce + the payload and compare it with the HMAC contained
 * in the header. We return ESP_OK if nonce is not seen before and the HMACs match.  
 * The key should be stored in config "API.KEY" (nvs storage). 
 * 
 * NOTE: We want to move to the more standard Authorization header like this: 
 *      Authorization: Arctic-Hmac userid;nonce,hmac (where userid is 'tracker'
 * 
 * See https://polaricserver.readthedocs.io/en/latest/clientauth.html#http-requests
 * 
 * NOTE: For now, we use the payload directly in computing the MAC. Polaric Server 
 * use a SHA256 hash of it. 
 *******************************************************************************************/

esp_err_t rest_isAuth(httpd_req_t *req, char* payload, int plsize) 
{    
    char nonce[NONCE_SIZE+1];
    char rhmac[HMAC_B64_SIZE+1];
    char hmac[HMAC_B64_SIZE+1];
    char httpauth[HTTPAUTH_SIZE+1];
    char* tokens[4];
    int tnum;
    bool authhdr_ok = false;
    
    /* First, try the Authorization header */
    if (httpd_req_get_hdr_value_str(req, "Authorization", httpauth, HTTPAUTH_SIZE+1) != ESP_OK)
        ESP_LOGD(TAG, "Couldn't get 'Authorization' header");
    else
        if (strncmp(httpauth, "Arctic-Hmac", 11) != 0)
            ESP_LOGI(TAG, "Authorization header is not 'Arctic-Hmac'");
        else {
            tnum = tokenize(httpauth+12, tokens, 4, ";", false);
            if (tnum < 3 || strcmp("tracker", tokens[0]) !=0) {
                ESP_LOGI(TAG, "Authorization header format error or unknown userid");
                return ESP_FAIL;
            }
            else {
                strncpy(nonce, tokens[1], NONCE_SIZE);
                nonce[NONCE_SIZE] = '\0';
                strncpy(rhmac, tokens[2], HMAC_B64_SIZE);
                rhmac[HMAC_B64_SIZE] = '\0';
                authhdr_ok = true;
            }
        }
    
    if (!authhdr_ok) {
        /* Fallback on Arctic-Hmac/Arctic-Nonce headers */
        if (httpd_req_get_hdr_value_str(req, "Arctic-Nonce", nonce, NONCE_SIZE+1 ) != ESP_OK) {
            ESP_LOGI(TAG, "Couldn't get header 'Arctic-Nonce'");
            return ESP_FAIL;
        }
        
       /* Get HMAC from header */
        if (httpd_req_get_hdr_value_str(req, "Arctic-Hmac", rhmac, HMAC_B64_SIZE+1) != ESP_OK) {
            ESP_LOGI(TAG, "Couldn't get header 'Arctic-Hmac'");
            return ESP_FAIL;
        }
    }
    
    /* Verify HMAC */
    char phash[SHA256_B64_SIZE+1];
    if (plsize > 0) 
        sec_sha256_b64(phash, (uint8_t*) payload, plsize);
    
    sec_hmac_api(hmac, HMAC_B64_SIZE, 
        (uint8_t*) nonce, NONCE_SIZE, 
        (uint8_t*) (plsize==0 ? "": phash), (plsize==0 ? 0 : SHA256_B64_SIZE));
            
    if (strncmp(hmac, rhmac, HMAC_TRUNC) != 0) {
        ESP_LOGI(TAG, "HMAC signature doesn't match");
        ESP_LOGD(TAG, "'%s' != '%s'", hmac, rhmac); 
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




