#ifndef ANALYSIS_CUTLINE_SIMPLE_H
#define ANALYSIS_CUTLINE_SIMPLE_H

#include "wotan_types.h"


/**** Typedefs ****/
typedef std::vector< std::vector<int> > t_cutline_simple_prob_struct;


/**** Classes ****/
/* A class used to lump together all data structures specific to the cutline_simple analysis method that
   need to be passed around during topological traversal */
class Cutline_Simple_Structs{
public:
	float prob_routable;
	t_cutline_simple_prob_struct cutline_simple_prob_struct;
	Physical_Type_Descriptor *fill_type;
};


/**** Function Declarations ****/
/* Called when node is popped from expansion queue during topological traversal */
void cutline_simple_node_popped_func(int popped_node, int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data);

/* Called when topological traversal is iterateing over a node's children */
bool cutline_simple_child_iterated_func(int parent_ind, int parent_edge_ind, int node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
                          e_traversal_dir traversal_dir, int max_path_weight, int from_node_ind, int to_node_ind, User_Options *user_opts, void *user_data);

/* Called once topological traversal is complete */
void cutline_simple_traversal_done_func(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data);


#endif
