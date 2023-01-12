
#include "defines.h"
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "cJSON.h"
#include "system.h"
#include "networking.h"
#include "config.h"
#include "restapi.h"
#include "trex.h"
#include "esp_crt_bundle.h"


#define SCRATCH_BUFSIZE (10240)
#define TAG "rest"


typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;


typedef struct rest_sess_context {
    char orig[32];
} rest_sess_context_t;
 
 
static rest_server_context_t *context; 
static httpd_handle_t server = NULL;

static char* get_origin(httpd_req_t *req);



void rest_cors_enable(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", get_origin(req));   
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, PUT, POST, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Credentials", "true");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Arctic-Nonce, Arctic-Hmac");
}


esp_err_t rest_options_handler(httpd_req_t *req) {
    rest_cors_enable(req);
    httpd_resp_set_hdr(req, "Allow", "GET, PUT, POST, DELETE");
    httpd_resp_sendstr(req, "");
    return ESP_OK;
}



/*******************************************************************************************
 * Return origin in requst, IF it matches API.ORIGINS regular expression
 *******************************************************************************************/

static char* get_origin(httpd_req_t *req) {
    char filter[64];
    char origin[32];

    if (req->sess_ctx == NULL)
        req->sess_ctx = malloc(sizeof(rest_sess_context_t));
    char* buf =  ((rest_sess_context_t*) req->sess_ctx)->orig; 
    buf[0]=0;
    
    /* Get regular expression */
    get_str_param("API.ORIGINS", filter, 64, "nohost");
    TRex *rex = trex_compile(filter);
    if (rex==NULL)
        return buf;
    
    /* Get origin header */
    if (httpd_req_get_hdr_value_str(req, "Origin", origin, 32) != ESP_OK) {
        ESP_LOGW(TAG, "Cannot retrieve Origin header");
        trex_free(rex);
        return buf;
    }
    /* Check it and return it if it matches */
    if (trex_match(rex, origin))
        strcpy(buf, origin);
    trex_free(rex);
    return buf;
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
    if (rest_isAuth(req, "", 0) != ESP_OK) {
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
    
    if (rest_isAuth(req, buf, size) != ESP_OK) {
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

extern void register_api_rest(void);

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
    
    register_api_rest();
}



void rest_stop() {
}




/***************************************************************************
 * HTTP post with HMAC authentication. 
 *  - URL, data, length-of-data
 ***************************************************************************/

esp_err_t rest_post(char* uri, char* data, int dlen) 
{
    esp_http_client_config_t config = {
        .url = uri,
        .method = HTTP_METHOD_POST, 
        
        /* We may configure this? See OTA */
        .cert_pem = NULL,
        .crt_bundle_attach = esp_crt_bundle_attach
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_post_field(client, data, dlen);
    rest_setSecHdrs(client, data, dlen);
    esp_err_t err = esp_http_client_perform(client);

    int status = esp_http_client_get_status_code(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
            status, esp_http_client_get_content_length(client));
    }
    else
        ESP_LOGW(TAG,  "HTTP post failed. Status = %d", status);
        
    esp_http_client_cleanup(client);
    return status;
}

