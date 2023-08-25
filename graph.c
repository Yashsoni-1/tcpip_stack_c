#include "graph.h"
#include <stdlib.h>
#include <string.h>


extern void
init_udp_socket(node_t *node);

void
insert_link_bw_two_nodes(node_t *node1,
                              node_t *node2,
                              char *from_if_name,
                              char *to_if_name,
                              unsigned int cost)
{
    link_t *link = calloc(1, sizeof(link_t));
    strncpy(link->intf1.if_name, from_if_name,
            IF_NAME_SIZE);
    link->intf1.if_name[IF_NAME_SIZE - 1] = '\0';
    strncpy(link->intf2.if_name, to_if_name,
            IF_NAME_SIZE);
    link->intf2.if_name[IF_NAME_SIZE - 1] = '\0';
    
    link->intf1.link = link;
    link->intf2.link = link;
    
    link->intf1.att_node = node1;
    link->intf2.att_node = node2;
    
    link->cost = cost;
    
    int empty_intf_slot;
    
    empty_intf_slot = get_node_intf_available_slot(node1);
    node1->interfaces[empty_intf_slot] = &link->intf1;
    
    empty_intf_slot = get_node_intf_available_slot(node2);
    node2->interfaces[empty_intf_slot] = &link->intf2;
    
    init_intf_nw_prop(&link->intf1.intf_nw_props);
    init_intf_nw_prop(&link->intf2.intf_nw_props);
    
    interface_assign_mac_address(&link->intf1);
    interface_assign_mac_address(&link->intf2);
}

graph_t *
create_new_graph(char *topology_name)
{
    graph_t *topo = calloc(1, sizeof(graph_t));
    strncpy(topo->topology_name, topology_name,
            TOPO_NAME_SIZE);
    topo->topology_name[TOPO_NAME_SIZE - 1] = '\0';
    init_glthread(&topo->node_list);
    return topo;
}

node_t *
create_graph_node(graph_t *graph,
                          char *node_name)
{
    node_t *node = calloc(1, sizeof(node_t));
    strncpy(node->name, node_name, NODE_NAME_SIZE);
    node->name[NODE_NAME_SIZE - 1] = '\0';
    init_udp_socket(node);
    init_glthread(&node->graph_glue);
    init_node_nw_prop(&node->node_nw_prop);
    glthread_add_next(&graph->node_list,
                      &node->graph_glue);
    return node;
}




