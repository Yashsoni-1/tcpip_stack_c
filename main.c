#include <stdio.h>
#include "graph.h"
#include "net.h"
#include "CommandParser/libcli.h"

extern graph_t *build_first_topo(void);

graph_t *topo = NULL;

int main(int argc, const char * argv[]) {
    
	nw_init_cli();
    	topo = build_first_topo();
	start_shell();
    
    return 0;
}
