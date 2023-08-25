#include <stdio.h>
#include "../graph.h"
#include "layer2.h"
#include "../comm.h"
#include <assert.h>



typedef struct mac_table_entry_
{
    mac_add_t mac;
    char oif_name[IF_NAME_SIZE];
    glthread_t mac_entry_glue;
} mac_table_entry_t;
GLTHREAD_TO_STRUCT(mac_entry_glue_to_mac_entry, mac_table_entry_t, mac_entry_glue);

typedef struct mac_table_
{
    glthread_t mac_entries;
} mac_table_t;

void init_mac_table(mac_table_t **mac_table)
{
    *mac_table = calloc(1, sizeof(mac_table));
    init_glthread(&((*mac_table)->mac_entries));
}

mac_table_entry_t *
mac_table_lookup(mac_table_t *mac_table, char *mac)
{
    glthread_t *curr;
    mac_table_entry_t *mac_table_entry;
    
    ITERATE_GLTHREAD_BEGIN(&mac_table->mac_entries, curr) {
        mac_table_entry = mac_entry_glue_to_mac_entry(curr);
        if(strncmp(mac_table_entry->mac.mac, mac, sizeof(mac_add_t)) == 0)
            return mac_table_entry;
    } ITERATE_GLTHREAD_END(&mac_table->mac_entries, curr);
    
    return NULL;
}

void
delete_mac_table_entry(mac_table_t *mac_table, char *mac)
{
    mac_table_entry_t *mac_table_entry;
    mac_table_entry = mac_table_lookup(mac_table, mac);
    if(!mac_table_entry) return;
    remove_glthread(&mac_table_entry->mac_entry_glue);
    free(mac_table_entry);
}

bool_t mac_table_entry_add(mac_table_t *mac_table,
                           mac_table_entry_t *mac_table_entry)
{
    mac_table_entry_t *mac_table_entry_old = mac_table_lookup(mac_table, mac_table_entry->mac.mac);
    
    if(mac_table_entry_old && strncmp(mac_table_entry_old->mac.mac, mac_table_entry->mac.mac, sizeof(mac_add_t)) == 0
       && strncmp(mac_table_entry_old->oif_name, mac_table_entry->oif_name, sizeof(IF_NAME_SIZE)) == 0 ) {
        return FALSE;
    }
    
    if(mac_table_entry_old) delete_mac_table_entry(mac_table, mac_table_entry_old->mac.mac);
    
    init_glthread(&mac_table_entry->mac_entry_glue);
    glthread_add_next(&mac_table->mac_entries, &mac_table_entry->mac_entry_glue);
    return TRUE;
}

void
dump_mac_table(mac_table_t *mac_table)
{
    glthread_t *curr;
    mac_table_entry_t *mac_table_entry;
    
    ITERATE_GLTHREAD_BEGIN(&mac_table->mac_entries, curr) {
        mac_table_entry = mac_entry_glue_to_mac_entry(curr);
        printf("MAC : %u:%u:%u:%u:%u:%u, OIF : %s\n",
               mac_table_entry->mac.mac[0],
               mac_table_entry->mac.mac[1],
               mac_table_entry->mac.mac[2],
               mac_table_entry->mac.mac[3],
               mac_table_entry->mac.mac[4],
               mac_table_entry->mac.mac[5],
               mac_table_entry->oif_name);
    } ITERATE_GLTHREAD_END(&mac_table->mac_entries, curr);
}


static void
l2_switch_perform_mac_learning(node_t *node, char *src_mac, char *if_name)
{
    mac_table_entry_t *mac_table_entry = calloc(1, sizeof(mac_table_entry_t));
    memcpy(mac_table_entry->mac.mac, src_mac, sizeof(mac_add_t));
    strncpy(mac_table_entry->oif_name, if_name, IF_NAME_SIZE);
    mac_table_entry->oif_name[15] = '\0';
    bool_t result = mac_table_entry_add(node->node_nw_prop.mac_table, mac_table_entry);
    if(result == FALSE)
        free(mac_table_entry);
}

static void
l2_switch_forward_frame(node_t *node, interface_t *recv_intf,
                     char *pkt, unsigned int pkt_size)
{
    ethernet_hdr_t *ethernet_hdr = (ethernet_hdr_t *) pkt;
    
    if(IS_MAC_BROADCAST(ethernet_hdr->dst_mac.mac))
    {
        send_pkt_flood(node, recv_intf, pkt, pkt_size);
        return;
    }
    
    mac_table_entry_t *mac_table_entry = mac_table_lookup(NODE_MAC_TABLE(node),
                                                          ethernet_hdr->dst_mac.mac);
    if(!mac_table_entry)
    {
        send_pkt_flood(node, recv_intf, pkt, pkt_size);
        return;
    }
    
    char *oif_name = mac_table_entry->oif_name;
    interface_t *oif = get_node_if_by_name(node, oif_name);
    if(!oif) return;
    send_pkt_out(pkt, pkt_size, oif);
}



void
l2_switch_recv_frame(interface_t *interface,
                     char *pkt, unsigned int pkt_size)
{
    node_t *node = interface->att_node;
    ethernet_hdr_t *ethernet_hdr = (ethernet_hdr_t *) pkt;
    
    char *dst_mac = (char *)ethernet_hdr->dst_mac.mac;
    char *src_mac = (char *)ethernet_hdr->src_mac.mac;
    
    l2_switch_perform_mac_learning(node, src_mac, interface->if_name);
    l2_switch_forward_frame(node, interface, pkt, pkt_size);
}


static bool_t
l2_switch_send_pkt_out(char *pkt, unsigned int pkt_size,
                       interface_t *oif)
{
    assert(!IS_INTF_L3_MODE(oif));
    
    intf_l2_mode_t intf_l2_mode = IF_L2_MODE(oif);
    
    if(intf_l2_mode == L2_MODE_UNKNOWN)
        return FALSE;
    
    ethernet_hdr_t *ethernet_hdr = (ethernet_hdr_t *)pkt;
    
    vlan_8021q_hdr_t *vlan_8021q_hdr = is_pkt_vlan_tagged((ethernet_hdr_t *)pkt);
    
    unsigned int pkt_vlan_id = 0, intf_vlan_id = 0;
    
    switch (intf_l2_mode) {
        case ACCESS:
        {
            
            intf_vlan_id = get_access_intf_operating_vlan_id(oif);
            
            assert(intf_vlan_id);
            
            if(vlan_8021q_hdr &&
                    intf_vlan_id == GET_802_1Q_VLAN_ID(vlan_8021q_hdr))
            {
                unsigned int new_pkt_size = 0;
                
                ethernet_hdr = untag_pkt_with_vlan_id(ethernet_hdr, pkt_size, &new_pkt_size);
                send_pkt_out((char *)ethernet_hdr, new_pkt_size, oif);
                return TRUE;
            }
            else
                return FALSE;
        }
            break;
        case TRUNK:
        {
            if(vlan_8021q_hdr){
                pkt_vlan_id = GET_802_1Q_VLAN_ID(vlan_8021q_hdr);
            }
            
            if(pkt_vlan_id && is_trunk_interface_vlan_enabled(oif, pkt_vlan_id))
            {
                send_pkt_out((char *)pkt, pkt_size, oif);
                return TRUE;
            }
            
            return FALSE;
        }
            break;
        default:
            return FALSE;
            break;
    }
   
    return FALSE;
}

static bool_t
l2_switch_flood_pkt_out(node_t *node, interface_t *exempted_intf,
                        char *pkt, unsigned int pkt_size)
{
    int i = 0;
    
    interface_t *oif = NULL;
    ethernet_hdr_t *ethernet_hdr = (ethernet_hdr_t *)pkt;
    vlan_8021q_hdr_t *vlan_8021q_hdr = is_pkt_vlan_tagged(ethernet_hdr);
    unsigned int output_vlan_id = 0;
    
    char *pkt_copy = NULL;
    char *temp_pkt = calloc(1, MAX_PACKET_BUFFER_SIZE);
    
    pkt_copy = temp_pkt + MAX_PACKET_BUFFER_SIZE - pkt_size;
    
    for(; i < MAX_INTF_PER_NODE; ++i)
    {
        oif = node->interfaces[i];
        if(!oif) break;
        if(oif == exempted_intf || IS_INTF_L3_MODE(oif)) continue;
            
        memcpy(pkt_copy, pkt, pkt_size);
        l2_switch_send_pkt_out(pkt_copy, pkt_size, oif);
    }
    
    free(temp_pkt);
    
    return TRUE;
}

