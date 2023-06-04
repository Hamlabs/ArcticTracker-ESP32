  
/* 
 * Borrowed/adapted from: 
 * https://github.com/sankarcheppali/esp_idf_esp32_posts/blob/master/tcp_server/ap_mode/main/esp_ap_tcp_server.c
 * Se also https://github.com/nkolban/esp32-snippets/blob/master/sockets/server/socket_server.c
 */


#include "esp_log.h"
#include "system.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "defines.h"


static const char *TAG="tcpserver";
void tcp_test_worker(void *wParam);


#define LISTENQ 4

typedef struct {
    int port; 
    int stack;
    TaskFunction_t worker;
} ServerInfo_t; 



static void tcp_server(void *pvParam)
{
    char tname[14]; 
    ServerInfo_t srv = *((ServerInfo_t*) pvParam);
    ESP_LOGI(TAG,"tcp_server task started on port %d", srv.port );
    struct sockaddr_in serverAddr;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons( srv.port );
    int ss;
    static struct sockaddr_in remote_addr;
    static socklen_t socklen;
    socklen = sizeof(remote_addr);
    sprintf(tname, "worker_%d", srv.port);

    while(1)
    {
        sleepMs(1000);
        ss = socket(AF_INET, SOCK_STREAM, 0);
        if(ss < 0) {
            ESP_LOGE(TAG, "Failed to allocate socket.");
            continue;
        }
        if(bind(ss, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) != 0) {
            ESP_LOGE(TAG, "Socket bind failed errno=%d", errno);
            close(ss);
            continue;
        }
        if(listen (ss, LISTENQ) != 0) {
            ESP_LOGE(TAG, "Socket listen failed errno=%d", errno);
            close(ss);
            continue;
        }
    
        while(1) {
            int cs = accept(ss, (struct sockaddr *)&remote_addr, &socklen);
            xTaskCreate(srv.worker, tname, srv.stack, (void*) &cs, 4, NULL); 
        }

        sleepMs(4000);
    }
    ESP_LOGI(TAG, "Server task ends...");
}



void tcp_test_worker(void *wParam)
{
    int sock = *((int*) wParam);
    FILE *f = fdopen(sock, "r+");
    ESP_LOGI(TAG, "Worker thread started ok");
    fprintf(f, "Welcome to test server\n");
    fprintf(f, "Enter your text: ");
    fflush(f);
    char buf[20];
    fgets(buf, 20, f);
    fprintf(f, "You said: %s\n", buf);
    fflush(f);
    sleepMs(4000);
    fprintf(f, "Closing ... \n");
    fflush(f);
    sleepMs(4000);     
    ESP_LOGI(TAG, "Closing Worker thread..");
    fclose(f); 
    close(sock);
    vTaskDelete(NULL);
}



void tcpserver_start(int port, TaskFunction_t worker, int stack)
{
    ServerInfo_t srv; 
    srv.port = port; 
    srv.worker = worker; 
    srv.stack = stack;
    char tname[18]; 
    sprintf(tname, "tcpsrv_%d", port);
    xTaskCreate(&tcp_server, tname, 3072, &srv, 5, NULL);
}



void tcpserver_init() {
    tcpserver_start(3000, &tcp_test_worker, 4096);
}



