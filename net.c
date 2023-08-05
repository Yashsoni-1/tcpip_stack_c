//
//  net.c
//  tcp_ip_stack_c
//
//  Created by Yash soni on 03/08/23.
//

#include "net.h"
#include <assert.h>
#include "graph.h"

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
    hash_code_val = hash_code(node->name,
                              NODE_NAME_SIZE);
    hash_code_val *= hash_code(interface->if_name,
                               IF_NAME_SIZE);
    memset(IF_MAC(interface), 0, sizeof(IF_MAC(interface)));
    memcpy(IF_MAC(interface), (char *)&hash_code_val,
           sizeof(unsigned int));
}

void print_mac(char *mac_address)
{
    printf("%s", mac_address);
}

void dump_node(node_t *node)
{
    printf("\nNode Name: %s\n", node->name);
    printf("\tlo addr: %s/%d\n", node->node_nw_prop.lp_addr.ip_addr,
           LO_MASK);
    for(int i=0; i < MAX_INTF_PER_NODE; ++i)
    {
        interface_t *interface = node->interfaces[i];
        if(interface)
        {
            printf("  Interface Name: %s\n", interface->if_name);
            printf("\tNbr Node: %s, Local Node: %s, cost = %d",
                   get_nbr_node(interface)->name,
                   interface->att_node->name, interface->link->cost);
            printf("\tIP addr : %s/%d MAC : ",
                   interface->intf_nw_props.ip_add.ip_addr, interface->intf_nw_props.mask);
            print_mac(interface->intf_nw_props.mac_add.mac);
            printf("\n");
        }
        
    }
}

void dump_nw_graph(graph_t *graph)
{
    printf("Topology Name: %s\n\n", graph->topology_name);
    glthread_t *glthread = NULL;
    node_t *node = NULL;
    
    ITERATE_GLTHREAD_BEGIN(&graph->node_list, glthread) {
        node = graph_glue_to_node(glthread);
        dump_node(node);
    }ITERATE_GLTHREAD_END(&graph->node_list, glthread);
}

bool_t node_set_loopback_address(node_t *node,
                                 char *ip_addr)
{
    node->node_nw_prop.is_lp_addr_config = TRUE;
    strncpy(NODE_LO_ADD(node), ip_addr, 16);
    NODE_LO_ADD(node)[15] = '\0';
    return TRUE;
}

bool_t node_set_intf_ip_address(node_t *node,
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
    return TRUE;
}

bool_t node_unset_intf_ip_address(node_t *node,
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


