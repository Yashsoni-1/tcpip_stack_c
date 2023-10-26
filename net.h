#ifndef net_h
#define net_h


#include <memory.h>
#include "utils.h"
#include "tcpconst.h"
#include <stdint.h>


typedef struct graph_ graph_t;
typedef struct interface_ interface_t;
typedef struct node_ node_t;


#pragma pack(push, 1)
typedef struct ip_add_
{
    unsigned char ip_addr[16];
} ip_add_t;

typedef struct mac_add_
{
    unsigned char mac[6];
} mac_add_t;
#pragma pack(pop)

typedef struct arp_table_ arp_table_t;
typedef struct mac_table_ mac_table_t;
typedef struct rt_table_ rt_table_t;


typedef struct node_nw_prop_
{
    arp_table_t *arp_table;
    mac_table_t *mac_table;
    
    rt_table_t *rt_table;
    
    bool_t is_lp_addr_config;
    ip_add_t lp_addr;
    
} node_nw_prop_t;

extern void init_arp_table(arp_table_t **arp_table);
extern void init_mac_table(mac_table_t **mac_table);
extern void init_rt_table(rt_table_t **rt_table);


static inline void
init_node_nw_prop(node_nw_prop_t *node_nw_prop)
{
    node_nw_prop->is_lp_addr_config = FALSE;
    
    memset(node_nw_prop->lp_addr.ip_addr, 0, 16);
    
    init_arp_table(&(node_nw_prop->arp_table));
    
    init_mac_table(&(node_nw_prop->mac_table));
    
    init_rt_table(&(node_nw_prop->rt_table));
}


typedef enum {
    ACCESS,
    TRUNK,
    L2_MODE_UNKNOWN
} intf_l2_mode_t;

static inline char *
intf_l2_mode_str(intf_l2_mode_t intf_l2_mode)
{
    switch(intf_l2_mode) {
        case ACCESS:
            return "access";
        case TRUNK:
            return "trunk";
        default:
            return "L2_MODE_UNKNOWN";
    }
}

#define MAX_VLAN_MEMBERSHIP 10

typedef struct intf_nw_props_
{
    bool_t is_up;
    
    mac_add_t mac_add;
    
    intf_l2_mode_t intf_l2_mode;
    
    unsigned int vlans[MAX_VLAN_MEMBERSHIP];
    
    bool_t is_ipaddr_config;
    ip_add_t ip_add;
    
    uint32_t pkt_recv;
    uint32_t pkt_sent;
    
    char mask;

} intf_nw_props_t;

static inline void
init_intf_nw_prop(intf_nw_props_t *intf_nw_prop)
{
    intf_nw_prop->is_up = TRUE;
    
    memset(intf_nw_prop->mac_add.mac, 0,
           sizeof(intf_nw_prop->mac_add.mac));
    
    intf_nw_prop->intf_l2_mode = L2_MODE_UNKNOWN;
    
    memset(intf_nw_prop->vlans, 0, sizeof(intf_nw_prop->vlans));
    
    intf_nw_prop->is_ipaddr_config = FALSE;
    memset(intf_nw_prop->ip_add.ip_addr, 0, 16);
    intf_nw_prop->mask = 0;
    
    intf_nw_prop->pkt_sent = 0;
    intf_nw_prop->pkt_recv = 0;
}

#define IF_MAC(intf_ptr) (intf_ptr->intf_nw_props.mac_add.mac)
#define IF_IP(intf_ptr) (intf_ptr->intf_nw_props.ip_add.ip_addr)
#define IF_IS_UP(intf_ptr) ((intf_ptr)->intf_nw_props.is_up == TRUE)

#define NODE_LO_ADD(node_ptr) (node_ptr->node_nw_prop.lp_addr.ip_addr)
#define NODE_ARP_TABLE(node_ptr)  (node_ptr->node_nw_prop.arp_table)
#define NODE_MAC_TABLE(node_ptr) (node_ptr->node_nw_prop.mac_table)
#define NODE_RT_TABLE(node_ptr) (node_ptr->node_nw_prop.rt_table)
#define IS_INTF_L3_MODE(intf_ptr) (intf_ptr->intf_nw_props.is_ipaddr_config)
#define IF_L2_MODE(intf_ptr) (intf_ptr->intf_nw_props.intf_l2_mode)

bool_t node_set_loopback_address(node_t *node, char *ip_addr);
bool_t node_set_intf_ip_address(node_t *node, char *local_if,
                           char *ip_addr, char mask);
bool_t node_unset_intf_ip_address(node_t *node, char *local_if);


void interface_assign_mac_address(interface_t *interface);
interface_t *node_get_matching_subnet_interface(node_t *node,
                                                char *ip_addr);


char*
pkt_buffer_shift_right(char *pkt, unsigned int pkt_size,
                      unsigned int total_buffer_size);

unsigned int
get_access_intf_operating_vlan_id(interface_t *interface);


bool_t
is_trunk_interface_vlan_enabled(interface_t *interface,
                                unsigned int vlan_id);



void dump_nw_graph(graph_t *graph, node_t *node1);
void dump_intf_props(interface_t *intf);
void dump_interface(interface_t *interface);
void dump_node_nw_props(node_t *node);
void dump_node(node_t *node);
void dump_node_interface_stats(node_t *node);
void dump_interface_stats(interface_t *interface);
void print_mac(char *heading, unsigned char *mac_address);

bool_t is_interface_l3_bidirectional(interface_t *interface);

#define ITERATE_NODE_NBRS_BEGIN(node_ptr, nbr_ptr, oif_ptr, ip_addr_ptr) \
    do{ 																\
			int i=0;													\
			interface_t *other_intf;									\						
			for(i=0; i < MAX_INTF_PER_NODE; ++i) {						\
				oif_ptr = node_ptr[i];									\
				if(!oif_ptr) continue;									\
				other_intf = &oif_ptr->link->intf1 == oif_ptr ? 		\
					&oif_ptr->link->intf2 : &oif_ptr->link->intf1;		\
				if(!other_intf) continue;								\
				nbr_ptr = get_nbr_node(oif_ptr);						\
				ip_addr_ptr = IF_IP(other_intf);						\

#define ITERATE_NODE_NBRS_END(node_ptr, nbr_ptr, oif_ptr, ip_addr_ptr) }}while(0);

#endif /* net_h */
