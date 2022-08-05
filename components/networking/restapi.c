
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


#define SCRATCH_BUFSIZE (10240)
#define TAG "restapi"


typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;


static rest_server_context_t *context; 
static httpd_handle_t server = NULL;


void rest_cors_enable(httpd_req_t *req) {
    // FIXME
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "http://localhost");   
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, PUT, POST, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Credentials", "true");
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

esp_err_t rest_get_input(httpd_req_t *req,  char **buf)
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
    return ESP_OK;
}



/*******************************************************************************************
 * Get, check and parse JSON input from HTTP payload. 
 * Typically used through macro (see restapi.h)
 *******************************************************************************************/

esp_err_t rest_JSON_input(httpd_req_t *req,  cJSON **json)
{
    char *buf;
    if (rest_get_input(req, &buf) == ESP_FAIL)
        return ESP_FAIL;
    
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
    /* Allocate context struct */
    context = calloc(1, sizeof(rest_server_context_t));
    strcpy(context->base_path, path);
    
    /* Set up and start HTTP server */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.max_uri_handlers = 32;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting REST HTTP Server on port %d", port);
    httpd_start(&server, &config);
    
    register_api_test();
}



void rest_stop() {
}



