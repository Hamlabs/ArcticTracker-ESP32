#include "esp_log.h"
#include "mdns.h"
 
static const char * if_str[] = {"STA", "AP", "ETH", "MAX"};
static const char * ip_protocol_str[] = {"V4", "V6", "MAX"};

#define TAG "mdns"

static char hostname[32]; 


/**********************************************************************************
 * start mdns service and announce ourselves 
 **********************************************************************************/

void mdns_start(char* ident) {
  
    //initialize mDNS service
    esp_err_t err = mdns_init();
    if (err) {
        printf("MDNS Init failed: %d\n", err);
        return;
    }
    
    char buffer[32]; 
    
    /* Set hostname */
    sprintf(hostname, "arctic-%s", ident);
    mdns_hostname_set(hostname);
    
    /* Set default instance */
    sprintf(buffer, "Arctic Tracker: %s", ident);
    mdns_instance_name_set(buffer);
    
    /* Announce services */
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    mdns_service_instance_name_set("_http", "_tcp", "Arctic Tracker HTTP Server");
    
    mdns_txt_item_t txtData[1] = {
        {"ident", ident}
    };
    /* Set txt data for service (will free and replace current data) */
    mdns_service_txt_set("_http", "_tcp", txtData, 1);
}



char* mdns_hostname(char* buf) {
    sprintf(buf, "%s.local", hostname);
    return buf;
}



/**********************************************************************************
 * start mdns service and announce ourselves 
 **********************************************************************************/

mdns_result_t* mdns_find_service(const char * service_name, const char * proto)
{
    mdns_result_t * results = NULL;
    esp_err_t err = mdns_query_ptr(service_name, proto, 3000, 20,  &results);
    if(err){
        ESP_LOGW(TAG, "Query Failed");
        return NULL;
    }
    if(!results){
        ESP_LOGI(TAG, "No results found!");
        return NULL;
    }
    return results;
}




/**********************************************************************************
 * print results from mdns search
 **********************************************************************************/

void mdns_print_results(mdns_result_t * results)
{
    mdns_result_t * r = results;
    mdns_ip_addr_t * a = NULL;
    int i = 1, t;
    while(r){
        
        printf("%d: %s\n", i++, r->instance_name); 
        a = r->addr;
        while(a){
            if(a->addr.type == ESP_IPADDR_TYPE_V6){
                printf("     IPv6: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
            } else {
                printf("     IPv4: " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
            }
            a = a->next;
        }
        printf("     Port: %u\n", r->port);
        printf("     Host: %s.local\n", r->hostname);
        
        if(r->txt_count) {
            printf("    Attrs: \n");
            for(t=0; t<r->txt_count; t++){
                printf("%s=%s; ", r->txt[t].key, r->txt[t].value);
            }
            printf("\n");
        }

        r = r->next;
    }

}




