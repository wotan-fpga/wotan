#include "wotan_cleanup.h"

using namespace std;


/**** Function Declarations ****/
/* frees rr nodes */
static void free_rr_nodes(t_rr_node &rr_node, int num_rr_nodes);


/**** Function Definitions ****/

/* frees allocated structures */
void free_wotan_structures(Arch_Structs *arch_structs, Routing_Structs *routing_structs){
	
	/* Free rr nodes */
	free_rr_nodes(routing_structs->rr_node, routing_structs->get_num_rr_nodes());

}

/* frees rr nodes */
static void free_rr_nodes(t_rr_node &rr_node, int num_rr_nodes){
	for (int inode = 0; inode < num_rr_nodes; inode++){
		rr_node[inode].free_allocated_members();
	}
}
