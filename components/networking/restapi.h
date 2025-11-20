#include "esp_http_server.h"
#include "esp_http_client.h"
#include "cJSON.h"


#define JSON_GETITEM(root, id, type, dfl) \
   (cJSON_GetObjectItem((root), (id)) == NULL ? (dfl) : cJSON_GetObjectItem((root), (id))->value##type)

#define REGISTER_GET(uri,    handler) rest_register((uri), HTTP_GET, (handler))
#define REGISTER_PUT(uri,    handler) rest_register((uri), HTTP_PUT, (handler))
#define REGISTER_POST(uri,   handler) rest_register((uri), HTTP_POST, (handler))
#define REGISTER_DELETE(uri, handler) rest_register((uri), HTTP_DELETE, (handler))
#define REGISTER_OPTIONS(uri,handler) rest_register((uri), HTTP_OPTIONS, (handler))

#define JSON_STR(root, id)  JSON_GETITEM(root, id, string, "")
#define JSON_BYTE(root, id)  (uint8_t) JSON_GETITEM(root, id, int, 0)
#define JSON_BOOL(root, id)  (bool) JSON_GETITEM(root, id, int, 0)
#define JSON_U16(root, id)   (uint16_t) JSON_GETITEM(root, id, int, 0)
#define JSON_U32(root, id)   (uint32_t) JSON_GETITEM(root, id, int, 0)
#define JSON_INT(root, id)   (int) JSON_GETITEM(root, id, int, 0)

#define CHECK_JSON_INPUT(req, json)  \
    if (rest_JSON_input(req, &json) == ESP_FAIL) \
        return ESP_FAIL; 

#define CHECK_AUTH(req)  \
    if (rest_AUTH(req) == ESP_FAIL) \
        return ESP_FAIL; 
    
    
/* REST API server */
void      rest_register(char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r) );
esp_err_t rest_get_input(httpd_req_t *req,  char **buf, int* size);
esp_err_t rest_AUTH(httpd_req_t *req);
esp_err_t rest_JSON_input(httpd_req_t *req,  cJSON **json);
esp_err_t rest_JSON_send(httpd_req_t *req, cJSON *root);
void      rest_start(uint16_t port, uint16_t sport, const char *path);
void      rest_stop(void);
void      rest_cors_enable(httpd_req_t *req);
esp_err_t rest_options_handler(httpd_req_t *req);
char*     get_client_ip(httpd_req_t *req);

/* Security */
char*     compute_sha256_b64(char* hash, uint8_t *data, int len); 
char*     compute_hmac(const char* keyid, char* res, int hlen, uint8_t* data1, int len1, uint8_t* data2, int len2);
esp_err_t rest_isAuth(httpd_req_t *req, char* payload, int plsize);
void      rest_setSecHdrs(esp_http_client_handle_t client, char* service, char* data, int dlen, char* key);
void      nonce_init();

/* REST API client */
esp_err_t rest_post(char* uri, char* service, char* data, int dlen, char* key);
