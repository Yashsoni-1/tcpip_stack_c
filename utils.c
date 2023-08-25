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


