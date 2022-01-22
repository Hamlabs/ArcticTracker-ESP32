

#include <string.h>
#include "defines.h" 
#include "system.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "networking.h"


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
 *  - host/ip, URI, content-type, data, length-of-data
 ***************************************************************************/

int http_post(char* host, uint16_t port, char* uri, char* ctype, char* data, int dlen) {
    
    if (inet_open(host, port) != 0) {
        // COULD-NOT-OPEN-CONNECTION
        ESP_LOGW(TAG, "Could not open connection to: %s:%d", host, port);
        return -1;
    }
    char buf[256];
    char msg[40];
    int len = 0;
    int status; 
    len = sprintf(buf,      "POST %s HTTP/1.0\r\n", uri);
    len += sprintf(buf+len, "Host: %s\r\n", host);
    len += sprintf(buf+len, "User-Agent: Arctic Tracker\r\n");
    len += sprintf(buf+len, "Content-Type: %s\r\n", ctype);
    len += sprintf(buf+len, "Content-Length: %d\r\n", dlen);
    inet_write(buf, len);
    inet_write("\r\n", 2);
    
    /* Send the content */
    inet_write(data, dlen);
    inet_write("\r\n", 2);
    
    /* Get the response */
    inet_read(buf, 255);
    sscanf(buf, "HTTP/1.1 %d %[^\n]\r\n", &status, msg);
    if (status != 200)
        ESP_LOGW(TAG, "http POST response=%d %s", status, msg);
    else
        ESP_LOGI(TAG, "http POST ok");
    return status;
}





/* Write data from fbuf */
//void inet_writeFB(FBUF *fb);

//void inet_mon_on(bool on);

//void inet_disable_read(bool on);

//void inet_signalReader(void);

