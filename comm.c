
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include "comm.h"
#include "graph.h"
#include "net.h"
#include <errno.h>


static int
_send_pkt_out(int sock_fd, char *pkt_data, unsigned int pkt_size,
                         unsigned int dst_udp_port_no)
{
    printf("\nIn the function %s\n", __FUNCTION__);

    int rc = 0;
    struct sockaddr_in dest_addr;
    
    struct hostent *host = (struct hostent *) gethostbyname("127.0.0.1");
    
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = dst_udp_port_no;
    dest_addr.sin_addr = *((struct in_addr *)host->h_addr);
    
    rc = sendto(sock_fd, pkt_data,
                pkt_size, 0, (struct sockaddr *)&dest_addr,
                sizeof(struct sockaddr));
    
    
    if(rc == -1) {
        perror("Couldn't send the data to udp port\n");
    }
    return rc;
}

static unsigned int udp_port_number = 40000;

static unsigned int
get_next_udp_port_number()
{
    return udp_port_number++;
}

void
init_udp_socket(node_t *node)
{
    printf("\nIn the function %s\n", __FUNCTION__);

    if(node->udp_port_number) return;

    node->udp_port_number = get_next_udp_port_number();
    
    int udp_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    if(udp_sock_fd == -1) {
        printf("Socked creation failed for node %s\n", node->name);
        return;
    }
    
    struct sockaddr_in node_addr;
    node_addr.sin_family = AF_INET;
    node_addr.sin_port = node->udp_port_number;
    node_addr.sin_addr.s_addr = INADDR_ANY;
    
    
    if(bind(udp_sock_fd,
            (struct sockaddr *)&node_addr,
            sizeof(struct sockaddr)) == -1) {
        printf("Error : failed to bind the socket for node %s\n",
               node->name);
        return;
    }
    
    node->udp_sock_fd = udp_sock_fd;
}

static char recv_buffer[MAX_PACKET_BUFFER_SIZE];
static char send_buffer[MAX_PACKET_BUFFER_SIZE];

static void
_pkt_receive(node_t *receiving_node,
             char *pkt_with_aux_data,
             unsigned int pkt_size)
{
    printf("\nIn the function %s\n", __FUNCTION__);

    char *recv_intf_name = pkt_with_aux_data;
    interface_t *recv_intf = get_node_if_by_name(receiving_node,
                                                 recv_intf_name);
    if(!recv_intf) {
        printf("Error : pkt received on unknown interface %s on node %s\n",
               recv_intf->if_name, receiving_node->name);
        return;
    }
    
    pkt_receive(receiving_node, recv_intf,
                pkt_with_aux_data + IF_NAME_SIZE,
                pkt_size - IF_NAME_SIZE);
}

static void *
_network_start_pkt_receiver_thread(void *arg)
{
    printf("\nIn the function %s\n", __FUNCTION__);

    node_t *node;
    glthread_t *curr;
    
    fd_set active_sock_fd_set,
           backup_sock_fd_set;
    
    int sock_max_fd = 0,
        bytes_recvd = 0;
    
    graph_t *topo = (void *)arg;
    
    int addr_len = sizeof(struct sockaddr);
    
    FD_ZERO(&active_sock_fd_set);
    FD_ZERO(&backup_sock_fd_set);

    struct sockaddr_in sender_addr;
    
    ITERATE_GLTHREAD_BEGIN(&topo->node_list, curr) {
        
        node = graph_glue_to_node(curr);
        
        if(!node->udp_sock_fd)  continue;
            
        if(node->udp_sock_fd > sock_max_fd)
            sock_max_fd = node->udp_sock_fd;
        
        FD_SET(node->udp_sock_fd, &backup_sock_fd_set);
        
    } ITERATE_GLTHREAD_END(&topo->node_list, curr);
    
    while(1) {
        memcpy(&active_sock_fd_set, &backup_sock_fd_set, sizeof(fd_set));
        select(sock_max_fd + 1, &active_sock_fd_set, NULL, NULL, NULL);
        
        ITERATE_GLTHREAD_BEGIN(&topo->node_list, curr) {
            
            node = graph_glue_to_node(curr);
            
            if(FD_ISSET(node->udp_sock_fd, &active_sock_fd_set)) {
                
                memset(recv_buffer, 0, MAX_PACKET_BUFFER_SIZE);
                bytes_recvd = (int)recvfrom(node->udp_sock_fd,
                                            (char *)recv_buffer, MAX_PACKET_BUFFER_SIZE,
                                            0, (struct sockaddr *)&sender_addr,
                                            (unsigned int *)&addr_len);
                
                _pkt_receive(node, recv_buffer, bytes_recvd);
                
            }
        } ITERATE_GLTHREAD_END(&topo->node_list, curr);
    }
}

void
network_start_pkt_receiver_thread(graph_t *topo)
{
    printf("\nIn the function %s\n", __FUNCTION__);

    pthread_attr_t attr;
    pthread_t recv_pkt_thread;
    
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&recv_pkt_thread, &attr,
                   _network_start_pkt_receiver_thread, (void *)topo);
}


int send_pkt_out(char *pkt, unsigned int pkt_size,
                 interface_t *interface)
{
    printf("\nIn the function %s\n", __FUNCTION__);

    int rc = 0;
    node_t *sending_node = interface->att_node;
    node_t *nbr_node = get_nbr_node(interface);
    
    if(!nbr_node) return -1;
    
    if(pkt_size + IF_NAME_SIZE > MAX_PACKET_BUFFER_SIZE)
    {
        printf("Error: Node :%s, Pkt size exceeds\n", sending_node->name);
        return -1;
    }
    
    unsigned int dst_udp_port_no = nbr_node->udp_port_number;
    
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    if(sock < 0) {
        printf("Error : Sending socket Creation failed, errno = %d\n", errno);
        return -1;
    }
    
    interface_t *other_interface = &interface->link->intf1 == interface ? \
                                    &interface->link->intf2 : &interface->link->intf1;
    
    memset(send_buffer, 0, MAX_PACKET_BUFFER_SIZE);
    
    char *pkt_with_aux_data = send_buffer;
    
    strncpy(pkt_with_aux_data, other_interface->if_name,
            IF_NAME_SIZE);
    
    pkt_with_aux_data[IF_NAME_SIZE - 1] = '\0';
    
    memcpy(pkt_with_aux_data + IF_NAME_SIZE, pkt, pkt_size);
    
    rc = _send_pkt_out(sock, pkt_with_aux_data, pkt_size + IF_NAME_SIZE,
                       dst_udp_port_no);
    close(sock);
    return rc;
}

extern void
layer2_frame_recv(node_t *node, interface_t *interface,
                  char *pkt, unsigned int pkt_size);

int
pkt_receive(node_t *node,
                 interface_t *interface,
                 char *pkt,
                 unsigned int pkt_size)
{
    printf("\n\nmsg recvd = %s, on node = %s, iif = %s\n", pkt, node->name, interface->if_name);
    
    return 0;
    
    pkt = pkt_buffer_shift_right(pkt, pkt_size,
                                 MAX_PACKET_BUFFER_SIZE - IF_NAME_SIZE);
    layer2_frame_recv(node, interface, pkt, pkt_size);
    return 0;
}

int send_pkt_flood(node_t *node,
                   interface_t *exempted_intf,
                   char *pkt, unsigned int pkt_size)
{
    interface_t *intf;
    
    for(int i=0; i < MAX_INTF_PER_NODE; ++i)
    {
        intf = node->interfaces[i];
        if(!intf) return 0;
        
        if(intf == exempted_intf) continue;
            
        send_pkt_out(pkt, pkt_size, intf);
    }
    return 0;
}
