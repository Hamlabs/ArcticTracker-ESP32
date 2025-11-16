 
/* Based on example from esp-idf */


#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "esp_err.h"
#include "esp_log.h"

#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (200*1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  8192



struct fileserv_context {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
}; 

typedef struct fileserv_context fileserv_context_t;

static const char *TAG = "rest";



#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)


    
    
/**********************************************************************************
 *  Set HTTP response content type according to file extension 
 **********************************************************************************/

static esp_err_t set_content_type(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".css")) {
        return httpd_resp_set_type(req, "text/css");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");   
    } else if (IS_FILE_EXT(filename, ".png")) {
        return httpd_resp_set_type(req, "image/png");    
    } else if (IS_FILE_EXT(filename, ".js")) {
        return httpd_resp_set_type(req, "text/javascript");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}




/***********************************************************************************
 * Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) 
 ***********************************************************************************/

static const char* get_path(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    if (base_pathlen + pathlen + 1 > destsize) 
        /* Full path string won't fit into destination buffer */
        return NULL;

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}



/*********************************************************************************
 *  Handler to download a file kept on the server 
 *********************************************************************************/

static esp_err_t file_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    const char *filename = get_path(filepath, ((fileserv_context_t*) req->user_ctx)->base_path,
                req->uri, sizeof(filepath));
    
    if (!filename) {
        ESP_LOGE(TAG, "Filename is too long");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }
    
    if (strlen(filename) == 0) {
        if (strlen(filepath) + strlen("/index.html") < FILE_PATH_MAX) {
            strcpy((char*)filename, "/index.html");
        } else {
            ESP_LOGE(TAG, "Path too long for index.html");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Path too long");
            return ESP_FAIL;
        }
    }
    
    /* If name has trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        if (strlen(filepath) + strlen("index.html") < FILE_PATH_MAX) {
            strcpy((char*)(filename + strlen(filename)), "index.html");
        } else {
            ESP_LOGE(TAG, "Path too long for index.html");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Path too long");
            return ESP_FAIL;
        }
    }
    
    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGW(TAG, "Failed to stat file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type(req, filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct fileserv_context *)req->user_ctx)->scratch;
    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
               return ESP_FAIL;
           }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);
    ESP_LOGI(TAG, "File sending complete");

    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}





/************************************************************************ 
 * Function to register file-server on an extisting HTTP server 
 ************************************************************************/

esp_err_t register_file_server(httpd_handle_t *server, const char *path)
{
    static fileserv_context_t *context = NULL;

    /* Allocate memory for server data only if not already allocated */
    if (context == NULL) {
        context = calloc(1, sizeof(fileserv_context_t));
        if (context == NULL) {
            ESP_LOGE(TAG, "Failed to allocate file server context");
            return ESP_ERR_NO_MEM;
        }
    }
    strcpy(context->base_path, path);

    /* URI handler for getting uploaded files */
    httpd_uri_t file_download = {
        .uri       = "/*",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = file_get_handler,
        .user_ctx  = context    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_download);

    return ESP_OK;
}
