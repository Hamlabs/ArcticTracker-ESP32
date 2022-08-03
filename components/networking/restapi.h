
#define REGISTER_GET(uri, handler) rest_register((uri), HTTP_GET, (handler))
#define REGISTER_PUT(uri, handler) rest_register((uri), HTTP_PUT, (handler))
#define REGISTER_POST(uri, handler) rest_register((uri), HTTP_POST, (handler))
#define REGISTER_DELETE(uri, handler) rest_register((uri), HTTP_DELETE, (handler))
#define REGISTER_OPTIONS(uri, handler) rest_register((uri), HTTP_OPTIONS, (handler))


#define CHECK_JSON_INPUT(req, json)  \
    if (rest_JSON_input(req, &json) == ESP_FAIL) \
        return ESP_FAIL; 


void      rest_register(char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r) );
esp_err_t rest_get_input(httpd_req_t *req,  char **buf);
esp_err_t rest_JSON_input(httpd_req_t *req,  cJSON **json);
void      rest_start(int port, const char *path);
void      rest_stop(void);
void      rest_cors_enable(httpd_req_t *req);
esp_err_t rest_options_handler(httpd_req_t *req);
