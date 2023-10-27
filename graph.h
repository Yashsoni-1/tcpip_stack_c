#ifndef graph_h
#define graph_h

#include <assert.h>
#include "gl_thread/gl_thread.h"
#include "net.h"
#include <stdlib.h>


#define NODE_NAME_SIZE 16
#define IF_NAME_SIZE 16
#define MAX_INTF_PER_NODE 10


#define TOPO_NAME_SIZE 32
#define OFFSET_OF_FIELD(struct, field) \
    (uintptr_t)&(((struct*)0)->field)

typedef struct node_ node_t;
typedef struct link_ link_t;

typedef struct interface_
{
    char if_name[IF_NAME_SIZE];
    struct node_ *att_node;
    struct link_ *link;
    intf_nw_props_t intf_nw_props;
} interface_t;

struct link_
{
    interface_t intf1;
    interface_t intf2;
    unsigned int cost;
} ;

static inline uint32_t
get_link_cost(interface_t *interface)
{
    return interface->link->cost;
}

typedef struct spf_data_ spf_data_t; 

struct node_
{
    char name[NODE_NAME_SIZE];
    interface_t *interfaces[MAX_INTF_PER_NODE];
    glthread_t graph_glue;
    unsigned int udp_port_number;
    int udp_sock_fd;
    node_nw_prop_t node_nw_prop;

    spf_data_t *spf_data;
} ;

GLTHREAD_TO_STRUCT(graph_glue_to_node, node_t, graph_glue);

typedef struct graph_
{
    char topology_name[TOPO_NAME_SIZE];
    glthread_t node_list;
} graph_t;


graph_t *
create_new_graph(char *topology_name);

node_t *
create_graph_node(graph_t *graph,
                  char *node_name);

void
insert_link_bw_two_nodes(node_t *node1,
                        node_t *node2,
                        char *from_if_name,
                        char *to_if_name,
                        unsigned int cost);

static inline node_t *
get_nbr_node(interface_t *interface)
{
    assert(interface->att_node);
    assert(interface->link);
    
    
    link_t *link = interface->link;
    
    if(&link->intf1 != interface) {
        return (link->intf1).att_node;
    } else
        return (link->intf2).att_node;
}

static inline int
get_node_intf_available_slot(node_t *node)
{
    int i = 0;
    for(; i<MAX_INTF_PER_NODE; ++i) {
        if((node->interfaces)[i])
            continue;
        return i;
    }
    return -1;
}


static inline interface_t *
get_node_if_by_name(node_t *node, char *if_name)
{
    
    interface_t *intf;
    
    for(int i = 0; i < MAX_INTF_PER_NODE; ++i)
    {
        intf = node->interfaces[i];
        
        if(!intf) return NULL;
        
        if(strncmp(intf->if_name, if_name, IF_NAME_SIZE) == 0)
            return intf;
    }
    return NULL;
}

static inline node_t *
get_node_by_node_name(graph_t *topo, char *node_name)
{
    glthread_t *curr = NULL;
    node_t *node = NULL;
    
    ITERATE_GLTHREAD_BEGIN(&(topo->node_list), curr)
    {
        node = graph_glue_to_node(curr);
        if(strncmp(node->name, node_name, strlen(node_name)) == 0)
            return node;
    } ITERATE_GLTHREAD_END(&(topo->node_list), curr);
    
    return NULL;
}


#endif /* graph_h */
