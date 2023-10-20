#include "net.h"
#include "graph.h"
#include "utils.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

static unsigned int
hash_code (void *ptr, unsigned int size)
{
    unsigned int value = 0, i = 0;
    char *str = (char *)ptr;
    while (i < size)
    {
        value += *str;
        value *= 97;
        ++str;
        ++i;
    }
    return value;
}


void
interface_assign_mac_address(interface_t *interface)
{
    node_t *node = interface->att_node;
    
    if(!node) return;
    
    unsigned int hash_code_val = 0;
    hash_code_val = hash_code(node->name, NODE_NAME_SIZE);
    hash_code_val *= hash_code(interface->if_name, IF_NAME_SIZE);
    
    memset(IF_MAC(interface), 0, sizeof(IF_MAC(interface)));
    memcpy(IF_MAC(interface), (char *)&hash_code_val, sizeof(unsigned int));
}


extern void
rt_table_add_direct_route(rt_table_t *rt_table, char *ip_addr, char mask);

bool_t
node_set_loopback_address(node_t *node, char *ip_addr)
{

    assert(ip_addr);
    
    node->node_nw_prop.is_lp_addr_config = TRUE;
    strncpy(NODE_LO_ADD(node), ip_addr, 16);
    NODE_LO_ADD(node)[15] = '\0';
    
    rt_table_add_direct_route(NODE_RT_TABLE(node),
                              ip_addr, 32);
    return TRUE;
}

bool_t
node_set_intf_ip_address(node_t *node,
                                char *local_if,
                                char *ip_addr,
                                char mask)
{
    interface_t *interface = get_node_if_by_name(node,
                                            local_if);
    if(!interface) assert(0);
    
    interface->intf_nw_props.is_ipaddr_config = TRUE;
    
    strncpy(IF_IP(interface), ip_addr, 16);
    IF_IP(interface)[15] = '\0';
    
    interface->intf_nw_props.mask = mask;
    rt_table_add_direct_route(NODE_RT_TABLE(node),
                              ip_addr, mask);
    return TRUE;
}

bool_t
node_unset_intf_ip_address(node_t *node,
                                  char *local_if)
{
    interface_t *interface = get_node_if_by_name(node,
                                            local_if);
    memset(IF_MAC(interface), 0, sizeof(IF_MAC(interface)));
    interface->intf_nw_props.is_ipaddr_config = FALSE;
    memset(IF_IP(interface), 0, 16);
    interface->intf_nw_props.mask = 0;
    return TRUE;
}

interface_t *
node_get_matching_subnet_interface(node_t *node, char *ip_addr)
{
    interface_t *interface = NULL;
    char mask;
    char *intf_ip, intf_subnet[16], subnet2[16];
    
    for(int i=0; i < MAX_INTF_PER_NODE; ++i)
    {
        interface = (node->interfaces)[i];
        
        if(!interface) return NULL;
        
        if(interface->intf_nw_props.is_ipaddr_config == FALSE)
            continue;
        
        mask = interface->intf_nw_props.mask;
        
        intf_ip = IF_IP(interface);
        
        memset(intf_subnet, 0, 16);
        memset(subnet2, 0, 16);
        
        apply_mask(intf_ip, mask, intf_subnet);
        apply_mask(ip_addr, mask, subnet2);
        
        if(strncmp(intf_subnet, subnet2, 16) == 0)
            return interface;
    }
    return NULL;
}

unsigned int
ip_p_to_n(char *ip_addr)
{
    uint32_t ip = 0;
    inet_pton(AF_INET, ip_addr, &ip);
    ip = htonl(ip);
    return ip;
}

char *
ip_n_to_p(unsigned int ip_addr, char *output_buffer)
{   
    char *out = NULL;
    
    static char str_ip[16];
    
    out = !output_buffer ? str_ip : output_buffer;
    
    memset(out, 0, 16);

    ip_addr = htonl(ip_addr);
    inet_ntop(AF_INET, &ip_addr, out, 16);
    out[15] = '\0';
    
    return out;
}

char *
pkt_buffer_shift_right(char *pkt, unsigned int pkt_size,
                             unsigned int total_buffer_size)
{
    char *temp = NULL;
    
    bool_t need_temp_memory = FALSE;
    
    if(pkt_size * 2 > total_buffer_size)
        need_temp_memory = TRUE;
    
    if(need_temp_memory)
    {
        temp = calloc(1, pkt_size);
        memcpy(temp, pkt, pkt_size);
        memset(pkt, 0, total_buffer_size);
        memcpy(pkt + (total_buffer_size - pkt_size), temp, pkt_size);
        free(temp);
        return pkt + (total_buffer_size - pkt_size);
    }
    
    memcpy(pkt + (total_buffer_size - pkt_size),
           pkt, pkt_size);
    memset(pkt, 0, pkt_size);
    
    return pkt + (total_buffer_size - pkt_size);
}


unsigned int
get_access_intf_operating_vlan_id(interface_t *interface)
{
    if(IF_L2_MODE(interface) != ACCESS)
        assert(0);
    
    return interface->intf_nw_props.vlans[0];
}


bool_t
is_trunk_interface_vlan_enabled(interface_t *interface, unsigned int vlan_id)
{
    if(IF_L2_MODE(interface) != TRUNK)
        assert(0);
    
    for(int i=0; i<MAX_VLAN_MEMBERSHIP; ++i)
    {
        if(interface->intf_nw_props.vlans[i] == vlan_id)
            return TRUE;
    }
    return FALSE;
}


void
print_mac(char *heading, unsigned char *mac_address)
{
    printf("%s = %u:%u:%u:%u:%u:%u",
           heading,
           mac_address[0], mac_address[1],
           mac_address[2], mac_address[3],
           mac_address[4], mac_address[5]);
}

void
dump_interface(interface_t *interface)
{
    node_t *nbr_node = get_nbr_node(interface);
    printf("  Interface Name: %s\n\tNbr Node: %s, Local Node: %s, cost = %d",
           interface->if_name,
           nbr_node->name,
           interface->att_node->name,
           interface->link->cost);
   
    if(interface->intf_nw_props.is_ipaddr_config) {
        printf("  IP addr : %s/%d",
               IF_IP(interface),
               interface->intf_nw_props.mask);
        print_mac("  MAC", IF_MAC(interface));
        printf("\n");
    }
    else
    {
        printf("\t l2 mode = %s", intf_l2_mode_str(IF_L2_MODE(interface)));
        printf("\t Vlan membership : ");
        for(int i=0; i<MAX_VLAN_MEMBERSHIP; ++i)
        {
            if(interface->intf_nw_props.vlans[i]) {
                printf("%u ", interface->intf_nw_props.vlans[i]);
            }
        }
        
        printf("\n");
    }
}

void dump_node(node_t *node)
{
    interface_t *interface = NULL;
    printf("\nNode Name: %s\t UDP Port: %u\n",
           node->name,
           node->udp_port_number);
    
    if(node->node_nw_prop.is_lp_addr_config)
        printf("\t\tLo addr: %s/32\n", node->node_nw_prop.lp_addr.ip_addr);
    for(int i=0; i < MAX_INTF_PER_NODE; ++i)
    {
        interface = node->interfaces[i];
        if(interface)
            dump_interface(interface);
        else
            break;
    }
}

void
dump_nw_graph(graph_t *graph, node_t *node1)
{
    glthread_t *curr = NULL;
    node_t *node = NULL;
    interface_t *interface;
    int i;
    
    printf("Topology Name: %s\n\n", graph->topology_name);
    
    if(!node1)
    {
        ITERATE_GLTHREAD_BEGIN(&graph->node_list, curr)
        {
            node = graph_glue_to_node(curr);
            dump_node(node);
        } ITERATE_GLTHREAD_END(&graph->node_list, curr);
    }
    else
    {
        dump_node(node1);
    }
}

void
dump_interface_stats(interface_t *interface)
{
    printf("%s    :: PktTx : %u, PktRx : %u",
           interface->if_name,
           interface->intf_nw_props.pkt_sent,
           interface->intf_nw_props.pkt_recv);
}

void
dump_node_interface_stats(node_t *node)
{
    interface_t *intf;
    
    int i = 0;
    
    for(; i < MAX_INTF_PER_NODE; ++i)
    {
        intf = node->interfaces[i];
        if(!intf) return;
        
        dump_interface_stats(intf);
        printf("\n");
    }
}
