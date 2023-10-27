#include "graph.h"
#include "layer3.h"
#include <arpa/inet.h>
#include <assert.h>
#include "comm.h"
#include "../Layer2/layer2.h"

extern void
demote_pkt_to_layer2(node_t *node,
                     unsigned int next_hop_ip,
                     char *oif,
                     char *pkt,
                     unsigned int pkt_size,
                     int protocol_number);

extern void 
spf_flush_nexthops(nexthop_t *nexthop);

static bool_t
l3_is_direct_route(l3_route_t *l3_route)
{
    return l3_route->is_direct;
}


nexthop_t *
l3_route_get_active_nexthop(l3_route_t *l3_route)
{
	if(l3_is_direct_route(l3_route)) return NULL;

	nexthop_t *nexthop = l3_route->nexthops[l3_route->nxthop_idx];
	assert(nexthop);

	l3_route->nxthop_idx++;

	if(l3_route->nxthop_idx == MAX_NXT_HOPS ||
		!l3_route->nexthops[l3_route->nxthop_idx]) {
		l3_route->nxthop_idx = 0;	
	}

	return nexthop;
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

static void
l3_route_free(l3_route_t *l3_route)
{
	assert(IS_GLTHREAD_LIST_EMPTY(&l3_route->rt_glue));
	spf_flush_nexthops(l3_route->nexthops);
	free(l3_route);
}

void 
clear_rt_table(rt_table_t *rt_table)
{
	glthread_t *curr;
	l3_route_t *l3_route;

	ITERATE_GLTHREAD_BEGIN(&rt_table->route_list, curr) 
	{
		l3_route = rt_glue_to_l3_route(curr);
		if(l3_is_direct_route(l3_route))
			continue;

		remove_glthread(curr);
		l3_route_free(l3_route);
	} ITERATE_GLTHREAD_END(&rt_table->route_list, curr);
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
    init_glthread(&l3_route->rt_glue);
    glthread_add_next(&rt_table->route_list, &l3_route->rt_glue);
    return TRUE;
}

void
rt_table_add_direct_route(rt_table_t *rt_table,
                          char *dst, char mask)
{
   rt_table_add_route(rt_table, dst, mask, 0, 0, 0);
}

void
rt_table_add_route(rt_table_t *rt_table,
                   char *dst, char mask,
                   char *gw, interface_t *oif,
                    uint32_t spf_metric)
{
    uint32_t dst_int;
    char dst_str_with_mask[16];
    bool_t new_route = FALSE;
	
    apply_mask(dst, mask, dst_str_with_mask);
    
    dst_int = ip_p_to_n(dst_str_with_mask);
    
    l3_route_t *l3_route = l3rib_lookup_lpm(rt_table, dst_int);
    
    if(!l3_route) {
    
	    l3_route = calloc(1, sizeof(l3_route_t));
	    strncpy(l3_route->dest, dst_str_with_mask, 16);
	    l3_route->dest[15] = '\0';
	    l3_route->mask = mask;
	    new_route = TRUE;
	    l3_route->is_direct = TRUE;
	}

	int i = 0;
	
	if(!new_route) {
		for(; i < MAX_NXT_HOPS; ++i) {
			if(l3_route->nexthops[i]) {
				if(strncmp(l3_route->nexthops[i]->gw_ip, gw, 16) == 0 &&
					l3_route->nexthops[i]->oif == oif) {
					printf("Error : Attempt to Add Duplicate Route\n");
					return;
				}
			}
			else break;
		}
	}

	if(i == MAX_NXT_HOPS) {
		printf("Error: No Nexthop space left for route %s/%u\n", dst_str_with_mask, mask);
		return;
	}
	
    if(gw && oif)
    {
		nexthop_t *nexthop = calloc(1, sizeof(nexthop_t));
		l3_route->nexthops[i] = nexthop;
		l3_route->is_direct = FALSE;
		l3_route->spf_metric = spf_metric;
		nexthop->ref_count++;
        strncpy(nexthop->gw_ip, gw, 16);
        nexthop->gw_ip[15] = '\0';
        nexthop->oif = oif;
    }

	if(new_route) {
	    if(!_rt_table_entry_add(rt_table, l3_route))
	    {
	        printf("Error : Route %s/%d Installation failed\n",
	               dst_str_with_mask, mask);
	        l3_route_free(l3_route);
	    }
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




void
demote_packet_to_layer3(node_t *node,
                        char *pkt, unsigned int size,
                        int protocol_number,
                        unsigned int dst_ip_addr)
{
	ip_hdr_t iphdr;
	initialize_ip_hdr(&iphdr);

	iphdr.protocol = protocol_number;
	uint32_t addr_int = 0;
	addr_int = ip_p_to_n(NODE_LO_ADD(node));
	iphdr.src_ip = addr_int;
	iphdr.dst_ip = dst_ip_addr;

	iphdr.total_length = IP_HDR_COMPUTE_DEFAULT_TOTAL_LEN(size);
	
	char *new_pkt = NULL;
	uint32_t new_pkt_size = 0;
	
	new_pkt_size = IP_HDR_TOTAL_LEN_IN_BYTES((&iphdr));
	new_pkt = calloc(1, MAX_PACKET_BUFFER_SIZE);

	memcpy(new_pkt, (char *)&iphdr, IP_HDR_LEN_IN_BYTES((&iphdr)));

	if(pkt && size)
		memcpy(new_pkt + IP_HDR_LEN_IN_BYTES((&iphdr)), pkt, size);

	l3_route_t *l3_route = l3rib_lookup_lpm(NODE_RT_TABLE(node), iphdr.dst_ip);

	if(!l3_route) {
		printf("Node : %s : No L3 route\n", node->name);
		free(new_pkt);
		return;
	}

	bool_t is_direct_route = l3_is_direct_route(l3_route);

	char *shifted_pkt_buffer = pkt_buffer_shift_right(new_pkt, new_pkt_size, MAX_PACKET_BUFFER_SIZE);

	if(is_direct_route) {
		demote_pkt_to_layer2(node, dst_ip_addr, 
			0,
			shifted_pkt_buffer, new_pkt_size, 
			ETH_IP);

		return;
	}

	uint32_t next_hop_ip;
	nexthop_t *nexthop = NULL;

	nexthop = l3_route_get_active_nexthop(l3_route);

	if(!nexthop) {
		free(new_pkt);
		return;
	}

	next_hop_ip = ip_p_to_n(nexthop->gw_ip);

	demote_pkt_to_layer2(node, 
		next_hop_ip, 
		nexthop->oif->if_name, 
		shifted_pkt_buffer, 
		new_pkt_size, 
		ETH_IP);

	free(new_pkt);
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
	nexthop_t *nexthop = NULL;
	nexthop = l3_route_get_active_nexthop(l3_route);
	assert(nexthop);
	
    next_hop_ip = ip_p_to_n(nexthop->gw_ip);
    
    demote_pkt_to_layer2(node, next_hop_ip,
                         nexthop->oif->if_name, (char *)ip_hdr,
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
	int i=0, count=0;
    l3_route_t *l3_route = NULL;
    glthread_t *curr = NULL;
    
    printf("L3 Routing Table:\n");
    
    ITERATE_GLTHREAD_BEGIN(&rt_table->route_list, curr)
    {
        l3_route = rt_glue_to_l3_route(curr);
		count++;

		if(l3_route->is_direct) 
		{
			if(count != 1) 
			{
				printf("\t|===================|=======|====================|==============|==========|\n");
			}
			else 
			{
				printf("\t|======= IP ========|== M ==|======== GW ========|===== Oif ====|== Cost ==|\n");	
			}
			 printf("\t|%-18s |  %-4d | %-18s | %-12s |          |\n",
               l3_route->dest, l3_route->mask, "NA", "NA");

			continue;
		}

		for(int i=0; i< MAX_NXT_HOPS; ++i) 
		{
			if(l3_route->nexthops[i])
			{
				if(i == 0)
				{
					if(count != 1)
					{
						printf("\t|===================|=======|====================|==============|==========|\n");	
					}
					else 
					{
						printf("\t|======= IP ========|== M ==|======== GW ========|===== Oif ====|== Cost ==|\n");	
					}

					printf("\t|%-18s |  %-4d | %-18s | %-12s |  %-4u    |\n",
               			l3_route->dest,
					 	l3_route->mask,
					 	l3_route->nexhops[i]->gw_ip,
					 	l3_route->nexhops[i]->oif->if_name,
					 	l3_route->spf_metric);
				}
				else 
				{
					printf("\t|                   |          | %-18s | %-12s |          |\n",
					 	l3_route->nexhops[i]->gw_ip,
					 	l3_route->nexhops[i]->oif->if_name);
				}
			}
		}
        
    }ITERATE_GLTHREAD_END(&rt_table->route_list, curr);
	printf("\t|===================|=======|====================|==============|==========|\n");	
}
