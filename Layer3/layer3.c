#include "layer3.h"
#include <arpa/inet.h>
#include <assert.h>
#include "comm.h"
#include "graph.h"

extern void
demote_pkt_to_layer2(node_t *node,
                     unsigned int next_hop_ip,
                     char *oif,
                     char *pkt,
                     unsigned int pkt_size,
                     int protocol_number);

static bool_t
l3_is_direct_route(l3_route_t *l3_route)
{
    return l3_route->is_direct;
}

bool_t
is_layer3_local_delivery(node_t *node, uint32_t dst_ip)
{
    char ip[16];
    ip_n_to_p(dst_ip, ip);
    ip[15] = '\0';
    if(strncmp(NODE_LO_ADD(node), ip, 16) == 0)
        return TRUE;
    
    interface_t *interface = NULL;
   
    for(int i=0; i<MAX_INTF_PER_NODE; ++i)
    {
        interface = node->interfaces[i];
        if(!interface) break;
        if(!interface->intf_nw_props.is_ipaddr_config)
            continue;
        
        if(strncmp(IF_IP(interface), ip, 16) == 0)
            return TRUE;
    }
    
    return FALSE;
}

void
init_rt_table(rt_table_t **rt_table)
{
    (*rt_table) = calloc(1, sizeof(rt_table_t));
    
    init_glthread(&((*rt_table)->route_list));
}

l3_route_t *
rt_table_lookup(rt_table_t *rt_table, char *dest,
                char mask)
{
    l3_route_t *l3_route = NULL;
    glthread_t *curr = NULL;
    
    ITERATE_GLTHREAD_BEGIN(&rt_table->route_list, curr)
    {
        l3_route = rt_glue_to_l3_route(curr);
        if(strncmp(dest, l3_route->dest, 16) == 0 &&
           l3_route->mask == mask)
            return l3_route;
    }ITERATE_GLTHREAD_END(&rt_table->route_list, curr);
    
    
    return NULL;
}

void
delete_rt_table_entry(rt_table_t *rt_table, char *dest, char mask)
{
    char dst_str_with_mask[16];
    apply_mask(dest, mask, dst_str_with_mask);
    l3_route_t *l3_route = rt_table_lookup(rt_table, dst_str_with_mask,
                                           mask);
    
    if(!l3_route) return ;
    remove_glthread(&l3_route->rt_glue);
    free(l3_route);
}

static bool_t
_rt_table_entry_add(rt_table_t *rt_table, l3_route_t *l3_route)
{
    l3_route_t *l3_route_old = rt_table_lookup(rt_table,
                                               l3_route->dest,
                                               l3_route->mask);
    
    if(l3_route_old && IS_L3_ROUTES_EQUAL(l3_route_old, l3_route))
    {
        return FALSE;
    }
    
    if(l3_route_old)
    {
        delete_rt_table_entry(rt_table, l3_route_old->dest,
                              l3_route_old->mask);
    }
    
    init_glthread(&l3_route->rt_glue);
    glthread_add_next(&rt_table->route_list, &l3_route->rt_glue);
    return TRUE;
}

void
rt_table_add_direct_route(rt_table_t *rt_table,
                          char *dst, char mask)
{
   rt_table_add_route(rt_table, dst, mask, 0, 0);
}

void
rt_table_add_route(rt_table_t *rt_table,
                   char *dst, char mask,
                   char *gw, char *oif)
{
    unsigned int dst_int;
    char dst_str_with_mask[16];
    
    apply_mask(dst, mask, dst_str_with_mask);
    
    inet_pton(AF_INET, dst_str_with_mask, &dst_int);
    
    l3_route_t *l3_route = l3rib_lookup_lpm(rt_table, dst_int);
    
    assert(!l3_route);
    
    l3_route = calloc(1, sizeof(l3_route_t));
    strncpy(l3_route->dest, dst_str_with_mask, 16);
    l3_route->dest[15] = '\0';
    l3_route->mask = mask;
    
    l3_route->is_direct = (!gw && !oif) ? TRUE : FALSE;
    
    if(gw && oif)
    {
        strncpy(l3_route->gw_ip, gw, 16);
        l3_route->gw_ip[15] = '\0';
        strncpy(l3_route->oif, oif, IF_NAME_SIZE);
        l3_route->oif[IF_NAME_SIZE - 1] = '\0';
    }
    
    if(!_rt_table_entry_add(rt_table, l3_route))
    {
        printf("Error : Route %s/%d Installation failed\n",
               dst_str_with_mask, mask);
        free(l3_route);
    }
}

l3_route_t *
l3rib_lookup_lpm(rt_table_t *rt_table,
                 uint32_t dest_ip)
{
    l3_route_t *l3_route = NULL,
               *lpm_l3_route = NULL,
               *default_l3_route = NULL;
    
    glthread_t *curr = NULL;
    char dest_ip_str[16], subnet[16], longest_mask = 0;
    ip_n_to_p(dest_ip, dest_ip_str);
    
    
    ITERATE_GLTHREAD_BEGIN(&rt_table->route_list, curr)
    {
        l3_route = rt_glue_to_l3_route(curr);
        
        memset(subnet, 0, 16);
        
        apply_mask(dest_ip_str, l3_route->mask, subnet);
        
        if(strncmp("0.0.0.0", l3_route->dest, 16) == 0 &&
           l3_route->mask == 0) {
            default_l3_route = l3_route;
        }
        else if(strncmp(l3_route->dest, subnet, strlen(subnet)) == 0)
        {
            if(l3_route->mask > longest_mask)
            {
                longest_mask = l3_route->mask;
                lpm_l3_route = l3_route;
            }
        }
            
    } ITERATE_GLTHREAD_END(&rt_table->route_list, curr)
    
    return lpm_l3_route ? lpm_l3_route : default_l3_route;
}


static void
layer3_pkt_recv_from_top(node_t *node, char *pkt, unsigned int size,
                         int protocol_number, unsigned int dst_ip_addr)
{
    ip_hdr_t ip_hdr;
    
    initialize_ip_hdr(&ip_hdr);
    
    ip_hdr.protocol = protocol_number;
    
    unsigned int addr_int = 0;
    
    addr_int = ip_p_to_n(NODE_LO_ADD(node));
    
    ip_hdr.src_ip = addr_int;
    ip_hdr.dst_ip = dst_ip_addr;
    
    ip_hdr.total_length = (unsigned short) ip_hdr.ihl +
                          (unsigned short)(size/4) +
                          (unsigned short) ((size % 4) ? 1 : 0);
    
    l3_route_t *l3_route = l3rib_lookup_lpm(NODE_RT_TABLE(node), ip_hdr.dst_ip);
    
    char ip_address[16];
    
    ip_n_to_p(dst_ip_addr, ip_address);
    
    if(!l3_route)
    {
        printf("Router %s : Cannot Route IP : %s\n",
               node->name, ip_address);
        return;
    }
    
    char *new_pkt = NULL;
    unsigned int new_pkt_size = 0;
    
    new_pkt_size = ip_hdr.total_length * 4;
    new_pkt = calloc(1, MAX_PACKET_BUFFER_SIZE);
    
    memcpy(new_pkt, (char *)&ip_hdr, ip_hdr.ihl * 4);
    
    if(pkt && size)
        memcpy(new_pkt + ip_hdr.ihl * 4, pkt, size);
    
    bool_t is_direct_route = l3_is_direct_route(l3_route);
    
    unsigned int next_hop_ip;
    
    if(!is_direct_route)
    {
        next_hop_ip =  ip_p_to_n(l3_route->gw_ip);
    }
    else
    {
        next_hop_ip = dst_ip_addr;
    }
    
    char *shifted_pkt_buffer = pkt_buffer_shift_right(new_pkt,
                                                      new_pkt_size,
                                                      MAX_PACKET_BUFFER_SIZE);
    
    demote_pkt_to_layer2(node, next_hop_ip,
                         is_direct_route ? 0 : l3_route->oif,
                         shifted_pkt_buffer,
                         new_pkt_size,
                         ETH_IP);
    free(new_pkt);
}

void
demote_packet_to_layer3(node_t *node,
                        char *pkt, unsigned int size,
                        int protocol_number,
                        unsigned int dst_ip_addr)
{
    layer3_pkt_recv_from_top(node, pkt, size, protocol_number, dst_ip_addr);
}

static void
layer3_ip_pkt_recv_from_layer2(node_t *node,
                               interface_t *interface,
                               ip_hdr_t *pkt,
                               unsigned int pkt_size)
{
    char dst_ip_add[16];
    
    ip_hdr_t *ip_hdr = pkt;
    
    ip_n_to_p(ip_hdr->dst_ip, dst_ip_add);
    
    l3_route_t *l3_route = l3rib_lookup_lpm(NODE_RT_TABLE(node),
                                            ip_hdr->dst_ip);
    
    if(!l3_route)
    {
        printf("Router %s : Cannot Route IP : %s\n",
               node->name, dst_ip_add);
        return;
    }
    
    if(l3_is_direct_route(l3_route))
    {
        if(is_layer3_local_delivery(node, ip_hdr->dst_ip))
        {
            switch(ip_hdr->protocol)
            {
                case ICMP_PRO:
                    printf("IP Address : %s, ping success\n", dst_ip_add);
                    break;
                case IP_IN_IP:{
                    printf("Recvd IP in IP pkt on node : %s\n", node->name);
                    
                    layer3_ip_pkt_recv_from_layer2(node, interface,
                                                   (ip_hdr_t *)INCREMENT_IPHDR(ip_hdr),
                                                   IP_HDR_PAYLOAD_SIZE(ip_hdr));
                    
                    return;
                }
                default:
                    ;
            }
            return;
        }
        demote_pkt_to_layer2(node,
                             0,
                             NULL,
                             (char *)ip_hdr,
                             pkt_size,
                             ETH_IP);
        return;
    }
    
    ip_hdr->ttl--;
    
    if(ip_hdr->ttl == 0) return;
    
    unsigned int next_hop_ip = 0;
    next_hop_ip = ip_p_to_n(l3_route->gw_ip);
    
    demote_pkt_to_layer2(node, next_hop_ip,
                         l3_route->oif, (char *)ip_hdr,
                         pkt_size, ETH_IP);
}

static void
_layer3_pkt_recv_from_layer2(node_t *node,
                             interface_t *interface,
                             char *pkt, uint32_t pkt_size,
                             int L3_protocol_number)
{
    switch(L3_protocol_number)
    {
        case ETH_IP:
            layer3_ip_pkt_recv_from_layer2(node, interface, (ip_hdr_t *)pkt, pkt_size);
            break;
        default:
            break;
    }
}

void promote_pkt_to_layer3(node_t *node,
                              interface_t *interface,
                              char *pkt, unsigned int pkt_size,
                              int L3_protocol_number)
{
    _layer3_pkt_recv_from_layer2(node, interface,
                                pkt, pkt_size, L3_protocol_number);
}

void
dump_rt_table(rt_table_t *rt_table)
{
    l3_route_t *l3_route = NULL;
    glthread_t *curr = NULL;
    
    printf("L3 Routing Table:\n");
    
    ITERATE_GLTHREAD_BEGIN(&rt_table->route_list, curr)
    {
        l3_route = rt_glue_to_l3_route(curr);
        
        printf("\t%-18s %-4d %-18s %s\n",
               l3_route->dest, l3_route->mask,
               l3_route->is_direct ? "NA" : (char *)l3_route->gw_ip,
               l3_route->is_direct ? "NA" : l3_route->oif);
       
    }ITERATE_GLTHREAD_END(&rt_table->route_list, curr);
}
