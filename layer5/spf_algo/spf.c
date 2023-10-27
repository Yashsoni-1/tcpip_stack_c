#include <stdio.h>
#include "../../tcp_public.h"
#include <stdint.h>

extern graph_t *topo;

#define INFINITE_METRIC 0xFFFFFFFF

#define SPF_METRIC(nodeptr) (nodeptr->spf_data->spf_metric)


typedef struct spf_data_ 
{
	node_t *node;
	glthread_t spf_result_head;

	uint32_t spf_metric;
	glthread_t priority_thread_glue;
	nexthop_t *nexthops[MAX_NXT_HOPS];
} spf_data_t;
GLTHREAD_TO_STRUCT(priority_thread_glue_to_spf_data, spf_data_t, priority_thread_glue);

#define spf_data_offset_from_priority_thread_glue \
	((size_t)&(((spf_data_t *)0)->priority_thread_glue)) 

typedef struct spf_result_
{
	node_t *node;
	uint32_t spf_metric;
	nexthop_t *nexthops[MAX_NXT_HOPS];
	glthread_t spf_res_glue;
} spf_result_t;
GLTHREAD_TO_STRUCT(spf_res_glue_to_spf_result, spf_result_t, spf_res_glue);

void spf_flush_nexthops(nexthop_t **nexthop)
{
	int i = 0;

	if(!nexthop) return;

	for(; i<MAX_NXT_HOPS; ++i) {
		if(nexthop[i]) {
			assert(nexthop[i]->ref_count);
			nexthop[i]->ref_count--;
			if(nexthop[i]->ref_count == 0) {
				free(nexthop[i]);
			}

			nexthop[i] = NULL;
			
		}
	}
}

static inline void 
free_spf_result(spf_result_t *spf_result) 
{
	spf_flush_nexthops(spf_result->nexthops);
	remove_glthread(&spf_result->spf_res_glue);
	free(spf_result);
}

static void
init_node_spf_data(node_t *node, bool_t delete_spf_result)
{
	if(!node->spf_data) {
		node->spf_data = calloc(1, sizeof(spf_data_t));
		init_glthread(&node->spf_data->spf_result_head);
		node->spf_data->node = node;
	}
	else if(delete_spf_result) {
		glthread_t *curr;
		ITERATE_GLTHREAD_BEGIN(&node->spf_data->spf_result_head, curr)
		{
			spf_result_t *res = spf_res_glue_to_spf_result(curr);
			free_spf_result(res);
		}ITERATE_GLTHREAD_END(&node->spf_data->spf_result_head, curr);
		init_glthread(&node->spf_data->spf_result_head);
	}

	SPF_METRIC(node) = INFINITE_METRIC;
	remove_glthread(&node->spf_data->priority_thread_glue);
	spf_flush_nexthops(node->spf_data->nexthops);
}

static nexthop_t *
create_new_nexthop(interface_t *oif)
{
	nexthop_t *nexthop = calloc(1, sizeof(nexthop_t));
	nexthop->oif = oif;
	interface_t *other_intf = &oif->link->intf1 == oif ? &oif->link->intf2 : &oif->link->intf1;
	if(!other_intf) {
		free(nexthop);
		return NULL;
	}

	strncpy(nexthop->gw_ip, IF_IP(other_intf), 16);
	nexthop->ref_count = 0;
	return nexthop;
}

static bool_t
spf_insert_new_nexthop(nexthop_t **nexthop_array, nexthop_t *nexthop)
{
	int i=0;

	for(; i < MAX_NXT_HOPS; ++i) {
		if(nexthop_array[i]) continue;
		nexthop_array[i] = nexthop;
		nexthop_array[i]->ref_count++;
		return TRUE;
	}

	return FALSE;
}

static bool_t
spf_is_nexthop_exist(nexthop_t **nexthop_array, nexthop_t *nexthop)
{
	int i=0;

	for(; i < MAX_NXT_HOPS; ++i) {
		if(!nexthop_array[i]) continue;
		
		if(nexthop_array[i]->oif == nexthop->oif) return TRUE;
	}

	return FALSE;
}

static int 
spf_union_nexthops_array(nexthop_t **src, nexthop_t **dst)
{
	int i=0, j=0, copied_count=0;

	while(j < MAX_NXT_HOPS && dst[j]) j++;

	if(j == MAX_NXT_HOPS) return 0;

	for(; i < MAX_NXT_HOPS && j < MAX_NXT_HOPS; ++i, ++j) {

		if(src[i] && spf_is_nexthop_exist(dst, src[i]) == FALSE) {
			dst[j] = src[i];
			dst[j]->ref_count++;
			copied_count++;
		}
	}

	return copied_count;
}

static int
spf_comparison_fn(void *data1, void *data2)
{
	spf_data_t *spf_data_1 = (spf_data_t *)data1;
	spf_data_t *spf_data_2 = (spf_data_t *)data2;

	if(spf_data_1->spf_metric < spf_data_2->spf_metric) 
		return -1;
	else if(spf_data_1->spf_metric > spf_data_2->spf_metric) 
		return 1;

	return 0;
}

static spf_result_t *
spf_lookup_spf_result_by_node(node_t *spf_root, node_t *node)
{
	glthread_t *curr;
	spf_result_t *spf_result;
	spf_data_t *curr_spf_data;

	ITERATE_GLTHREAD_BEGIN(&spf_root->spf_data->spf_result_head, curr) {
		spf_result = spf_res_glue_to_spf_result(curr);
		if(spf_result->node == node) {
			return spf_result;
		}
	} ITERATE_GLTHREAD_END(&spf_root->spf_data->spf_result_head, curr) 

	return NULL;
}

static int
spf_install_routes(node_t *spf_root)
{
	rt_table_t *rt_table = NODE_RT_TABLE(spf_root);

	clear_rt_table(rt_table);

	int i = 0;
	int count = 0;
	glthread_t *curr;
	spf_result_t *spf_result;
	nexthop_t *nexthop = NULL;

	ITERATE_GLTHREAD_BEGIN(&spf_root->spf_data->spf_result_head, curr)
	{
		spf_result = spf_res_glue_to_spf_result(curr);
		for(int i=0; i < MAX_NXT_HOPS; ++i) {
			nexthop = spf_result->nexthops[i];
			if(!nexthop) continue;
			rt_table_add_route(rt_table, NODE_LO_ADD(spf_result->node), 32, nexthop->gw_ip, nexthop->oif, spf_result->spf_metric);
			++count;
		}
	} ITERATE_GLTHREAD_END(&spf_root->spf_data->spf_result_head, curr);

	return count;
}

void initialize_direct_nbrs(node_t *spf_root)
{
	node_t *nbr = NULL;
	char *next_hop_ip = NULL;
	interface_t *oif = NULL;
	nexthop_t *nexthop = NULL;

	ITERATE_NODE_NBRS_BEGIN(spf_root, nbr, oif, next_hop_ip) 
	{
		if(!is_interface_l3_bidirectional(oif)) continue;

		if(get_link_cost(oif) < SPF_METRIC(nbr)) {
			spf_flush_nexthops(nbr->spf_data->nexthops);
			nexthop = create_new_nexthop(oif);
			spf_insert_new_nexthop(nbr->spf_data->nexthops, nexthop);
			SPF_METRIC(nbr) = get_link_cost(oif);
		}

		else if(get_link_cost(oif) == SPF_METRIC(nbr)) {
			nexthop = create_new_nexthop(oif);
			spf_insert_new_nexthop(nbr->spf_data->nexthops, nexthop);
		}
	} ITERATE_NODE_NBRS_END(spf_root, nbr, oif, nxt_hop_ip) ;
}

static void 
spf_record_result(node_t *spf_root, node_t *processed_node)
{
	assert(!spf_lookup_spf_result_by_node(spf_root, processed_node));
	spf_result_t *spf_result = calloc(1, sizeof(spf_result_t));
	
	spf_result->node = processed_node;
	spf_result->spf_metric = processed_node->spf_data->spf_metric;
	spf_union_nexthops_array(processed_node->spf_data->nexthops, spf_result->nexthops);

	init_glthread(&spf_result->spf_res_glue);
	glthread_add_next(&spf_root->spf_data->spf_result_head, &spf_result->spf_res_glue);
}

static void 
spf_explore_nbrs(node_t *spf_root,
				node_t *curr_node,
					glthread_t *priority_lst)
{
	node_t *nbr;
	interface_t *oif;
	char *nxt_hop_ip = NULL;

	ITERATE_NODE_NBRS_BEGIN(curr_node, nbr, oif, nxt_hop_ip) 
	{
		if(!is_interface_l3_bidirectional(oif)) continue;

		if(SPF_METRIC(curr_node) + get_link_cost(oif) < SPF_METRIC(nbr)) {
			spf_flush_nexthops(nbr->spf_data->nexthops);
			spf_union_nexthops_array(curr_node->spf_data->nexthops, nbr->spf_data->nexthops);
			SPF_METRIC(nbr) = SPF_METRIC(curr_node) + get_link_cost(oif);

			if(!IS_GLTHREAD_LIST_EMPTY(&nbr->spf_data->priority_thread_glue)) {
				remove_glthread(&nbr->spf_data->priority_thread_glue);
			}

			glthread_priority_insert(priority_lst, 
				&nbr->spf_data->priority_thread_glue,
				spf_comparison_fn,
				spf_data_offset_from_priority_thread_glue);
		}
		else if(SPF_METRIC(curr_node) + get_link_cost(oif) == SPF_METRIC(nbr)) {
			spf_union_nexthops_arrays(curr_node->spf_data->nexthops, nbr->spf_data->nexthops);
		}
	} ITERATE_NODE_NBRS_END(curr_node, nbr, oif, nxt_hop_ip);

	spf_flush_nexthops(curr_node->spf_data->nexthops);
}


void compute_spf(node_t *spf_root)
{
	node_t *node, *nbr;
	glthread_t *curr;
	interface_t *oif;
	char *nxt_hop_ip = NULL;
	spf_data_t *curr_spf_data;

	init_node_spf_data(spf_root, TRUE);
	SPF_METRIC(spf_root) = 0;

	ITERATE_GLTHREAD_BEGIN(&topo->node_list, curr)
	{
		node = graph_glue_to_node(curr);
		if(node = spf_root) continue;
		init_node_spf_data(node, FALSE);
	} ITERATE_GLTHREAD_END(&topo->node_list, curr);

	initialize_direct_nbrs(spf_root);

	glthread_t priority_lst;
	init_glthread(&priority_lst);

	glthread_priority_insert(&priority_lst, 
		&spf_root->spf_data->priority_thread_glue,
		spf_comparison_fn,
		spf_data_offset_from_priority_thread_glue);

	while(!IS_GLTHREAD_LIST_EMPTY(&priority_lst)) {
		curr = dequeue_glthread_first(&priority_lst);
		curr_spf_data = priority_thread_glue_to_spf_data(curr);

		if(curr_spf_data->node == spf_root) {

			ITERATE_NODE_NBRS_BEGIN(curr_spf_data->node, nbr, oif, nxt_hop_ip) {

				if(!is_interface_l3_bidirectional(oif)) continue;

				if(IS_GLTHREAD_LIST_EMPTY(&nbr->spf_data->priority_thread_glue)) {
					glthread_priority_insert(&priority_lst,
						&nbr->spf_data->priority_thread_glue,
						spf_comparison_fn,
						spf_data_offset_from_priority_thread_glue);
				}
			} ITERATE_NODE_NBRS_END(curr_spf_data->node, nbr, oif, nxt_hop_ip);
		}

		spf_record_result(spf_root, curr_spf_data->node);
		
		spf_explore_nbrs(spf_root, curr_spf_data->node, &priority_lst);
	}
	
	int count = spf_install_routes(spf_root);
}

static void show_spf_results(node_t *node)
{
	int i=0, j=0;
	glthread_t *curr;
	interface_t *oif = NULL;
	spf_result_t *res = NULL;

	printf("\nSPF run results for node = %s\n", node->name);

	ITERATE_GLTHREAD_BEGIN(&node->spf_data->spf_result_head, curr) {

		res = spf_res_glue_to_spf_result(curr);
		printf("DEST : %-10s spf_metric : %-6u", res->node->name, res->spf_metric);

		printf(" Nxt Hop : ");
		j=0;

		for(i=0; i < MAX_NXT_HOPS; ++i, ++j) {
			if(!res->nexthops[i]) continue;

			oif = res->nexthops[i]->oif;

			if(j == 0) {
				printf("%-8s      OIF : %-7s    gateway : %-16s ref_count = %u\n",
					nexthop_node_name(res->nexthops[i]),
					oif->if_name, res->nexthops[i]->gw_ip,
					res->nexthops[i]->ref_count);
			}
			else {
				printf("                                          : "
					"%-8s      OIF : %-7s    gateway : %-16s ref_count = %u\n",
					nexthop_node_name(res->nexthops[i]),
					oif->if_name, res->nexthops[i]->gw_ip,
					res->nexthops[i]->ref_count);
			}
		}
	} ITERATE_GLTHREAD_END(&node->spf_data->spf_result_head, curr);
}

static void
compute_spf_all_routers(graph_t *topo) 
{
	glthread_t *curr;
	ITERATE_GLTHREAD_BEGIN(&topo->node_list, curr) {
		node_t *node = graph_glue_to_node(curr);
		compute_spf(node);
	} ITERATE_GLTHREAD_END(&topo->node_list, curr);
}

int 
spf_algo_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
	node_t *node;
	char *node_name;
	
	tlv_struct_t *tlv = NULL;
	int CMDCODE = -1;

	CMDCODE = EXTRACT_CMD_CODE(tlv_buf);
	
	TLV_LOOP_BEGIN(tlv_buf, tlv) 
	{
		if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0)
			node_name = tlv->value;
		
	}TLV_LOOP_END;

	node = get_node_by_node_name(topo, node_name);

	switch(CMDCODE) {
		
		case CMDCODE_SHOW_SPF_RESULTS:
			show_spf_results(node);
			break;
		case CMDCODE_RUN_SPF:
			compute_spf(node);
			break;
		
		default:
			;
	}
	
	return 0;
}

