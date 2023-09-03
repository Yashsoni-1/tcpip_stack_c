#include <stdio.h>
#include "graph.h"
#include "net.h"
#include "CommandParser/libcli.h"

extern graph_t *build_first_topo(void);
extern graph_t *build_simple_l2_switch_topo(void);
extern graph_t *build_linear_topo(void);
extern graph_t *build_dualswitch_topo(void);
extern graph_t *build_square_topo(void);
extern void nw_init_cli(void);

graph_t *topo = NULL;

int main(int argc, const char * argv[]) {
    
	nw_init_cli();
    	topo = build_square_topo();
	start_shell();
    
    return 0;
}
