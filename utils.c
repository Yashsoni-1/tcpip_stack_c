#include "utils.h"
#include <arpa/inet.h>
#include <stdint.h>

void apply_mask(char *prefix, char mask, char *str_prefix)
{
    uint32_t binary_prefix = 0;
    uint32_t subnet_mask = 0xffffffff;
    
    if(mask == 32){
        strcpy(str_prefix, prefix);
        str_prefix[15] = '\0';
        return;
    }
    
    inet_pton(AF_INET, prefix, &binary_prefix);
    binary_prefix = htonl(binary_prefix);
    
    subnet_mask = subnet_mask << (32 - mask);
    binary_prefix = binary_prefix & subnet_mask;
    
    binary_prefix = htonl(binary_prefix);
    inet_ntop(AF_INET, &binary_prefix, str_prefix, 16);
    str_prefix[15] = '\0';
}

void layer2_fill_with_broadcast_mac(unsigned char *mac_array)
{
    mac_array[0] = 0xff;
    mac_array[1] = 0xff;
    mac_array[2] = 0xff;
    mac_array[3] = 0xff;
    mac_array[4] = 0xff;
    mac_array[5] = 0xff;
}

unsigned int
ip_p_to_n(char *ip_addr)
{
    uint32_t ip = 0;
    inet_pton(AF_INET, ip_addr, &ip);
    ip = htonl(ip);
    return ip;
}

char *
ip_n_to_p(unsigned int ip_addr, char *output_buffer)
{   
    char *out = NULL;
    
    static char str_ip[16];
    
    out = !output_buffer ? str_ip : output_buffer;
    
    memset(out, 0, 16);

    ip_addr = htonl(ip_addr);
    inet_ntop(AF_INET, &ip_addr, out, 16);
    out[15] = '\0';
    
    return out;
}

