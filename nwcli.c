#include "CommandParser/libcli.h"
#include "CommandParser/cmdtlv.h"
#include "cmdcodes.h"
#include "graph.h"
#include "stdio.h"

extern graph_t *topo;

static void 
display_node_interfaces(param_t *param, ser_buff_t *tlv_buf)
{
	node_t *node;
	char *node_name;

	tlv_struct_t *tlv = NULL;
	
	TLV_LOOP_BEGIN(tlv_buf, tlv)
	{
		if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0)
			node_name = tlv->value;

	} TLV_LOOP_END;

	if(!node_name)
		return;

	node = get_node_by_node_name(node_name);

	int i=0;

	interface_t *intf;

	for(; i < MAX_INTF_PER_NODE; ++i)
	{
		intf = node->interfaces[i];
		if(!intf) continue;

		printf(" %s\n", intf->if_name);
	}
}

static int 
intf_config_handler(param_t *param, ser_buff_t *tlv_buf,
		    op_mode enable_or_disable)
{
	node_t *node = NULL;
	char *node_name = NULL;
	char *intf_name = NULL;
	interface_t *interface = NULL;

	char *if_up_down;

	int CMDCODE = -1;

	CMDCODE = EXTRACT_CMD_CODE(tlv_buf);
	
	tlv_struct_t *tlv = NULL;

	TLV_LOOP_BEGIN(tlv_buf, tlv)
	{
		if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0)
			node_name = tlv->value;
		else if(strncmp(tlv->leaf_id, "if-name", strlen("if-name")) == 0)
			intf_name = tlv->value;
		else if(strncmp(tlv->leaf_id, "if-up-down", strlen("if-up-down")) == 0)
			if_up_down = tlv->value;
		else
			assert(0);
		
	} TLV_LOOP_END;

	node = get_node_by_node_name(topo, node_name);

	interface = get_node_if_by_name(node, intf_name);

	if(!interface)
	{
		printf("Error : Interface %s do not exist\n", intf_name);
		return -1;
	}

	uint32_t if_change_flags = 0;

	switch(CMDCODE) {
		case CMDCODE_CONF_INTF_UP_DOWN:
			if(strncmp(if_up_down, "up", strlen("up")) == 0) {
				interface->intf_nw_props.is_up == TRUE;
			} else {
				interface->intf_nw_props.is_up == FALSE;
			}
			break;
		default:
			break;
	}

	return 0;
}


static void 
display_graph_node(param_t *param, ser_buff_t *tlv_buf)
{
	node_t *node;
	glthread_t *curr;

	ITERATE_GLTHREAD_BEGIN(&topo->node_list, curr);
	{
		node = graph_glue_to_node(curr);
		printf("%s\n", node->name);

	} ITERATE_GLTHREAD_END(&topo->node_list, curr);
}

static int 
validate_node_existence(char *node_name)
{
	node_t *node = get_node_by_node_name(topo, node_name);
	if(node)
	{
		return VALIDATION_SUCCESS;
	}

	printf("Error : Node %s do not exists\n", node->name);
	return VALIDATION_FAILED;
}

static int
validate_mask_value(char *mask_str)
{
	unsigned int mask = atoi(mask_str);
	if(!mask) {
		printf("Error : Invalid Mask Value\n");
		return VALIDATION_FAILED;
	}

	if(mask >=0 && mask <= 32) {
		return VALIDATION_SUCCESS;
	}

	return VALIDATION_FAILED;
}

extern void
send_arp_broadcast_request(node_t *node, interface_t *oif, char *ip_addr);


static int 
arp_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
	node_t *node;
	char *node_name;
	char *ip_addr;
	tlv_struct_t *tlv = NULL;

	TLV_LOOP_BEGIN(tlv_buf, tlv) 
	{
		if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0)
			node_name = tlv->value;
		else if(strncmp(tlv->leaf_id, "ip-address", strlen("ip-address")) == 0)
			ip_addr = tlv->value;
	}TLV_LOOP_END;

	node = get_node_by_node_name(topo, node_name);
	send_arp_broadcast_request(node, NULL, ip_addr);
	return 0;
}

typedef struct arp_table_ arp_table_t;

extern void
dump_arp_table(arp_table_t *arp_table);

static int
show_arp_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
	node_t *node = NULL;
	char *node_name;
	tlv_struct_t *tlv = NULL;

	TLV_LOOP_BEGIN(tlv_buf, tlv)
	{
		if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0)
			node_name = tlv->value;
	}TLV_LOOP_END;

	node = get_node_by_node_name(topo, node_name);
	dump_arp_table(NODE_ARP_TABLE(node));

	return 0;
}

typedef struct mac_table_ mac_table_t;

extern void 
dump_mac_table(mac_table_t *mac_table);

static int 
show_mac_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
	node_t *node = NULL;
	char *node_name;
	tlv_struct_t *tlv = NULL;

	TLV_LOOP_BEGIN(tlv_buf, tlv)
	{
		if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0)
			node_name = tlv->value;
	} TLV_LOOP_END;

	node = get_node_by_node_name(topo, node_name);
	dump_mac_table(NODE_MAC_TABLE(node));

	return 0;
}

typedef struct  rt_table_ rt_table_t;

extern void 
dump_rt_table(rt_table_t *rt_table);

static int 
show_rt_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
	node_t *node = NULL;
	char *node_name;
	tlv_struct_t *tlv = NULL;

	TLV_LOOP_BEGIN(tlv_buf, tlv)
	{
		if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0)
			node_name = tlv->value;
	} TLV_LOOP_END;

	node = get_node_by_node_name(topo, node_name);
	dump_rt_table(NODE_RT_TABLE(node));

	return 0;
}

extern void 
delete_rt_table_entry(rt_table_t *rt_table,
		     char *ip_addr, char mask);

extern void
rt_table_add_route(rt_table_t *rt_table, char *dst,
			char mask, char *gw, char *oif);


static int 
l3_config_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
	node_t *node = NULL;
	char *node_name = NULL;
	char *intf_name = NULL;
	char *gwip = NULL;
	char *mask_str = NULL;
	char *dest = NULL;

	int CMDCODE = -1;

	CMDCODE = EXTRACT_CMD_CODE(tlv_buf);
	
	tlv_struct_t *tlv = NULL;

	TLV_LOOP_BEGIN(tlv_buf, tlv)
	{
		if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0)
			node_name = tlv->value;
		else if(strncmp(tlv->leaf_id, "ip-address", strlen("ip-address")) == 0)
			dest = tlv->value;
		else if(strncmp(tlv->leaf_id, "mask", strlen("mask")) == 0)
			mask_str = tlv->value;
		else if(strncmp(tlv->leaf_id, "gw-ip", strlen("gw-ip")) == 0)
			gwip = tlv->value;
		else if(strncmp(tlv->leaf_id, "oif", strlen("oif")) == 0)
			intf_name = tlv->value;
		else
			assert(0);
		
	} TLV_LOOP_END;

	node = get_node_by_node_name(topo, node_name);

	char mask;

	if(mask_str) {
		mask = atoi(mask_str);
	}

	switch(CMDCODE) {
		case CMDCODE_CONF_NODE_L3ROUTE:
			switch(enable_or_disable) {
				case CONFIG_ENABLE:
				{
					interface_t *intf;
					if(intf_name) {
						intf = get_node_if_by_name(node, intf_name);

						if(!intf) {
							printf("Config Error : Non-Existing Interface : %s\n",
								intf_name);
							return -1;
						}
						if(!IS_INTF_L3_MODE(intf)) {
							printf("Config Error : Not L3 Mode Interface : %s\n",
								intf_name);
							return -1;
						}
					}
					rt_table_add_route(NODE_RT_TABLE(node), dest, mask, gwip, intf_name);
				}
				break;
				case CONFIG_DISABLE:
					delete_rt_table_entry(NODE_RT_TABLE(node), dest, mask);	
					break;
				default:
					;
			}
		break;
		default:
			break;
	}

	return 0;
}


static int
show_nw_topology_handler(param_t *param, ser_buff_t *tlv_buf,
		op_mode enable_or_disable) {


	node_t *node = NULL;
	char *node_name = NULL;
	tlv_struct_t  *tlv = NULL;
	int CMDCODE = -1;
	CMDCODE = EXTRACT_CMD_CODE(tlv_buf);

	TLV_LOOP_BEGIN(tlv_buf, tlv) {
		if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0) {
			node_name = tlv->value;
		}else {
			assert(0);
		}
	}TLV_LOOP_END;

	if(node_name) {
		node = get_node_by_node_name(topo, node_name);
	}

	switch(CMDCODE) {
		case CMDCODE_SHOW_NW_TOPOLOGY:
			dump_nw_graph(topo, node);
			break;
		default:
			;

	}
	return 0;
}

extern void
layer5_ping_fn(node_t *node, char *dst_ip_addr);

extern void
layer3_ero_ping_fn(node_t *node, char *ip_addr, char *ero_ip_addr);

static int 
ping_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
	node_t *node = NULL;
	char *node_name;
	char *ip_addr, 
	     *ero_ip_addr;
	tlv_struct_t *tlv = NULL;

	int CMDCODE = -1;
	CMDCODE = EXTRACT_CMD_CODE(tlv_buf);

	TLV_LOOP_BEGIN(tlv_buf, tlv)
	{
		if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0)
			node_name = tlv->value;
		else if(strncmp(tlv->leaf_id, "ip-address", strlen("ip-address")) == 0)
			ip_addr = tlv->value;
		else if(strncmp(tlv->leaf_id, "ero-ip-address", strlen("ero-ip-address")) == 0)
			ero_ip_addr = tlv->value;			
		else 
			assert(0);
	} TLV_LOOP_END;

	node = get_node_by_node_name(topo, node_name);
	switch(CMDCODE) {
		case CMDCODE_PING:
		layer5_ping_fn(node, ip_addr);
			break;
		case CMDCODE_ERO_PING:
		layer3_ero_ping_fn(node, ip_addr, ero_ip_addr);
		default:
			;
	}

	return 0;
}

void nw_init_cli()
{
	init_libcli();

	param_t *show = libcli_get_show_hook();
	param_t *debug = libcli_get_debug_hook();
    param_t *config = libcli_get_config_hook();
	param_t *run = libcli_get_run_hook();
	param_t *debug_show = libcli_get_debug_show_hook();
	param_t *root = libcli_get_root();

	{
		static param_t topology;
		init_param(&topology, CMD, "topology", show_nw_topology_handler,
				0, INVALID, 0, "Dump Complete Network Topology");
		libcli_register_param(show, &topology);
		set_param_cmd_code(&topology, CMDCODE_SHOW_NW_TOPOLOGY);


		{ 
			static param_t node;
			init_param(&node, CMD, "node", 0,
							0, INVALID, 0, "\"node\" keyword");
			libcli_register_param(&topology, &node);
			libcli_register_display_callback(&node, display_graph_node);
	
	
				{
					static param_t node_name;
					init_param(&node_name, LEAF, NULL, show_nw_topology_handler, validate_node_existence, STRING, "node-name", 
							"Node Name");
					libcli_register_param(&node, &node_name);
					set_param_cmd_code(&node_name, CMDCODE_SHOW_NW_TOPOLOGY);
				}
		}
	}


	{
		static param_t node;
		init_param(&node, CMD, "node", 0,
						0, INVALID, 0, "\"node\" keyword");
		libcli_register_param(run, &node);
		libcli_register_display_callback(&node, display_graph_node);


		{
			static param_t node_name;
			init_param(&node_name, LEAF, NULL, NULL, validate_node_existence, STRING, "node-name", 
					"Node Name");
			libcli_register_param(&node, &node_name);

			{
				static param_t ping;
				init_param(&ping, CMD, "ping", 0,
								0, INVALID, 0, "PING Utility");
				libcli_register_param(&node_name, &ping);
				
				{
					static param_t ip_addr;
					init_param(&ip_addr, LEAF, 0, ping_handler,
									0, IPV4,
						"ip-address",
						"IPv4 Address");
					libcli_register_param(&ping, &ip_addr);
					set_param_cmd_code(&ip_addr, CMDCODE_PING); 
					
					{
						static param_t ero;
						init_param(&ero, CMD, "ero", 0,
										0, INVALID, 0, "ERO(Explicit Route Object)");
						libcli_register_param(&ip_addr, &ero);
						
						{
							static param_t ero_ip_addr;
							init_param(&ero_ip_addr, LEAF, 0, ping_handler,
											0, IPV4,
								"ero-ip-address",
								"ERO IPv4 Address");
							libcli_register_param(&ero, &ero_ip_addr);
							set_param_cmd_code(&ero_ip_addr, CMDCODE_ERO_PING);
						}
					}
				}
			}


			{
				static param_t resolve_arp;
				init_param(&resolve_arp, CMD, "resolve-arp", 0,
								0, INVALID, 0, "Resolve ARP");
				libcli_register_param(&node_name, &resolve_arp);
                

				{
					static param_t ip_addr;
					init_param(&ip_addr, LEAF, 0, arp_handler,
									0, IPV4,
						"ip-address",
						"Nbr IPv4 Address");
					libcli_register_param(&resolve_arp, &ip_addr);
					set_param_cmd_code(&ip_addr, CMDCODE_RUN_ARP);

				}
			}
		}
    }

	{
		static param_t node;
		init_param(&node, CMD, "node", 0,
						0, INVALID, 0, "\"node\" keyword");
		libcli_register_param(show, &node);
		libcli_register_display_callback(&node, display_graph_node);


		{
			static param_t node_name;
			init_param(&node_name, LEAF, NULL, NULL, validate_node_existence, STRING, "node-name", 
					"Node Name");
			libcli_register_param(&node, &node_name);

			{
				static param_t arp;
                		init_param(&arp, CMD, "arp", show_arp_handler,
                                		0, INVALID, 0, "Dump ARP Table");
                		libcli_register_param(&node_name, &arp);
				set_param_cmd_code(&arp, CMDCODE_SHOW_NODE_ARP_TABLE);
			}

			{
				static param_t mac;
                		init_param(&mac, CMD, "mac", show_mac_handler,
                                		0, INVALID, 0, "Dump MAC Table");
                		libcli_register_param(&node_name, &mac);
				set_param_cmd_code(&mac, CMDCODE_SHOW_NODE_MAC_TABLE);
			}

			{
				static param_t rt;
                		init_param(&rt, CMD, "rt", show_rt_handler,
                                		0, INVALID, 0, "Dump L3 Routing Table");
                		libcli_register_param(&node_name, &rt);
				set_param_cmd_code(&rt, CMDCODE_SHOW_NODE_RT_TABLE);
			}
		}

	}

	{
		static param_t node;
		init_param(&node, CMD, "node", 0,
			0, INVALID, 0, "\"node\" keyword");
		libcli_register_param(config, &node);
		libcli_register_display_callback(&node, display_graph_node);

		{
			static param_t node_name;
			init_param(&node_name, LEAF, 0, 0, validate_node_existence, STRING, "node-name", 
					"Node Name");
			libcli_register_param(&node, &node_name);
			{
				static param_t interface;
				init_param(&interface, CMD, "interface", 0,
				0, INVALID, 0, "\"Interface\" keyword");
				libcli_register_display_callback(&interface, display_node_interfaces);
				libcli_register_param(&node, &interface);
				{
					static param_t if_name;
					init_param(&if_name, LEAF, 0, 0,
						0, STRING, "if-name", "Interface Name");
					libcli_register_param(&interface, &if_name);
					{
						static param_t if_up_down_status;
						init_param(&if_up_down_status, LEAF, 0, intf_config_handler,
							validate_if_up_down_status, STRING, "if-up-down", "<up | down>");
						libcli_register_param(&if_name, &if_up_down_status);
						set_param_cmd_code(&if_up_down_status, CMDCODE_CONF_INTF_UP_DOWN);
					}
				}
			}

			{
				static param_t route;
				init_param(&route, CMD, "route", 0, NULL, INVALID, NULL, "L3 route");
				libcli_register_param(&node_name, &route);

				{
					static param_t ip_addr;
					init_param(&ip_addr, LEAF, 0, NULL, NULL, IPV4, "ip-address", 
					"IPV4 Address");
					libcli_register_param(&route, &ip_addr);
					
					{
						static param_t mask;
						init_param(&mask, LEAF, NULL, l3_config_handler, validate_mask_value,
								INT, "mask", 
								"mask(0-32)");
						libcli_register_param(&ip_addr, &mask);
						set_param_cmd_code(&mask, CMDCODE_CONF_NODE_L3ROUTE);

						{
							static param_t gwip;
							init_param(&gwip, LEAF, 0, 0, 0,
								IPV4, "gw-ip", 
								"IPv4 Address");
							libcli_register_param(&mask, &gwip);

							{
								static param_t oif;
								init_param(&oif, LEAF, NULL, l3_config_handler, 0,
										STRING, "oif", "Out going intf name");
								libcli_register_param(&gwip, &oif);
								set_param_cmd_code(&oif, CMDCODE_CONF_NODE_L3ROUTE);
							}
						}
					}
				}
			}
			support_cmd_negation(&node_name);
		}
	}
	
	support_cmd_negation(config);
}

