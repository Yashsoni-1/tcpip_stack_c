#ifndef layer2_h
#define layer2_h

#include <stdio.h>
#include "../net.h"
#include "../graph.h"
#include "../tcpconst.h"
#include <stdint.h>

#define PAYLOAD_SIZE 248

#pragma pack(push, 1)
typedef struct arp_hdr_
{
    short hw_type;
    short proto_type;
    char hw_addr_len;
    char proto_addr_len;
    short op_code;
    mac_add_t src_mac;
    unsigned int src_ip;
    mac_add_t dest_mac;
    unsigned int dest_ip;
} arp_hdr_t;

typedef struct ethernet_hdr_
{
    mac_add_t dst_mac;
    mac_add_t src_mac;
    unsigned short type;
    char payload[PAYLOAD_SIZE];
    unsigned int FCS;
} ethernet_hdr_t;
#pragma pack(pop)

#define ETH_HDR_SIZE_EXCL_PAYLOAD \
    (sizeof(ethernet_hdr_t) - sizeof(((ethernet_hdr_t *)0)->payload))

#define ETH_FCS(eth_hdr_ptr, payload_size) \
    (*(unsigned int *)(((char *)(((ethernet_hdr_t *)eth_hdr_ptr)->payload) + payload_size)))

static inline ethernet_hdr_t*
ALLOC_ETH_HDR_WITH_PAYLOAD(char *pkt, unsigned int pkt_size)
{
    printf("\nfn : %s\n", __FUNCTION__);

    char *temp = calloc(1, pkt_size);
    memcpy(temp, pkt, pkt_size);

    ethernet_hdr_t *eth_hdr = (ethernet_hdr_t *)(pkt - ETH_HDR_SIZE_EXCL_PAYLOAD);
    memset((char *) eth_hdr, 0, ETH_HDR_SIZE_EXCL_PAYLOAD);
    memcpy(eth_hdr->payload, temp, pkt_size);
    ETH_FCS(eth_hdr, pkt_size) = 0;
    free(temp);
    return eth_hdr;
}



typedef struct arp_table_ {
    glthread_t arp_entries;
} arp_table_t;

typedef struct arp_entry_ arp_entry_t;
typedef struct arp_pending_entry_ arp_pending_entry_t;

typedef void (*arp_processing_fn) (node_t *node,
                                   interface_t *oif,
                                   arp_entry_t *arp_entry,
                                   arp_pending_entry_t *arp_pending_entry) ;

typedef struct arp_pending_entry_
{
    glthread_t arp_pending_entry_glue;
    arp_processing_fn cb;
    uint32_t pkt_size;
    char pkt[0];
}arp_pending_entry_t;
GLTHREAD_TO_STRUCT(arp_pending_entry_glue_to_arp_pending_entry, \
                   arp_pending_entry_t, arp_pending_entry_glue);

typedef struct arp_entry_ {
    ip_add_t ip_addr;
    mac_add_t mac_addr;
    char oif_name[IF_NAME_SIZE];
    glthread_t arp_glue;
    bool_t is_sane;
    glthread_t arp_pending_list;
} arp_entry_t;
GLTHREAD_TO_STRUCT(arp_glue_arp_entry, arp_entry_t, arp_glue);
GLTHREAD_TO_STRUCT(arp_pending_list_to_arp_entry, arp_entry_t, arp_pending_list);

#define IS_ARP_ENTRIES_EQUAL(arp_entry_1, arp_entry_2) \
    (strncmp(arp_entry_1->ip_addr.ip_addr, arp_entry_2->ip_addr.ip_addr, 16) == 0 && \
    (strncmp(arp_entry_1->mac_addr.mac, arp_entry_2->mac_addr.mac, sizeof(mac_add_t)) == 0) && \
    (strncmp(arp_entry_1->oif_name, arp_entry_2->oif_name, IF_NAME_SIZE) == 0) && \
    arp_entry_1->is_sane == arp_entry_2->is_sane && \
    arp_entry_1->is_sane == FALSE \
)

void init_arp_table(arp_table_t **arp_table);

void send_arp_broadcast_request(node_t *node, interface_t *oif,
                                char *ip_addr);

bool_t
arp_table_entry_add(arp_table_t *arp_table,
                    arp_entry_t *arp_entry,
                    glthread_t **arp_pending_list);




arp_entry_t *arp_table_lookup(arp_table_t *arp_table, char *ip_addr);

void arp_table_update_from_arp_reply(arp_table_t *arp_table,
                                     arp_hdr_t *arp_hdr,
                                     interface_t *iif);

void delete_arp_table_entry(arp_table_t *arp_table, char *ip_addr);

void dump_arp_table(arp_table_t *arp_table);

void node_set_intf_l2_mode(node_t *node, char *intf_name,
                           intf_l2_mode_t intf_l2_mode);

void
node_set_intf_vlan_membership(node_t *node,
                              char *intf_name,
                              unsigned int vlan_id);

void
add_arp_pending_entry(arp_entry_t *arp_entry,
                      arp_processing_fn,
                      char *pkt,
                      unsigned int pkt_size);

arp_entry_t *
create_arp_sane_entry(arp_table_t *arp_table,
                      char *ip_addr);

static bool_t
arp_entry_sane(arp_entry_t *arp_entry)
{
    return arp_entry->is_sane;
}

#pragma pack(push, 1)
typedef struct vlan_8021q_hdr_
{
    unsigned short tpid;
    short tci_pcp : 3;
    short tci_dei : 1;
    short tci_vid : 12;
} vlan_8021q_hdr_t;

typedef struct vlan_ethernet_hdr_
{
    mac_add_t dst_mac;
    mac_add_t src_mac;
    vlan_8021q_hdr_t vlan_8021q_hdr;
    unsigned short type;
    char payload[248];
    unsigned int FCS;
} vlan_ethernet_hdr_t;
#pragma pack(pop)

static inline vlan_8021q_hdr_t*
is_pkt_vlan_tagged(ethernet_hdr_t *ethernet_hdr)
{
    printf("\nfn : %s\n", __FUNCTION__);

    vlan_8021q_hdr_t *vlan_8021q_hdr =
        (vlan_8021q_hdr_t *)((char *)ethernet_hdr + (2 * sizeof(mac_add_t)));
    
    if(vlan_8021q_hdr->tpid == 0x8100)
        return vlan_8021q_hdr;
    
    return NULL;
}

static inline unsigned int
GET_802_1Q_VLAN_ID(vlan_8021q_hdr_t * vlan_8021q_hdr)
{
    printf("\nfn : %s\n", __FUNCTION__);

    return (unsigned int)vlan_8021q_hdr->tci_vid;
}


#define VLAN_ETH_FCS(vlan_eth_hdr_ptr, payload_size) \
(*(unsigned int *)(((char *)(((vlan_ethernet_hdr_t *)vlan_eth_hdr_ptr)->payload) + payload_size)))

#define VLAN_ETH_HDR_SIZE_EXCL_PAYLOAD \
    (sizeof(vlan_ethernet_hdr_t) - sizeof(((vlan_ethernet_hdr_t *)0)->payload))

static inline char *
GET_ETHERNET_HDR_PAYLOAD(ethernet_hdr_t *ethernet_hdr)
{
    printf("\nfn : %s\n", __FUNCTION__);

    vlan_8021q_hdr_t *vlan_8021q_hdr = is_pkt_vlan_tagged(ethernet_hdr);
    
    if(vlan_8021q_hdr) {
        vlan_ethernet_hdr_t *vlan_ethernet_hdr = (vlan_ethernet_hdr_t *)(ethernet_hdr);
        return (char *)vlan_ethernet_hdr->payload;
    }
    
    return (char *)ethernet_hdr->payload;
}



#define GET_COMMON_ETH_FCS(eth_hdr_ptr, payload_size) \
    (is_pkt_vlan_tagged(eth_hdr_ptr) ? VLAN_ETH_FCS(eth_hdr_ptr, payload_size) : \
    ETH_FCS(eth_hdr_ptr, payload_size))



static inline void
SET_COMMON_ETH_FCS(ethernet_hdr_t *ethernet_hdr,
                   unsigned int payload_size, unsigned int new_fcs)
{
    printf("\nfn : %s\n", __FUNCTION__);

    if(is_pkt_vlan_tagged(ethernet_hdr))
        VLAN_ETH_FCS(ethernet_hdr, payload_size) = new_fcs;
    else
        ETH_FCS(ethernet_hdr, payload_size) = new_fcs;
}


static inline unsigned int
GET_ETH_HDR_SIZE_EXCL_PAYLOAD(ethernet_hdr_t *ethernet_hdr)
{
    printf("\nfn : %s\n", __FUNCTION__);

    if(is_pkt_vlan_tagged(ethernet_hdr)) {
        return VLAN_ETH_HDR_SIZE_EXCL_PAYLOAD;
    }
    else
        return ETH_HDR_SIZE_EXCL_PAYLOAD;
}



static inline bool_t
l2_frame_recv_qualify_on_interface(interface_t *interface,
                                   ethernet_hdr_t *eth_hdr,
                                   unsigned int *output_vlan_id)
{
    printf("\nfn : %s\n", __FUNCTION__);

    *output_vlan_id = 0;
    
    
    vlan_8021q_hdr_t *vlan_8021q_hdr = is_pkt_vlan_tagged(eth_hdr);
    
    
    if(!(IS_INTF_L3_MODE(interface)) && IF_L2_MODE(interface) == L2_MODE_UNKNOWN)
        return FALSE;
    
    if(IF_L2_MODE(interface) == ACCESS &&
       get_access_intf_operating_vlan_id(interface) == 0)
    {
        return FALSE;
    }
    
    
    unsigned int intf_vlan_id = 0, pkt_vlan_id = 0;
    
    if(IF_L2_MODE(interface) == ACCESS)
    {
        intf_vlan_id = get_access_intf_operating_vlan_id(interface);
        
        if(!vlan_8021q_hdr && intf_vlan_id)
        {
            *output_vlan_id = intf_vlan_id;
            return TRUE;
        }
        
        pkt_vlan_id = GET_802_1Q_VLAN_ID(vlan_8021q_hdr);
        
        if(pkt_vlan_id == intf_vlan_id)
            return TRUE;
        else
            return FALSE;
    }
    
    if(IF_L2_MODE(interface) == TRUNK)
    {
        if(!vlan_8021q_hdr)
            return FALSE;
    }
    
    if(IF_L2_MODE(interface) == TRUNK && vlan_8021q_hdr)
    {
        pkt_vlan_id = GET_802_1Q_VLAN_ID(vlan_8021q_hdr);
        if(is_trunk_interface_vlan_enabled(interface, pkt_vlan_id))
            return TRUE;
        else
            return FALSE;
    }
    
    if(IS_INTF_L3_MODE(interface) && vlan_8021q_hdr)
        return FALSE;
    
    
    if(IS_INTF_L3_MODE(interface) &&
       memcmp(IF_MAC(interface),
              eth_hdr->dst_mac.mac,
              sizeof(mac_add_t)) == 0)
    {
        return TRUE;
    }
    
    
    if(IS_INTF_L3_MODE(interface) &&
            IS_MAC_BROADCAST(eth_hdr->dst_mac.mac))
        return TRUE;
    
    return FALSE;
}


ethernet_hdr_t *
tag_pkt_with_vlan_id(ethernet_hdr_t *ethernet_hdr, unsigned int total_pkt_size,
                     int vlan_id, unsigned int *new_pkt_size);

ethernet_hdr_t *
untag_pkt_with_vlan_id(ethernet_hdr_t *ethernet_hdr,
                       unsigned int total_pkt_size,
                       unsigned int *new_pkt_size);







#endif /* layer2_h */
