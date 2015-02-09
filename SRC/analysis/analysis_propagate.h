#ifndef ANALYSIS_PROPAGATE_H
#define ANALYSIS_PROPAGATE_H

#include <vector>
#include "wotan_types.h"


/**** Typedefs ****/

/**** Classes ****/
/* A class used to lump together all data structures specific to the propagate analysis method that
   need to be passed around during topological traversal */
class Propagate_Structs{
public:
	float prob_routable;
	Physical_Type_Descriptor *fill_type;
};


/**** Function Declarations ****/
/* Called when node is popped from expansion queue during topological traversal */
void propagate_node_popped_func(int popped_node, int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data);

/* Called when topological traversal is iterateing over a node's children */
bool propagate_child_iterated_func(int parent_ind, int node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
                          e_traversal_dir traversal_dir, int max_path_weight, int from_node_ind, int to_node_ind, User_Options *user_opts, void *user_data);

/* Called once topological traversal is complete */
void propagate_traversal_done_func(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data);


#endif
