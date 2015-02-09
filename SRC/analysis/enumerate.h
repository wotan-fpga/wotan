#ifndef ENUMERATE_H
#define ENUMERATE_H

#include "wotan_types.h"


/**** Classes ****/
class Enumerate_Structs{
public:
	/* will be used to return the number of legal nodes encountered during 
	   path enumeration */
	int num_routing_nodes_in_subgraph;
	e_bucket_mode mode;

	Enumerate_Structs(){
		this->num_routing_nodes_in_subgraph = 0;
	}
};


/**** Function Declarations ****/
/* Called when node is popped from expansion queue during topological traversal */
void enumerate_node_popped_func(int popped_node, int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data);

/* Called when topological traversal is iterateing over a node's children */
bool enumerate_child_iterated_func(int parent_ind, int node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
                          e_traversal_dir traversal_dir, int max_path_weight, int from_node_ind, int to_node_ind, User_Options *user_opts, void *user_data);

/* Called once topological traversal is complete */
void enumerate_traversal_done_func(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data);


#endif
