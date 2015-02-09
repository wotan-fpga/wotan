/*
	The 'cutline_simple' method of analyzing reachability in a graph is invoked after paths have been enumerated
through the routing resource graph.

	blah blah blah

	The 'cutline_simple' method of reachability analysis builds on top of a topological traversal of the subraph. Hence
the constituent functions are meant to be passed-in to a topological traversal function (see topological_traversal.h)

*/

#include <cmath>
#include "analysis_main.h"
#include "analysis_cutline_simple.h"
#include "exception.h"
#include "wotan_util.h"

using namespace std;



/**** Function Declarations ****/
/* returns estimate of probability that sink is reachable from source */
static float get_prob_reachable(t_rr_node &rr_node, int from_node_ind, int to_node_ind, Cutline_Simple_Structs *cutline_simple_structs, User_Options *user_opts);


/**** Function Definitions ****/
/* Called when node is popped from expansion queue during topological traversal */
void cutline_simple_node_popped_func(int popped_node, int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data){

	Cutline_Simple_Structs *cutline_simple_structs = (Cutline_Simple_Structs*)user_data;

	if (popped_node == from_node_ind || popped_node == to_node_ind){
		return;
	}

	/* check source/sink hops of this node and assign it to a level based on that */
	int source_hops = ss_distances[popped_node].get_source_hops();
	int sink_hops = ss_distances[popped_node].get_sink_hops();

	int level_from_source = source_hops - 1;
	int level_from_sink = sink_hops - 1;


	t_cutline_simple_prob_struct &prob_struct = cutline_simple_structs->cutline_simple_prob_struct;
	int num_prob_struct_entries = (int)prob_struct.size();
	int last_entry_ind = num_prob_struct_entries - 1;
	int source_demarcation = (int)ceil( (float)num_prob_struct_entries / 2.0 ) - 1;
	int sink_demarcation = last_entry_ind - (source_demarcation + 1);

	//a sanity check
	if (level_from_source <= source_demarcation && level_from_sink <= sink_demarcation){
		WTHROW(EX_PATH_ENUM, "a node should not fall into *both* the source and sink's spheres of influence");
	}

	int index = UNDEFINED;
	if (level_from_source <= source_demarcation){
		index = level_from_source;
	} else if (level_from_sink <= sink_demarcation){
		index = num_prob_struct_entries - 1 - level_from_sink;
	} else {
		index = UNDEFINED;
	}
	//cout << "last_entry_ind: " << last_entry_ind << "  level_from_source: " << level_from_source << "  level_from_sink: " << level_from_sink <<
	//	"  source_demarcation: " << source_demarcation << "  sink_demarcation: " << sink_demarcation << "  index: " << index << endl;

	if (index >= 0){
		prob_struct[index].push_back( popped_node );
	}

	//bool level_assigned = false;
	//if (level_from_source >= 0 && level_from_source < 2){
	//	cutline_simple_structs->cutline_simple_prob_struct[level_from_source].push_back( popped_node );
	//	//cout << " node " << popped_node << " onto level " << level_from_source << endl;
	//	level_assigned = true;
	//}

	//if (level_from_sink >= 0 && level_from_sink < 2 && !level_assigned){	//avoid inserting the same node into the level structure
	//	cutline_simple_structs->cutline_simple_prob_struct[3 - level_from_sink].push_back( popped_node );
	//	//cout << " node " << popped_node << " onto level " << 3 - level_from_sink << endl;
	//}
}

/* Called when topological traversal is iterateing over a node's children */
bool cutline_simple_child_iterated_func(int parent_ind, int node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
                          e_traversal_dir traversal_dir, int max_path_weight, int from_node_ind, int to_node_ind, User_Options *user_opts, void *user_data){
	bool ignore_node = false;

	return ignore_node;
}

/* Called once topological traversal is complete.
   Calculates probability of a source/sink connection being routable */
void cutline_simple_traversal_done_func(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data){

	Cutline_Simple_Structs *cutline_simple_structs = (Cutline_Simple_Structs*)user_data;


	float prob_routable = get_prob_reachable( rr_node, from_node_ind, to_node_ind, cutline_simple_structs, user_opts );

	cutline_simple_structs->prob_routable = prob_routable;
}


/* returns estimate of probability that sink is reachable from source */
static float get_prob_reachable(t_rr_node &rr_node, int from_node_ind, int to_node_ind, Cutline_Simple_Structs *cutline_simple_structs, User_Options *user_opts){
	float prob_unreachable = 0;

	t_cutline_simple_prob_struct &cutline_simple_prob_struct = cutline_simple_structs->cutline_simple_prob_struct;

	Physical_Type_Descriptor *fill_type = cutline_simple_structs->fill_type;

	int num_levels = (int)cutline_simple_prob_struct.size();
	for (int ilevel = 0; ilevel < num_levels; ilevel++){
		int num_nodes = (int)cutline_simple_prob_struct[ilevel].size();

		if (num_nodes == 0){
			continue;
		}

		float level_prob = 1;
		for (int inode = 0; inode < num_nodes; inode++){
			int node_ind = cutline_simple_prob_struct[ilevel][inode];

			if (node_ind == from_node_ind || node_ind == to_node_ind){
				WTHROW(EX_PATH_ENUM, "Didn't expect node_ind to be source/sink node");
			}

			//cout << "node ind: " << node_ind << endl;

			float node_demand = get_node_demand_adjusted_for_path_history(node_ind, rr_node, from_node_ind, to_node_ind, fill_type, user_opts);

			float node_unavailable = min(1.0F, node_demand);

			level_prob *= node_unavailable;
		}

		prob_unreachable = or_two_probs(level_prob, prob_unreachable);
	}

	return (1 - prob_unreachable);
}

