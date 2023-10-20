//
//  ping.c
//  tcp_ip_stack_c
//
//  Created by Yash soni on 20/08/23.
//

#include <stdio.h>
#include "../graph.h"
#include <arpa/inet.h>
#include "../Layer3/layer3.h"

extern void
demote_packet_to_layer3(node_t *node,
                         char *pkt,
                         unsigned int size,
                         int protocol_number,
                         unsigned int dest_ip_address);

void
layer5_ping_fn(node_t *node, char *dst_ip_addr)
{
    unsigned int addr_int;
    
    printf("Src node : %s, Ping ip : %s\n", node->name, dst_ip_addr);
    
    inet_pton(AF_INET, dst_ip_addr, &addr_int);
    
    addr_int = htonl(addr_int);
    
    demote_packet_to_layer3(node, NULL, 0, ICMP_PRO, addr_int);
}


void
layer3_ero_ping_fn(node_t *node, char *dst_ip_addr, char *ero_ip_addr)
{
    ip_hdr_t *ip_hdr_in = calloc(1, sizeof(ip_hdr_t));
    initialize_ip_hdr(ip_hdr_in);
    ip_hdr_in->protocol = ICMP_PRO;
    ip_hdr_in->total_length = (unsigned short)(sizeof(ip_hdr_t)/4);
    
    printf("Src node : %s, Ping ip : %s\n", node->name, dst_ip_addr);
    
    unsigned int addr_int = 0,
    dst_addr_int = 0,
    ero_addr_int = 0;
    
    addr_int = ip_p_to_n(NODE_LO_ADD(node));
    dst_addr_int = ip_p_to_n(dst_ip_addr);
    ero_addr_int = ip_p_to_n(ero_ip_addr);
    
    ip_hdr_in->src_ip = addr_int;
    ip_hdr_in->dst_ip = dst_addr_int;
    
    l3_route_t *l3_route = l3rib_lookup_lpm(NODE_RT_TABLE(node), ip_hdr_in->dst_ip);
    char ip_address[16];
    ip_n_to_p(ip_hdr_in->dst_ip, ip_address);
    if(!l3_route)
    {
        printf("Router %s : Cannot Route IP : %s\n",
               node->name, ip_address);
        return;
    }
    
    demote_packet_to_layer3(node, (char *)ip_hdr_in, ip_hdr_in->total_length * 4,
                            IP_IN_IP, ero_addr_int);
    
    free(ip_hdr_in);
    
}
