#ifndef net_h
#define net_h

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "utils.h"

typedef struct graph_ graph_t;
typedef struct interface_ interface_t;
typedef struct node_ node_t;

#define LO_MASK 32

typedef struct ip_add_ {
    char ip_addr[16];
} ip_add_t;

typedef struct mac_add_ {
    char mac[8];
} mac_add_t;

typedef struct node_nw_prop_
{
    bool_t is_lp_addr_config;
    ip_add_t lp_addr;
} node_nw_prop_t;

static inline void
init_node_nw_prop(node_nw_prop_t *node_nw_prop)
{
    node_nw_prop->is_lp_addr_config = FALSE;
    memset(node_nw_prop->lp_addr.ip_addr, 0, 16);
}

typedef struct intf_nw_props_
{
    mac_add_t mac_add;
    bool_t is_ipaddr_config;
    ip_add_t ip_add;
    char mask;
} intf_nw_props_t;


static inline void
init_intf_nw_prop(intf_nw_props_t *intf_nw_prop)
{
    memset(intf_nw_prop->mac_add.mac, 0,
           sizeof(intf_nw_prop->mac_add.mac));
    
    intf_nw_prop->is_ipaddr_config = FALSE;
    memset(intf_nw_prop->ip_add.ip_addr, 0, 16);
    intf_nw_prop->mask = 0;
}


#define IF_MAC(intf_ptr) (intf_ptr->intf_nw_props.mac_add.mac)
#define IF_IP(intf_ptr) (intf_ptr->intf_nw_props.ip_add.ip_addr)
#define NODE_LO_ADD(node_ptr) (node_ptr->node_nw_prop.lp_addr.ip_addr)
#define IS_INTF_L3_MODE(intf_ptr) (intf_ptr->intf_nw_props.is_ipaddr_config);


bool_t node_set_loopback_address(node_t *node, char *ip_addr);
bool_t node_set_intf_ip_address(node_t *node, char *local_if,
                           char *ip_addr, char mask);
bool_t node_unset_intf_ip_address(node_t *node, char *local_if);


void interface_assign_mac_address(interface_t *interface);

void dump_nw_graph(graph_t *graph);



#endif /* net_h */
