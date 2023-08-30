

#include "layer2.h"
#include <assert.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../comm.h"



extern void
l2_switch_recv_frame(interface_t *interface,
                     char *pkt, unsigned int pkt_size);

extern void
promote_pkt_to_layer3(node_t *node, interface_t *interface,
                      char *pkt, unsigned int pkt_size,
                      int L3_protocol_type);


void init_arp_table(arp_table_t **arp_table)
{
    *arp_table = calloc(1, sizeof(arp_table_t));
    init_glthread(&((*arp_table)->arp_entries));
}


arp_entry_t *
arp_table_lookup(arp_table_t *arp_table, char *ip_addr)
{
    glthread_t *curr;
    arp_entry_t *arp_entry;
    
    ITERATE_GLTHREAD_BEGIN(&arp_table->arp_entries, curr) {
        arp_entry = arp_glue_arp_entry(curr);
        if (strncmp(ip_addr, arp_entry->ip_addr.ip_addr, 16) == 0)
            return arp_entry;
    } ITERATE_GLTHREAD_END(&arp_table->arp_entries, curr);
    
    return NULL;
}


static void
delete_arp_pending_entry(arp_pending_entry_t *arp_pending_entry)
{
    remove_glthread(&arp_pending_entry->arp_pending_entry_glue);
    free(arp_pending_entry);
}


void
delete_arp_entry(arp_entry_t *arp_entry)
{
    glthread_t *curr;
    arp_pending_entry_t *arp_pending_entry = NULL;
    remove_glthread(&arp_entry->arp_glue);
    
    ITERATE_GLTHREAD_BEGIN(&arp_entry->arp_pending_list, curr)
    {
        arp_pending_entry = arp_pending_entry_glue_to_arp_pending_entry(curr);
        delete_arp_pending_entry(arp_pending_entry);
    } ITERATE_GLTHREAD_END(&arp_entry->arp_pending_list, curr);
    
    free(arp_entry);
}


bool_t
arp_table_entry_add(arp_table_t *arp_table,
                    arp_entry_t *arp_entry,
                    glthread_t **arp_pending_list)
{
    
    if(arp_pending_list)
        assert(*arp_pending_list == NULL);
    
    
    arp_entry_t *arp_entry_old = arp_table_lookup(arp_table,
                                                  arp_entry->ip_addr.ip_addr);
    
    if(!arp_entry_old) {
        init_glthread(&arp_entry->arp_glue);
        glthread_add_next(&arp_table->arp_entries, &arp_entry->arp_glue);
        return TRUE;
    }
    
    if(arp_entry_old &&
       IS_ARP_ENTRIES_EQUAL(arp_entry_old, arp_entry))
    {
        delete_arp_table_entry(arp_table, arp_entry_old->ip_addr.ip_addr);
        return FALSE;
    }
    
    if(arp_entry_old && !arp_entry_sane(arp_entry_old))
    {
        delete_arp_entry(arp_entry_old);
        init_glthread(&arp_entry->arp_glue);
        glthread_add_next(&arp_table->arp_entries, &arp_entry->arp_glue);
        return TRUE;
    }
    
    if(arp_entry_old &&
       arp_entry_sane(arp_entry_old) &&
       arp_entry_sane(arp_entry))
    {
        if(!IS_GLTHREAD_LIST_EMPTY(&arp_entry_old->arp_pending_list))
        {
            glthread_add_next(&arp_entry_old->arp_pending_list,
                              arp_entry->arp_pending_list.right);
        }
        
        if(arp_pending_list) {
            *arp_pending_list = &arp_entry_old->arp_pending_list;
        }
        
        return FALSE;
    }
    
    if(arp_entry_old &&
       arp_entry_sane(arp_entry_old) &&
       !arp_entry_sane(arp_entry))
    {
        strncpy(arp_entry_old->mac_addr.mac, arp_entry->mac_addr.mac,
                sizeof(mac_add_t));
        strncpy(arp_entry_old->oif_name, arp_entry->oif_name,
                IF_NAME_SIZE);
        arp_entry_old->oif_name[IF_NAME_SIZE - 1] = '\0';
        
        if(arp_pending_list) {
            *arp_pending_list = &arp_entry_old->arp_pending_list;
        }
        
        return FALSE;
    }
    
    return FALSE;
}

static void
pending_arp_processing_callback_function(node_t *node,
                                         interface_t *oif,
                                         arp_entry_t *arp_entry,
                                         arp_pending_entry_t *arp_pending_entry)
{
    ethernet_hdr_t* ethernet_hdr = (ethernet_hdr_t *)(arp_pending_entry->pkt);
    uint32_t pkt_size = arp_pending_entry->pkt_size;
    
    memcpy(ethernet_hdr->dst_mac.mac, arp_entry->mac_addr.mac, sizeof(mac_add_t));
    memcpy(ethernet_hdr->src_mac.mac, IF_MAC(oif), sizeof(mac_add_t));
    SET_COMMON_ETH_FCS(ethernet_hdr,
                       pkt_size - GET_ETH_HDR_SIZE_EXCL_PAYLOAD(ethernet_hdr),
                       0);
    send_pkt_out((char *)ethernet_hdr, pkt_size, oif);
    
}

static void
process_arp_pending_entry(node_t *node,
                          interface_t *oif,
                          arp_entry_t *arp_entry,
                          arp_pending_entry_t *arp_pending_entry)
{
    arp_pending_entry->cb(node, oif, arp_entry, arp_pending_entry);
}


void
arp_table_update_from_arp_reply(arp_table_t *arp_table,
                                     arp_hdr_t *arp_hdr,
                                     interface_t *iif)
{
    unsigned int src_ip = 0;
    glthread_t *arp_pending_list = NULL;
    
    assert(arp_hdr->op_code == ARP_REPLY);
    
    arp_entry_t *arp_entry = calloc(1, sizeof(arp_entry_t));
    
    src_ip = htonl(arp_hdr->src_ip);
    
    ip_n_to_p(src_ip, arp_entry->ip_addr.ip_addr);
    
    memcpy(arp_entry->mac_addr.mac, arp_hdr->src_mac.mac, sizeof(mac_add_t));
    
    strncpy(arp_entry->oif_name, iif->if_name, IF_NAME_SIZE);
    arp_entry->oif_name[IF_NAME_SIZE - 1] = '\0';
    
    arp_entry->is_sane = FALSE;
    
    bool_t rc = arp_table_entry_add(arp_table, arp_entry, &arp_pending_list);
    
    glthread_t *curr;
    arp_pending_entry_t *arp_pending_entry;
    
    if(arp_pending_list)
    {
        ITERATE_GLTHREAD_BEGIN(arp_pending_list, curr)
        {
            arp_pending_entry = arp_pending_entry_glue_to_arp_pending_entry(curr);
            
            remove_glthread(&arp_pending_entry->arp_pending_entry_glue);
            
            process_arp_pending_entry(iif->att_node,
                                      iif, arp_entry, arp_pending_entry);
            delete_arp_pending_entry(arp_pending_entry);
            
        } ITERATE_GLTHREAD_END(arp_pending_list, curr);
        
        (arp_pending_list_to_arp_entry(arp_pending_list)->is_sane = FALSE);
    }
    
    if(rc == FALSE)
        delete_arp_entry(arp_entry);
}


void
delete_arp_table_entry(arp_table_t *arp_table, char *ip_addr)
{
    arp_entry_t *arp_entry = arp_table_lookup(arp_table, ip_addr);
    
    if(!arp_entry)
        return;
    
    delete_arp_entry(arp_entry);
}


void
dump_arp_table(arp_table_t *arp_table)
{
    glthread_t *curr;
    arp_entry_t *arp_entry;
    
    ITERATE_GLTHREAD_BEGIN(&arp_table->arp_entries, curr) {
        arp_entry = arp_glue_arp_entry(curr);
        printf("IP : %s, MAC : %u:%u:%u:%u:%u:%u, OIF : %s\n",
               arp_entry->ip_addr.ip_addr,
               arp_entry->mac_addr.mac[0],
               arp_entry->mac_addr.mac[1],
               arp_entry->mac_addr.mac[2],
               arp_entry->mac_addr.mac[3],
               arp_entry->mac_addr.mac[4],
               arp_entry->mac_addr.mac[5],
               arp_entry->oif_name);
    } ITERATE_GLTHREAD_END(&arp_table->arp_entries, curr);
}


void send_arp_broadcast_request(node_t *node, interface_t *oif, char *ip_addr)
{
    unsigned int payload_size = sizeof(arp_hdr_t);
    
    ethernet_hdr_t *ethernet_hdr = (ethernet_hdr_t *)calloc(1,
                    ETH_HDR_SIZE_EXCL_PAYLOAD + payload_size);
    if(!oif){
        oif = node_get_matching_subnet_interface(node, ip_addr);
        if(!oif) {
            printf("Error: %s : No eligible subnet for ARP resolution for IP-address : %s\n",
                   node->name, ip_addr);
            return;
        }
    }
    
    layer2_fill_with_broadcast_mac(ethernet_hdr->dst_mac.mac);
    memcpy(ethernet_hdr->src_mac.mac, IF_MAC(oif), sizeof(mac_add_t));
    ethernet_hdr->type = ARP_MSG;
    
    arp_hdr_t *arp_hdr = (arp_hdr_t *)ethernet_hdr->payload;
    arp_hdr->hw_type = 1;
    arp_hdr->proto_type = 0x0800;
    arp_hdr->hw_addr_len = sizeof(mac_add_t);
    arp_hdr->proto_addr_len = 4;
    arp_hdr->op_code = ARP_BROAD_REQ;
    memcpy(arp_hdr->src_mac.mac, IF_MAC(oif), sizeof(mac_add_t));
    arp_hdr->src_ip = ip_p_to_n(oif->intf_nw_props.ip_add.ip_addr);
    memset(arp_hdr->dest_mac.mac, 0, sizeof(mac_add_t));
    arp_hdr->dest_ip = ip_p_to_n(ip_addr);
    
    ETH_FCS(ethernet_hdr, payload_size) = 0;
    
    send_pkt_out((char *)ethernet_hdr, ETH_HDR_SIZE_EXCL_PAYLOAD + payload_size, oif);
    
    free(ethernet_hdr);
}


static void
send_arp_reply_msg(ethernet_hdr_t *ethernet_hdr_in, interface_t *oif)
{
    arp_hdr_t *arp_hdr_in = (arp_hdr_t *)ethernet_hdr_in->payload;
    ethernet_hdr_t *ethernet_hdr_reply = (ethernet_hdr_t *)calloc(1, MAX_PACKET_BUFFER_SIZE);
    memcpy(ethernet_hdr_reply->dst_mac.mac, ethernet_hdr_in->src_mac.mac, sizeof(mac_add_t));
    memcpy(ethernet_hdr_reply->src_mac.mac, IF_MAC(oif), sizeof(mac_add_t));
    
    ethernet_hdr_reply->type = ARP_MSG;
    arp_hdr_t *arp_hdr_reply = (arp_hdr_t *)ethernet_hdr_reply->payload;
    arp_hdr_reply->hw_type = 1;
    arp_hdr_reply->proto_type = 0x0800;
    arp_hdr_reply->hw_addr_len = sizeof(mac_add_t);
    arp_hdr_reply->proto_addr_len = 4;
    arp_hdr_reply->op_code = ARP_REPLY;
    memcpy(arp_hdr_reply->src_mac.mac, IF_MAC(oif), sizeof(mac_add_t));
    arp_hdr_reply->src_ip = ip_p_to_n(IF_IP(oif));
    memcpy(arp_hdr_reply->dest_mac.mac, arp_hdr_in->src_mac.mac, sizeof(mac_add_t));
    arp_hdr_reply->dest_ip = arp_hdr_in->src_ip;
    ETH_FCS(ethernet_hdr_reply, sizeof(arp_hdr_t)) = 0;
    
    unsigned int total_pkt_size = ETH_HDR_SIZE_EXCL_PAYLOAD + sizeof(arp_hdr_t);
    char *shifted_pkt_buffer = pkt_buffer_shift_right((char *)ethernet_hdr_reply,
                                                      total_pkt_size, MAX_PACKET_BUFFER_SIZE);
    send_pkt_out(shifted_pkt_buffer, total_pkt_size, oif);
    free(ethernet_hdr_reply);
    
}


static void
process_arp_reply_msg(node_t *node, interface_t *iif, ethernet_hdr_t *ethernet_hdr)
{
    printf("%s : ARP reply msg recvd on interface %s of node %s\n",
           __FUNCTION__, iif->if_name, node->name);
    arp_table_update_from_arp_reply(NODE_ARP_TABLE(node),
                                    (arp_hdr_t *)ethernet_hdr->payload, iif);
}


static void
process_arp_broadcast_req(node_t *node, interface_t *iif, ethernet_hdr_t *ethernet_hdr)
{
    printf("%s : ARP Broadcast msg recvd on interface %s of node %s\n",
           __FUNCTION__, iif->if_name, node->name);
    
    char ip_addr[16];
    
    arp_hdr_t *arp_hdr = (arp_hdr_t *)ethernet_hdr->payload;
    
    ip_n_to_p(arp_hdr->dest_ip, ip_addr);
    ip_addr[15] = '\0';
    
    if(strncmp(ip_addr, IF_IP(iif), 16)) {
        printf("%s : ARP Broadcast req msg dropped, Dst IP address %s did not match with interface IP %s\n",
               node->name, ip_addr, IF_IP(iif));
        return ;
    }
    
    send_arp_reply_msg(ethernet_hdr, iif);
}


void
add_arp_pending_entry(arp_entry_t *arp_entry,
                      arp_processing_fn cb,
                      char *pkt,
                      unsigned int pkt_size)
{
    arp_pending_entry_t *arp_pending_entry =
    calloc(1, sizeof(arp_pending_entry) + pkt_size);
    
    init_glthread(&arp_pending_entry->arp_pending_entry_glue);
    arp_pending_entry->cb = cb;
    arp_pending_entry->pkt_size = pkt_size;
    memcpy(arp_pending_entry->pkt, pkt, pkt_size);
    
    glthread_add_next(&arp_entry->arp_pending_list,
                      &arp_pending_entry->arp_pending_entry_glue);
}



arp_entry_t *
create_arp_sane_entry(arp_table_t *arp_table,
                      char *ip_addr)
{
    arp_entry_t *arp_entry = arp_table_lookup(arp_table, ip_addr);
    
    if(arp_entry)
    {
        if(!arp_entry_sane(arp_entry))
        {
            assert(0);
        }
        return arp_entry;
    }
    
    arp_entry = calloc(1, sizeof(arp_entry_t));
    
    strncpy(arp_entry->ip_addr.ip_addr, ip_addr, 16);
    arp_entry->ip_addr.ip_addr[15] = '\0';
    
    init_glthread(&arp_entry->arp_pending_list);
    
    arp_entry->is_sane = TRUE;
    
    bool_t rc = arp_table_entry_add(arp_table, arp_entry, 0);
    
    if(rc == FALSE)
        assert(0);
   
    return arp_entry;
}



void interface_set_l2_mode(node_t *node,
                           interface_t *interface,
                           char *l2_mode_option)
{
    intf_l2_mode_t intf_l2_mode;
    
    if(strncmp(l2_mode_option, "access", strlen("access")) == 0)
        intf_l2_mode = ACCESS;
    else if(strncmp(l2_mode_option, "trunk", strlen("trunk")) == 0)
        intf_l2_mode = TRUNK;
    else
        assert(0);

    
    if(IS_INTF_L3_MODE(interface))
    {
        interface->intf_nw_props.is_ipaddr_config = FALSE;
        IF_L2_MODE(interface) = intf_l2_mode;
        return;
    }
    
    if(IF_L2_MODE(interface) == L2_MODE_UNKNOWN)
    {
        IF_L2_MODE(interface) = intf_l2_mode;
        return;
    }
    
    if(IF_L2_MODE(interface) == intf_l2_mode)
        return;
    
    
    if(IF_L2_MODE(interface) == ACCESS && intf_l2_mode == TRUNK)
    {
        IF_L2_MODE(interface) = intf_l2_mode;
        return;
    }
    
    if(IF_L2_MODE(interface) == TRUNK && intf_l2_mode == ACCESS)
    {
        IF_L2_MODE(interface) = intf_l2_mode;
        unsigned int i = 0;
        
        for(; i < MAX_VLAN_MEMBERSHIP; ++i)
        {
            interface->intf_nw_props.vlans[i] = 0;
        }
    }
}

void
interface_set_vlan(node_t *node, interface_t *interface, unsigned int vlan_id)
{
    if(IS_INTF_L3_MODE(interface))
    {
        printf("Error: Interface %s : L3 mode enabled\n",
               interface->if_name);
        return;
    }
    
    if(IF_L2_MODE(interface) != ACCESS && IF_L2_MODE(interface) != TRUNK)
    {
        printf("Error: Interface %s : L2 mode not enabled\n",
               interface->if_name);
        return;
    }
    
    if(IF_L2_MODE(interface) == ACCESS)
    {
        unsigned int i = 0, *vlan = NULL;
        for(; i < MAX_VLAN_MEMBERSHIP; ++i)
        {
            if(interface->intf_nw_props.vlans[i])
            {
                vlan = &interface->intf_nw_props.vlans[i];
            }
        }
        
        if(vlan)
        {
            *vlan = vlan_id;
            return;
        }
        
        interface->intf_nw_props.vlans[0] = vlan_id;
    }
    
    if(IF_L2_MODE(interface) == TRUNK)
    {
        unsigned int i = 0, *vlan = NULL;
        
        for(; i < MAX_VLAN_MEMBERSHIP; ++i)
        {
            if(!vlan && interface->intf_nw_props.vlans[i] == 0)
            {
                vlan = &interface->intf_nw_props.vlans[i];
            } else if(interface->intf_nw_props.vlans[i] == vlan_id)
            {
                return;
            }
        }
        
        if(vlan)
        {
            *vlan = vlan_id;
            return;
        }
        
        printf("Error : Interface %s : Max Vlans membership limit reached",
               interface->if_name);
    }
}


void
node_set_intf_vlan_membership(node_t *node,
                              char *intf_name,
                              unsigned int vlan_id)
{
    interface_t *interface = get_node_if_by_name(node, intf_name);
    assert(interface);
    
    interface_set_vlan(node, interface, vlan_id);
}

void node_set_intf_l2_mode(node_t *node, char *intf_name,
                           intf_l2_mode_t intf_l2_mode)
{
    interface_t *interface = get_node_if_by_name(node, intf_name);
    assert(interface);
    
    interface_set_l2_mode(node, interface, intf_l2_mode_str(intf_l2_mode));
}


ethernet_hdr_t *
tag_pkt_with_vlan_id(ethernet_hdr_t *ethernet_hdr, unsigned int total_pkt_size,
                     int vlan_id, unsigned int *new_pkt_size)
{
    unsigned int payload_size = 0;
    *new_pkt_size = 0;
    vlan_8021q_hdr_t *vlan_8021q_hdr = is_pkt_vlan_tagged(ethernet_hdr);
    
    if(vlan_8021q_hdr)
    {
        payload_size = total_pkt_size - VLAN_ETH_HDR_SIZE_EXCL_PAYLOAD;
        vlan_8021q_hdr->tci_vid = (short)vlan_id;
        
        SET_COMMON_ETH_FCS(ethernet_hdr, payload_size, 0);
        *new_pkt_size = total_pkt_size;
        return ethernet_hdr;
    }
    
    
    ethernet_hdr_t *temp_ethernet_hdr = calloc(1, sizeof(ethernet_hdr_t));
    
    memcpy((char *)temp_ethernet_hdr, (char *)ethernet_hdr,
           ETH_HDR_SIZE_EXCL_PAYLOAD - sizeof(temp_ethernet_hdr->FCS));
    
    payload_size = total_pkt_size - ETH_HDR_SIZE_EXCL_PAYLOAD;
    
    vlan_ethernet_hdr_t *vlan_ethernet_hdr =
            (vlan_ethernet_hdr_t *)((char *)ethernet_hdr - sizeof(vlan_8021q_hdr_t));
    
    memset((char *)vlan_ethernet_hdr, 0,
           VLAN_ETH_HDR_SIZE_EXCL_PAYLOAD - sizeof(vlan_ethernet_hdr->FCS));
    
    memcpy(vlan_ethernet_hdr->src_mac.mac, temp_ethernet_hdr->src_mac.mac, sizeof(mac_add_t));
    memcpy(vlan_ethernet_hdr->dst_mac.mac, temp_ethernet_hdr->dst_mac.mac, sizeof(mac_add_t));
    
    
    vlan_ethernet_hdr->vlan_8021q_hdr.tpid = VLAN_8021Q_PROTO;
    vlan_ethernet_hdr->vlan_8021q_hdr.tci_pcp = 0;
    vlan_ethernet_hdr->vlan_8021q_hdr.tci_dei = 0;
    vlan_ethernet_hdr->vlan_8021q_hdr.tci_vid = (short)vlan_id;
    
    vlan_ethernet_hdr->type = temp_ethernet_hdr->type;
    
    SET_COMMON_ETH_FCS((ethernet_hdr_t *)vlan_ethernet_hdr, payload_size, 0);
    
    *new_pkt_size = sizeof(vlan_8021q_hdr_t) + total_pkt_size;
    
    free(temp_ethernet_hdr);
    
    return (ethernet_hdr_t *)vlan_ethernet_hdr;
}


ethernet_hdr_t *
untag_pkt_with_vlan_id(ethernet_hdr_t *ethernet_hdr,
                       unsigned int total_pkt_size,
                       unsigned int *new_pkt_size)
{
    *new_pkt_size = 0;
    
    vlan_8021q_hdr_t* vlan_8021q_hdr = is_pkt_vlan_tagged(ethernet_hdr);
    
    if(!vlan_8021q_hdr)
    {
        *new_pkt_size = total_pkt_size;
        return ethernet_hdr;
    }
    
    vlan_ethernet_hdr_t* vlan_ethernet_hdr_old = calloc(1, sizeof(vlan_ethernet_hdr_old));
    memcpy((char *)vlan_ethernet_hdr_old, ethernet_hdr,
           VLAN_ETH_HDR_SIZE_EXCL_PAYLOAD - sizeof(vlan_ethernet_hdr_old->FCS));
    
    ethernet_hdr = (ethernet_hdr_t *)((char *)ethernet_hdr + sizeof(vlan_8021q_hdr_t));
    
    memcpy(ethernet_hdr->src_mac.mac, vlan_ethernet_hdr_old->src_mac.mac, sizeof(mac_add_t));
    memcpy(ethernet_hdr->dst_mac.mac, vlan_ethernet_hdr_old->dst_mac.mac, sizeof(mac_add_t));
    
    ethernet_hdr->type = vlan_ethernet_hdr_old->type;
    
    unsigned int payload_size = total_pkt_size - VLAN_ETH_HDR_SIZE_EXCL_PAYLOAD;
    SET_COMMON_ETH_FCS(ethernet_hdr, payload_size, 0);
    *new_pkt_size = total_pkt_size - sizeof(vlan_8021q_hdr_t);
    return ethernet_hdr;
}


static void
promote_pkt_to_layer2(node_t *node, interface_t *iif,
                      ethernet_hdr_t *ethernet_hdr,
                      uint32_t pkt_size)
{
    switch (ethernet_hdr->type)
    {
        case ARP_MSG:
        {
            arp_hdr_t *arp_hdr = (arp_hdr_t *)(ethernet_hdr->payload);
            switch (arp_hdr->op_code)
            {
                case ARP_BROAD_REQ:
                    process_arp_broadcast_req(node, iif, ethernet_hdr);
                    break;
                case ARP_REPLY:
                    process_arp_reply_msg(node, iif, ethernet_hdr);
                    break;
                default:
                    break;
            }
        }
            break;
        case ETH_IP:
            promote_pkt_to_layer3(node,
                                  iif,
                                  GET_ETHERNET_HDR_PAYLOAD(ethernet_hdr),
                                  pkt_size - GET_ETH_HDR_SIZE_EXCL_PAYLOAD(ethernet_hdr),
                                  ethernet_hdr->type);
            break;
        
        default:
            ;
    }
}

void
layer2_frame_recv(node_t *node, interface_t *interface,
                       char *pkt, unsigned int pkt_size)
{
    unsigned int vlan_id_to_tag = 0;
    ethernet_hdr_t *ethernet_hdr = (ethernet_hdr_t *) pkt;
    if(l2_frame_recv_qualify_on_interface(interface,
                                          ethernet_hdr,
                                          &vlan_id_to_tag) == FALSE)
    {
        printf("L2 Frame rejected on node %s\n", node->name);
        return;
    }
    
    printf("L2 Frame accepted on node %s\n", node->name);
    
    if(IS_INTF_L3_MODE(interface))
    {
        promote_pkt_to_layer2(node, interface, ethernet_hdr, pkt_size);
    }
    else if (IF_L2_MODE(interface) == ACCESS || IF_L2_MODE(interface) == TRUNK)
    {
        unsigned int new_pkt_size = 0;
        if(vlan_id_to_tag)
        {
            pkt = (char *)tag_pkt_with_vlan_id((ethernet_hdr_t*)pkt,
                                               pkt_size,
                                               vlan_id_to_tag,
                                               &new_pkt_size );
            assert(new_pkt_size != pkt_size);
        }
        l2_switch_recv_frame(interface,
                             pkt,
                             vlan_id_to_tag ? new_pkt_size : pkt_size);
    }
    else
        return;
}


extern bool_t
is_layer3_local_delivery(node_t *node, uint32_t dst_ip);


static void
l2_forward_ip_packet(node_t *node, unsigned int next_hop_ip,
                     char *outgoing_intf, ethernet_hdr_t *pkt,
                     unsigned int pkt_size)
{
    interface_t *oif = NULL;
    char next_hop_ip_str[16];
    arp_entry_t *arp_entry = NULL;
    ethernet_hdr_t *ethernet_hdr = (ethernet_hdr_t *)pkt;
    unsigned int ethernet_payload_size =
        pkt_size - ETH_HDR_SIZE_EXCL_PAYLOAD;
    
    ip_n_to_p(next_hop_ip, next_hop_ip_str);
    
    if(outgoing_intf)
    {
        oif = get_node_if_by_name(node, outgoing_intf);
        assert(oif);
        
        arp_entry = arp_table_lookup(NODE_ARP_TABLE(node),
                                     next_hop_ip_str);
        if(!arp_entry)
        {
            arp_entry = create_arp_sane_entry(NODE_ARP_TABLE(node),
                                              next_hop_ip_str);
            add_arp_pending_entry(arp_entry,
                                  pending_arp_processing_callback_function,
                                  (char *)pkt,
                                  pkt_size);
            
            send_arp_broadcast_request(node, oif, next_hop_ip_str);
            return;
        }
        else if(arp_entry_sane(arp_entry))
        {
            add_arp_pending_entry(arp_entry,
                                  pending_arp_processing_callback_function,
                                  (char *)pkt,
                                  pkt_size);
            return;
        }
        else
            goto l2_frame_prepare;
    }
    
    if(is_layer3_local_delivery(node, next_hop_ip))
    {
        promote_pkt_to_layer3(node, 0, GET_ETHERNET_HDR_PAYLOAD(ethernet_hdr),
                              pkt_size - GET_ETH_HDR_SIZE_EXCL_PAYLOAD(ethernet_hdr),
                              ethernet_hdr->type);
        return;
    }
    
    oif = node_get_matching_subnet_interface(node, next_hop_ip_str);
    
    if(!oif)
    {
        printf("%s : Error : Local matching subnet for IP : %s couldn't be found\n",
               node->name, next_hop_ip_str);
        return;
    }
    
    
    arp_entry = arp_table_lookup(NODE_ARP_TABLE(node), next_hop_ip_str);
    
    if(!arp_entry)
    {
        arp_entry = create_arp_sane_entry(NODE_ARP_TABLE(node), next_hop_ip_str);
        
        add_arp_pending_entry(arp_entry,
                              pending_arp_processing_callback_function,
                              (char *)pkt, pkt_size);
        
        send_arp_broadcast_request(node, oif, next_hop_ip_str);
        return;
    }
    else if (arp_entry_sane(arp_entry))
    {
        add_arp_pending_entry(arp_entry,
                              pending_arp_processing_callback_function,
                              (char *)pkt,
                              pkt_size);
        return;
    }
    
l2_frame_prepare:
    memcpy(ethernet_hdr->dst_mac.mac, arp_entry->mac_addr.mac, sizeof(mac_add_t));
    memcpy(ethernet_hdr->src_mac.mac, IF_MAC(oif), sizeof(mac_add_t));
    SET_COMMON_ETH_FCS(ethernet_hdr, ethernet_payload_size, 0);
    send_pkt_out((char *)ethernet_hdr, pkt_size, oif);
}


static void
layer2_pkt_recv_from_top(node_t *node,
                         unsigned int next_hop_ip,
                         char *oif,
                         char *pkt,
                         unsigned int pkt_size,
                         int protocol_number)
{
    assert(pkt_size < sizeof(((ethernet_hdr_t *)0)->payload));
    
    if(protocol_number == ETH_IP)
    {
        ethernet_hdr_t *eth_hdr = ALLOC_ETH_HDR_WITH_PAYLOAD(pkt, pkt_size);
        eth_hdr->type = ETH_IP;
        
        l2_forward_ip_packet(node, next_hop_ip, oif, eth_hdr,
                             pkt_size + ETH_HDR_SIZE_EXCL_PAYLOAD);
    }
}

void
demote_pkt_to_layer2(node_t *node,
                     unsigned int next_hop_ip,
                     char *oif,
                     char *pkt,
                     unsigned int pkt_size,
                     int protocol_number)
{
    layer2_pkt_recv_from_top(node, next_hop_ip, oif,
                             pkt, pkt_size, protocol_number);
}

