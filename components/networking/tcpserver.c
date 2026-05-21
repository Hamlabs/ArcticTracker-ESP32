  
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
#include "networking.h"



static const char *TAG="tcpserver";

void tcp_test_worker(void *wParam);

#define LISTENQ 4

struct _serverinfo {
    bool running;
    int port; 
    int socket;
    int stack;
    char* name;
    TaskFunction_t worker;
}; 



static void tcp_server(void *pvParam)
{
    char tname[14]; 
    ServerInfo_t *srv = (ServerInfo_t*) pvParam; 
    ESP_LOGI(TAG,"tcp_server task started on port %d", srv->port );
    struct sockaddr_in serverAddr;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons( srv->port );
    int ss = 0;
    struct sockaddr_in remote_addr;
    socklen_t socklen;
    socklen = sizeof(remote_addr);
    sprintf(tname, "%s_worker", srv->name);
    int tries = 0;
    char ip[INET6_ADDRSTRLEN]; 
    
    while(srv->running && tries <= 3)
    {
        sleepMs(1000);
        srv->socket = ss = socket(AF_INET, SOCK_STREAM, 0);
        if(ss < 0) {
            ESP_LOGE(TAG, "Failed to allocate socket. errno=%d", errno);
            tries = 3;
            continue;
        }
        if(bind(ss, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) != 0) {
            ESP_LOGE(TAG, "Socket bind failed. errno=%d", errno);
            close(ss);
            sleepMs(10000);
            tries++;
            continue;
        }
        if(listen (ss, LISTENQ) != 0) {
            ESP_LOGE(TAG, "Socket listen failed. errno=%d", errno);
            close(ss);
            sleepMs(5000);
            tries++;
            continue;
        }
    
        /* Listen for incoming connections */
        while(srv->running) {
            int cs = accept(ss, (struct sockaddr *) &remote_addr, &socklen);
            if (cs < 0) {
                ESP_LOGE(TAG, "Failed to accept incoming connections. errno=%d", errno);
                close(ss);
                sleepMs(10000);
                tries++;
                continue;
            }
            sockaddr2ip( (struct sockaddr *) &remote_addr, ip);
            ESP_LOGI(TAG, "Connect from: %s", ip);
            xTaskCreate(srv->worker, tname, srv->stack, (void*)(intptr_t) cs, 4, NULL);
              /* Note that cs is passed "by value" to the thread */
        }

        sleepMs(2000);
    }
    ESP_LOGI(TAG, "Server task ends...");
    if (srv->running) 
        close(ss);
    vTaskDelete(NULL);
}




char* sockaddr2ip(struct sockaddr *sa, char *buf) 
{
    if (sa->sa_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)sa;
        inet_ntop(AF_INET, &(ipv4->sin_addr), buf, INET_ADDRSTRLEN);
    } else if (sa->sa_family == AF_INET6) {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)sa;
        inet_ntop(AF_INET6, &(ipv6->sin6_addr), buf, INET6_ADDRSTRLEN);
    } else {
        ESP_LOGW(TAG, "Unknown address family\n");
        buf[0] = '\0';
    }
    return buf;
}






ServerInfo_t *tcpserver_start(int port, TaskFunction_t worker, int stack, char* name)
{
    ServerInfo_t *srv = malloc(sizeof(ServerInfo_t)); 
    srv->running = true;
    srv->port = port; 
    srv->socket = 0;
    srv->worker = worker; 
    srv->stack = stack;
    srv->name = name;
    
    char tname[18]; 
    sprintf(tname, "%s_srv", name);
    xTaskCreate(&tcp_server, tname, 4096, srv, 5, NULL);
    return srv;
}




void tcpserver_stop(ServerInfo_t *srv) {
    srv->running = false;
    close(srv->socket);
    sleepMs(1000);
    free(srv);
}

