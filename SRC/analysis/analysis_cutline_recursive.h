#ifndef ANALYSIS_CUTLINE_RECURSIVE_H
#define ANALYSIS_CUTLINE_RECURSIVE_H

#include <vector>
#include <map>
#include "wotan_types.h"

/**** Forward-Declares ****/
class Topo_Inf_Backup;


/**** Typedefs ****/
/* used to store node indices for each level of the subgraph */
typedef std::vector< std::vector< int > > t_cutline_rec_prob_struct;
/* used to backup relevant node information during recursive topological traversal. key to map is node index */
typedef std::map< int, Topo_Inf_Backup > t_topo_inf_backups;


/**** Classes ****/
/* used for backing up select info from the node_topo_inf structure so that traversing graph
   recursively doesn't overwrite relevant data for parent traversal */
class Topo_Inf_Backup{
private:
	int node_ind;			/* index of node for which data is being backed up */

	bool done_from_source;
	bool done_from_sink;
	int times_visited_from_source;
	int times_visited_from_sink;
	short num_legal_in_nodes;
	short num_legal_out_nodes;
	bool node_smoothed;
	Node_Waiting node_waiting_info;
public:

	/* backup method */
	void backup( int node_ind, t_node_topo_inf &node_topo_inf );
	/* restore method */
	void restore( t_node_topo_inf &node_topo_inf );
	/* clears relevant node_topo_inf_structures (should be backed up before this is called */
	void clear_node_topo_inf( t_node_topo_inf &node_topo_inf );
};

/* A class used to lump together all data structures specific to the 'cutline recursive' analysis method that
   need to be passed around during topological traversal */
class Cutline_Recursive_Structs{
public:
	/* the upper bound on source hops. nodes with >= source hops are not to be accounted for */
	int bound_source_hops;
	/* the recursion level of the currente topological traversal */
	int recurse_level;
	/* a backup of nodes' node_topo_inf structs that is made as nodes are visited during this topological traversal,
	   and restored at the end of the traversal */
	t_topo_inf_backups topo_inf_backups;
	/* a structure that keeps track of which nodes are at which level for this topological traversal */
	t_cutline_rec_prob_struct cutline_rec_prob_struct;
	/* the probability that one of the levels for the current topological traversal is completely unavailable.
	   this is set at the end of the traversal */
	float prob_routable;

	/* the source node of the traversal (at the base recursion level) */
	int source_ind;
	/* the sink node of the traversal (at the base recursion level) */
	int sink_ind;
	/* the physical type descriptor for the 'fill' type block (i.e. the CLB) */
	Physical_Type_Descriptor *fill_type;

};


/**** Function Declarations ****/
/* Called when node is popped from expansion queue during topological traversal */
void cutline_recursive_node_popped_func(int popped_node, int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data);

/* Called when topological traversal is iterateing over a node's children */
bool cutline_recursive_child_iterated_func(int parent_ind, int node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
                          e_traversal_dir traversal_dir, int max_path_weight, int from_node_ind, int to_node_ind, User_Options *user_opts, void *user_data);

/* Called once topological traversal is complete */
void cutline_recursive_traversal_done_func(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data);


#endif
