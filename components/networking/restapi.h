#include "esp_http_server.h"
#include "cJSON.h"

#define JSON_GETITEM(root, id, type, dfl) \
   (cJSON_GetObjectItem((root), (id)) == NULL ? (dfl) : cJSON_GetObjectItem((root), (id))->value##type)

#define REGISTER_GET(uri, handler) rest_register((uri), HTTP_GET, (handler))
#define REGISTER_PUT(uri, handler) rest_register((uri), HTTP_PUT, (handler))
#define REGISTER_POST(uri, handler) rest_register((uri), HTTP_POST, (handler))
#define REGISTER_DELETE(uri, handler) rest_register((uri), HTTP_DELETE, (handler))
#define REGISTER_OPTIONS(uri, handler) rest_register((uri), HTTP_OPTIONS, (handler))

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
    

void      rest_register(char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r) );
esp_err_t rest_get_input(httpd_req_t *req,  char **buf, int* size);
esp_err_t rest_AUTH(httpd_req_t *req);
esp_err_t rest_JSON_input(httpd_req_t *req,  cJSON **json);
esp_err_t rest_JSON_send(httpd_req_t *req, cJSON *root);
void      rest_start(int port, const char *path);
void      rest_stop(void);
void      rest_cors_enable(httpd_req_t *req);
esp_err_t rest_options_handler(httpd_req_t *req);
char*     compute_hmac(const char* keyid, char* res, int hlen, uint8_t* data1, int len1, uint8_t* data2, int len2);
