#ifndef comm_h
#define comm_h

#define MAX_PACKET_BUFFER_SIZE 2048

typedef struct node_ node_t;
typedef struct interface_ interface_t;


int
send_pkt_out(char *pkt, unsigned int pkt_size,
                 interface_t *interface);

int
pkt_receive(node_t *node,
            interface_t *interface,
            char *pkt,
            unsigned int pkt_size);

int
send_pkt_flood(node_t *node,
               interface_t *exempted_intf,
               char *pkt,
               unsigned int pkt_size);

#endif /* comm_h */
