#include <stdio.h>
#include "../../tcp_public.h"

extern graph_t *topo;

void compute_spf(node_t *spf_root)
{
	printf("%s called...\n", __FUNCTION__);
}

static void show_spf_results(node_t *node)
{
	printf("%s called...\n", __FUNCTION__);
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

