#include <stdio.h>
#include "graph.h"

graph_t *
build_first_topo(void) {
    
#if 0
                    
                    
                                
                                    +-----------+
                                0/4 |           | 0/0
                +-------------------+   R0_re   +-------------------+
                |       40.1.1.1/24 | 122.1.1.0 | 20.1.1.1/24       |
                |                   +-----------+                   |
                |                                                   |
    40.1.1.2/24 | 0/5                                           0/1 | 20.1.1.2/24
                +-----------+                           +-----------+
                |           | 0/3                   0/2 |           |
                +   R2_re   +---------------------------+   R1_re   +
                | 122.1.1.2 | 30.1.1.2/24   30.1.1.1/24 | 122.1.1.1 |
                +-----------+                           +-----------+
#endif
    
    
    graph_t *graph = create_new_graph("From the Generic Graph");
    node_t *R0_re = create_graph_node(graph, "R0_re");
    node_t *R1_re = create_graph_node(graph, "R1_re");
    node_t *R2_re = create_graph_node(graph, "R2_re");
    
    insert_link_bw_two_nodes(R0_re, R1_re, "eth0/0", "eth0/1", 1);
    insert_link_bw_two_nodes(R1_re, R2_re, "eth0/2", "eth0/3", 1);
    insert_link_bw_two_nodes(R2_re, R0_re, "eth0/5", "eth0/4", 1);
    
    node_set_loopback_address(R0_re, "122.1.1.0");
    node_set_intf_ip_address(R0_re, "eth0/4", "40.1.1.1", 24);
    node_set_intf_ip_address(R0_re, "eth0/0", "20.1.1.1", 24);
    
    node_set_loopback_address(R1_re, "122.1.1.1");
    node_set_intf_ip_address(R1_re, "eth0/1", "20.1.1.2", 24);
    node_set_intf_ip_address(R1_re, "eth0/2", "30.1.1.1", 24);
    
    node_set_loopback_address(R2_re, "122.1.1.2");
    node_set_intf_ip_address(R2_re, "eth0/5", "40.1.1.2", 24);
    node_set_intf_ip_address(R2_re, "eth0/3", "30.1.1.2", 24);
    
    return graph;
}

