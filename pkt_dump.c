//
//  pkt_dump.c
//  tcp_ip_stack_c
//
//  Created by Yash soni on 13/08/23.
//

#include <stdio.h>
#include "Layer2/layer2.h"



void pkt_dump(ethernet_hdr_t *ethernet_hdr, unsigned int pkt_size)
{
    printf("Ethernet hdr:\n");
    print_mac("dst_mac", ethernet_hdr->dst_mac.mac);
    print_mac(" src_mac", ethernet_hdr->src_mac.mac);
    
    vlan_8021q_hdr_t *vlan_8021q_hdr = is_pkt_vlan_tagged(ethernet_hdr);
    
    if(vlan_8021q_hdr)
    {
        printf(" type = 802.1q\n tpid = %d, tci_pcp = %d, tci_dei = %d, tci_vid = %d\n",
               vlan_8021q_hdr->tpid,
               vlan_8021q_hdr->tci_pcp,
               vlan_8021q_hdr->tci_dei,
               vlan_8021q_hdr->tci_vid);
        return;
    }
    
    printf(" type = %d\n", ethernet_hdr->type);
    if(ethernet_hdr->type == ARP_MSG)
    {
        arp_hdr_t *arp_hdr = (arp_hdr_t *)ethernet_hdr->payload;
        printf(" ARP:\n hw_type = %d, proto_type = %d, hw_addr_len = %d, proto_addr_len = %d, op_code = %d",
               arp_hdr->hw_type, arp_hdr->proto_type, arp_hdr->hw_addr_len, arp_hdr->proto_addr_len, arp_hdr->op_code);
        
        print_mac("\n src_mac", arp_hdr->src_mac.mac);
        char src_ip[16];
        char dst_ip[16];
        ip_n_to_p(arp_hdr->src_ip, src_ip);
        ip_n_to_p(arp_hdr->dest_ip, dst_ip);
        printf("src_ip : %s, dst_ip : %s\n ", src_ip, dst_ip);
    }
}
