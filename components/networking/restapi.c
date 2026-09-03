/* 
 * Copyright (C) 2026 Øyvind Hanssen, LA7ECA
 * 
 * Arctic Tracker - REST API support functions
 *
 * Arctic Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details: 
 * <https://www.gnu.org/licenses/>.
 */


#include "defines.h"
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include <esp_https_server.h>
#include "esp_tls.h"
#include "lwip/sockets.h"
#include <arpa/inet.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_wifi.h"
#include "cJSON.h"
#include "system.h"
#include "networking.h"
#include "config.h"
#include "restapi.h"
#include "trex.h"
#include "esp_crt_bundle.h"
#include "cert.h"


#define SCRATCH_BUFSIZE (10240)
#define TAG "rest"

/* HTTP server configuration parameters */
#define HTTP_MAX_OPEN_SOCKETS 8
#define HTTP_RECV_TIMEOUT_SEC 5
#define HTTP_SEND_TIMEOUT_SEC 5


typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;


typedef struct rest_sess_context {
    char orig[64];
} rest_sess_context_t;
 
 
static rest_server_context_t *context; 
httpd_handle_t http_server = NULL;

static char* get_origin(httpd_req_t *req);



void rest_cors_enable(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", get_origin(req));   
    httpd_resp_set_hdr(req, "Vary", "Origin");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, PUT, POST, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Credentials", "true");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Arctic-Nonce, Arctic-Hmac, Authorization, X-Requested-With");
}


esp_err_t rest_options_handler(httpd_req_t *req) {
    rest_cors_enable(req);
    httpd_resp_set_hdr(req, "Allow", "GET, PUT, POST, DELETE");
    httpd_resp_sendstr(req, "");
    return ESP_OK;
}



/*******************************************************************************************
 * Free session context callback
 *******************************************************************************************/

static void free_sess_ctx(void *ctx) {
    if (ctx != NULL) {
        free(ctx);
    }
}


/*******************************************************************************************
 * Return origin in request, IF it matches API.ORIGINS regular expression
 *******************************************************************************************/

static char* get_origin(httpd_req_t *req) {
    char filter[64];
    char origin[64];

    if (req->sess_ctx == NULL) {
        req->sess_ctx = malloc(sizeof(rest_sess_context_t));
        if (req->sess_ctx == NULL) {
            ESP_LOGE(TAG, "Failed to allocate session context");
            return "";
        }
        req->free_ctx = free_sess_ctx;
    }
    char* buf =  ((rest_sess_context_t*) req->sess_ctx)->orig; 
    buf[0]=0;
    
    /* Get regular expression */
    get_str_param("API.ORIGINS", filter, 64, ".*");
    TRex *rex = trex_compile(filter);
    if (rex==NULL)
        return buf;
    
    /* Get origin header */
    if (httpd_req_get_hdr_value_str(req, "Origin", origin, 64) != ESP_OK) {
        trex_free(rex);
        return buf;
    }
    /* Check it and return it if it matches */
    if (trex_match(rex, origin)) {
        strncpy(buf, origin, 63);
        buf[63] = '\0';
    }
    
    trex_free(rex);
    return buf;
}


/*******************************************************************************************
 * Get the IP address (IPv4 or IPv6) of the client from the HTTP request
 * Returns a pointer to a static buffer containing the IP address string
 *******************************************************************************************/

char* get_client_ip(httpd_req_t *req) {
    static char ip_str[INET6_ADDRSTRLEN];
    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // Initialize with empty string in case of error
    ip_str[0] = '\0';
    
    // Get the socket file descriptor from the request
    int sockfd = httpd_req_to_sockfd(req);
    if (sockfd < 0) {
        ESP_LOGE(TAG, "Failed to get socket descriptor from request");
        return ip_str;
    }
    
    // Get the peer (client) address
    if (getpeername(sockfd, (struct sockaddr *)&client_addr, &addr_len) != 0) {
        ESP_LOGE(TAG, "Failed to get client address: errno %d", errno);
        return ip_str;
    }
    
    // Convert the IP address to string format based on address family
    if (client_addr.ss_family == AF_INET) {
        // IPv4 address
        struct sockaddr_in *addr_in = (struct sockaddr_in *)&client_addr;
        if (inet_ntop(AF_INET, &addr_in->sin_addr, ip_str, INET_ADDRSTRLEN) == NULL) {
            ESP_LOGE(TAG, "Failed to convert IPv4 address to string");
            ip_str[0] = '\0';
        }
    } else if (client_addr.ss_family == AF_INET6) {
        // IPv6 address
        struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)&client_addr;
        if (inet_ntop(AF_INET6, &addr_in6->sin6_addr, ip_str, INET6_ADDRSTRLEN) == NULL) {
            ESP_LOGE(TAG, "Failed to convert IPv6 address to string");
            ip_str[0] = '\0';
        }
    } else {
        ESP_LOGE(TAG, "Unknown address family: %d", client_addr.ss_family);
    }
    
    return ip_str;
}


/*******************************************************************************************
 * Serialize and send JSON with the response. 
 *******************************************************************************************/

esp_err_t rest_JSON_send(httpd_req_t *req, cJSON *root) {
    const char *sys_info = cJSON_Print(root);

    ESP_LOGD(TAG, "Response send: %s", get_client_ip(req));
    
    /* Set Connection: close when running in softAP mode to avoid keep-alive socket accumulation */
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_OK) {
        if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
            httpd_resp_set_hdr(req, "Connection", "close");
        }
    }
    
    esp_err_t err = httpd_resp_sendstr(req, sys_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send response: %d", err);
    }
    free((void *)sys_info);
    cJSON_Delete(root);
    return err;
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
    esp_err_t err = httpd_register_uri_handler(http_server, &system_info_get_uri);
    if (err == ESP_ERR_INVALID_ARG)
        ESP_LOGE(TAG, "Cannot register method for %s. Null arg.", uri);
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
        ESP_LOGW(TAG, "rest_get_input: content too long");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, *buf + cur_len, total_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive input");
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
    ESP_LOGD(TAG, "Client auth: %s", get_client_ip(req));
    if (rest_isAuth(req, "", 0) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Authenticaion failed");
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
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Authentication failed");
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

void rest_start(uint16_t port, uint16_t sport, const char *path) 
{
    nonce_init();
    
    /* Stop any running server first */
    if (http_server != NULL) {
        ESP_LOGI(TAG, "Stopping existing HTTP server before restart");
        rest_stop();
    }
    
    /* Allocate context struct */
    context = calloc(1, sizeof(rest_server_context_t));
    if (context == NULL) {
        ESP_LOGE(TAG, "Failed to allocate REST server context");
        return;
    }
    strcpy(context->base_path, path);
    
    /* Set up and start HTTP server */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = STACK_HTTPD;
    config.server_port = port;
    config.max_uri_handlers = 32;
    config.uri_match_fn = httpd_uri_match_wildcard;
     
    /* Defensive parameters to prevent socket exhaustion and improve reliability */
    config.lru_purge_enable = true;
    config.max_open_sockets = HTTP_MAX_OPEN_SOCKETS;
    config.recv_wait_timeout = HTTP_RECV_TIMEOUT_SEC;
    config.send_wait_timeout = HTTP_SEND_TIMEOUT_SEC;
    
    /* Set up HTTPS server */
#if defined WEBSERVER_HTTPS
    httpd_ssl_config_t sconfig = HTTPD_SSL_CONFIG_DEFAULT();
    sconfig.httpd = config;
    sconfig.port_secure = sport;
    sconfig.port_insecure = port;
    sconfig.session_tickets = false;  // Prevent memory exhaustion

    /* 
     * Use a runtime-generated self-signed certificate stored in NVS.
     */
    if (cert_init()) {
        size_t cert_len, key_len;
        sconfig.servercert     = cert_get_pem(&cert_len);
        sconfig.servercert_len = cert_len;
        sconfig.prvtkey_pem    = cert_get_key_pem(&key_len);
        sconfig.prvtkey_len    = key_len;
    }
#endif
    
#if defined WEBSERVER_HTTPS
    ESP_LOGI(TAG, "Starting REST HTTPS Server on port %u", sport);
    esp_err_t err = httpd_ssl_start(&http_server, &sconfig);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTPS server: %d", err);
        free(context);
        context = NULL;
        return;
    }
#else
    ESP_LOGI(TAG, "Starting REST HTTP Server on port %u", port);
    esp_err_t err = httpd_start(&http_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %d", err);
        free(context);
        context = NULL;
        return;
    }
#endif
    register_api_rest();
}



void rest_stop() {
    if (http_server != NULL) {
        ESP_LOGI(TAG, "Stopping REST HTTP server");
#if defined WEBSERVER_HTTPS
        esp_err_t err = httpd_ssl_stop(http_server);
#else
        esp_err_t err = httpd_stop(http_server);
#endif
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to stop HTTP server: %d", err);
        }
        http_server = NULL;
    }
    
    if (context != NULL) {
        ESP_LOGI(TAG, "Freeing REST server context");
        free(context);
        context = NULL;
    }
}



/***************************************************************************
 * HTTP post with HMAC authentication. 
 *  - URL, service, data, length-of-data, key
 ***************************************************************************/

int rest_post(char* uri, char* service, char* data, int dlen, char* key) 
{
    esp_http_client_config_t config = {
        .url = uri,
        .method = HTTP_METHOD_POST, 
        .user_agent = "ArcticTracker",
        .cert_pem = NULL,
        .crt_bundle_attach = esp_crt_bundle_attach
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    
    esp_http_client_set_post_field(client, data, dlen);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    rest_setSecHdrs(client, service, data, dlen, key);
    esp_err_t err = esp_http_client_perform(client);
    
    int status = esp_http_client_get_status_code(client);
    if (err == ESP_OK && status == 200) {
        long long len = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "Status = %d, content_length = %lld", status, len);
    }
    else
        ESP_LOGW(TAG, "HTTP post failed. Status = %d", status);
        
    esp_http_client_cleanup(client);
    return status;
}



/*
 * POST and get data from response. 
 * Return status code or -1 if other error. 
 * Errors are logged.
 */
int rest_post_r(const char *url, const char* service, const char *data, size_t dlen, char* key, char *rdata, size_t max_buf_len) {

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST, 
        .user_agent = "ArcticTracker",
        .cert_pem = NULL,
        .crt_bundle_attach = esp_crt_bundle_attach
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return -1;
    }
    
    /* Set the content type */
    esp_http_client_set_header(client, "Content-Type", "application/x-pem-file");
    rest_setSecHdrs(client, service, data, dlen, key);
    
    /* Open the connection and specify how much data we intendt to send (Content-Length) */
    esp_err_t err = esp_http_client_open(client, dlen);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Couldn't open connection to server: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return -1;
    }

    /* Send the POST payload to server */
    int bytes_written = esp_http_client_write(client, data, dlen);
    if (bytes_written < 0) {
        ESP_LOGE(TAG, "Error in writing POST-data");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return -1;
    }

    /* Get headers from server response */
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP Status: %d, Content-Length: %d", status_code, content_length);

    if (status_code != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return status_code;
    }

    int total_read = 0;
    
    /* Read as long as data is available and there is room in the buffer. */
    while (total_read < (max_buf_len - 1)) {
        int remaining_buf = (max_buf_len - 1) - total_read;
        
        int bytes_read = esp_http_client_read(client, rdata + total_read, remaining_buf);
        
        if (bytes_read < 0) {
            ESP_LOGE(TAG, "Error in reading of response data");
            total_read = -1; 
            break;
        } else if (bytes_read == 0) 
            break;
        
        total_read += bytes_read;
    }

    if (total_read >= 0) 
        rdata[total_read] = '\0';

    /* Cleanup */
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return status_code;
}


