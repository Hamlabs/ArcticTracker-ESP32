

#include <string.h>
#include "defines.h" 
#include "system.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "networking.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"


/* Return name/ip of connected host */
char* inet_chost(void);

#define TAG "tcp-cli"

static int sock = -1;
static struct sockaddr_in dest_addr;


/**************************************************************************
 *  Open internet connection 
 **************************************************************************/

int  inet_open(char* host, int port) 
{   
    struct hostent* addr = gethostbyname(host);
    if (addr == 0) {
        ESP_LOGW(TAG, "Failed DNS lookup for: %s", host); 
        return ERR_VAL;
    }
    dest_addr.sin_family = AF_INET;
    memcpy(&dest_addr.sin_addr.s_addr, addr->h_addr, sizeof(struct in_addr));
    dest_addr.sin_port = htons(port);
    
    sock =  socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) { 
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return errno;
    }    
    ESP_LOGI(TAG, "Inet socket connecting to %s:%d", inet_ntoa(dest_addr.sin_addr), port);

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) { 
        ESP_LOGW(TAG, "Socket unable to connect: errno=%d", errno);
        return errno;
    }
    
    ESP_LOGI(TAG, "Successfully connected to %s:%d", addr->h_name, port);
    return 0;
}        
        
        
/***************************************************************************
 *  Close internet connection 
 ***************************************************************************/

void inet_close(void) 
{
    if (sock != -1) {
        ESP_LOGI(TAG, "Closing connection...");
        shutdown(sock, 0);
        close(sock);
    }
}



/* FIXME FIXME Return true if we are connected */
bool inet_isConnected(void) {return false;}





/***************************************************************************
 *  Read data from connection 
 ***************************************************************************/

int inet_read(char* buf, int size) 
{
    int len = recv(sock, buf, size, 0);
    // Error occurred during receiving
    if (len < 0) 
        ESP_LOGE(TAG, "recv failed: errno %d", errno);
    else {
        // Data received
        buf[len] = 0; // Null-terminate whatever we received and treat like a string
        ESP_LOGI(TAG, "Received %d bytes", len);
    }
    return len;
}




/***************************************************************************
 * Write data to connection 
 ***************************************************************************/

void inet_write(char* data, int len)
{
    int err = send(sock, data, len, 0);
    if (err < 0) 
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
}




/***************************************************************************
 * HTTP post 
 *  - URL, content-type, data, length-of-data
 ***************************************************************************/

int http_post(char* uri, char* ctype, char* data, int dlen) 
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
    esp_err_t err = esp_http_client_perform(client);

    int status = esp_http_client_get_status_code(client);
    if (err != ESP_OK) 
        ESP_LOGW(TAG,  "HTTP post failed. Status = %d", status);
        
    esp_http_client_cleanup(client);
    return status;
}

