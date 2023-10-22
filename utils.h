#ifndef utils_h
#define utils_h

#include <stdio.h>
#include <string.h>



typedef enum {
    FALSE,
    TRUE
} bool_t;


void apply_mask(char *prefix, char mask, char *str_prefix);

void layer2_fill_with_broadcast_mac(unsigned char *mac_array);

#define IS_MAC_BROADCAST(mac) \
 (mac[0] == 0xFF && mac[1] == 0xFF && mac[2] == 0xFF && \
  mac[3] == 0xFF && mac[4] == 0xFF && mac[5] == 0xFF)

unsigned int
ip_p_to_n(char *ip_addr);

char *
ip_n_to_p(unsigned int ip_addr,
          char *output_buffer);


#endif /* utils_h */
