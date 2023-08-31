//
//  layer3.h
//  tcp_ip_stack_c
//
//  Created by Yash soni on 19/08/23.
//

#ifndef layer3_h
#define layer3_h

#include <stdio.h>
#include "../gl_thread/gl_thread.h"
#include "graph.h"
#include <stdint.h>


#pragma pack(push, 1)

typedef struct ip_hdr_
{
    unsigned int version : 4;
    unsigned int ihl : 4;
    char tos;
    unsigned short total_length;
    
    unsigned short identification;
    unsigned int unused_flag : 1;
    unsigned int DF_flag : 1;
    unsigned int MORE_flag : 1;
    unsigned int frag_offset : 13;
    
    char ttl;
    char protocol;
    short checksum;
    unsigned int src_ip;
    unsigned int dst_ip;
    
} ip_hdr_t;

#pragma pack(pop)


static inline void
initialize_ip_hdr(ip_hdr_t *ip_hdr)
{
    printf("\nfn : %s\n", __FUNCTION__);

    ip_hdr->version = 4;
    ip_hdr->ihl = 5;
    ip_hdr->tos = 0;
    ip_hdr->total_length = 0;
    
    ip_hdr->identification = 0;
    ip_hdr->unused_flag = 0;
    ip_hdr->DF_flag = 1;
    ip_hdr->MORE_flag = 0;
    ip_hdr->frag_offset = 0;
    
    ip_hdr->ttl = 64;
    ip_hdr->protocol = 0;
    ip_hdr->checksum = 0;
    ip_hdr->src_ip = 0;
    ip_hdr->dst_ip = 0;
}

typedef struct l3_route_
{
    unsigned char dest[16];
    unsigned char mask;
    bool_t is_direct;
    unsigned char gw_ip[16];
    char oif[IF_NAME_SIZE];
    glthread_t rt_glue;
} l3_route_t;

typedef struct rt_table_
{
    glthread_t route_list;
} rt_table_t;
GLTHREAD_TO_STRUCT(rt_glue_to_l3_route, l3_route_t, rt_glue);


#define IP_HDR_LEN_IN_BYTES(ip_hdr_ptr) \
    (ip_hdr_ptr->ihl * 4)
#define IP_HDR_TOTAL_LEN_IN_BYTES(ip_hdr_ptr) \
    (ip_hdr_ptr->total_length * 4)

#define INCREMENT_IPHDR(ip_hdr_ptr) \
    ((char *)ip_hdr_ptr + (ip_hdr_ptr->ihl * 4))


#define IP_HDR_PAYLOAD_SIZE(ip_hdr_ptr) \
    (IP_HDR_TOTAL_LEN_IN_BYTES(ip_hdr_ptr) - \
    IP_HDR_LEN_IN_BYTES(ip_hdr_ptr))


void
init_rt_table(rt_table_t **rt_table);

void
rt_table_add_direct_route(rt_table_t *rt_table,
                          char *dst, char mask);

void
rt_table_add_route(rt_table_t *rt_table,
                   char *dst, char mask,
                   char *gw, char *oif);

l3_route_t *
l3rib_lookup_lpm(rt_table_t *rt_table,
                 uint32_t dest_ip);

#define IS_L3_ROUTES_EQUAL(rt1, rt2) \
 ((strncmp(rt1->dest, rt2->dest, 16) == 0) && \
    (rt1->mask == rt2->mask) && \
    (rt1->is_direct == rt2->is_direct) && \
    (strncmp(rt1->gw_ip, rt2->gw_ip, 16) == 0) && \
    (strncmp(rt1->oif, rt2->oif, IF_NAME_SIZE) == 0))

#endif /* layer3_h */
