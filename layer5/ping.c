//
//  ping.c
//  tcp_ip_stack_c
//
//  Created by Yash soni on 20/08/23.
//

#include <stdio.h>
#include "../graph.h"
#include <arpa/inet.h>


extern void
demote_packet_to_layer3(node_t *node,
                     char *pkt,
                     unsigned int size,
                     int protocol_number,
                     unsigned int dest_ip_address);

void
layer5_ping_fn(node_t *node, char *dst_ip_addr)
{
    printf("\nfn : %s\n", __FUNCTION__);

    unsigned int addr_int;
    
    printf("Src node : %s, Ping ip : %s\n", node->name, dst_ip_addr);
    
    inet_pton(AF_INET, dst_ip_addr, &addr_int);
    
    addr_int = htonl(addr_int);
    
    demote_packet_to_layer3(node, NULL, 0, ICMP_PRO, addr_int);
}
