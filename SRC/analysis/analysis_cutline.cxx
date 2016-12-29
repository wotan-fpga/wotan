/*
	The 'cutline' method of analyzing reachability in a graph is invoked after paths have been enumerated
through the routing resource graph.

	The 'cutline' method estimates the probability that a given source/sink pair can be routed by breaking
up the subgraph between the source/sink into distinct levels. A set of nodes make up each level, and the set
of nodes in each level is orthogonal from all other levels (i.e. each node belongs to one level only). The 
probability that the source/sink CAN'T be routed is taken to be the probability that any one of the subgraph
levels in completely unavailable. The probabilities of different nodes beign unavailable are taken to be
independent of each other.

	Each node is assigned to its own level. blah blah blah, pigeon-holes nodes too much

	The 'cutline' method of reachability analysis builds on top of a topological traversal of the subraph. Hence
the constituent functions are meant to be passed-in to a topological traversal function (see topological_traversal.h)

//TODO: how are levels calculated? etc
*/

#include "analysis_main.h"
#include "analysis_cutline.h"
#include "exception.h"
#include "wotan_util.h"

using namespace std;



/**** Function Declarations ****/
/* adds specified node to the cutline probability structure according to the node's level in the topological traversal */
static void add_node_to_cutline_structure(int node_ind, t_node_topo_inf &node_topo_inf, t_cutline_prob_struct &cutline_probability_struct);
/* sets topological traversal level of specified node according to the parent node */
static void set_node_level(int parent_ind, int node_ind, t_node_topo_inf &node_topo_inf);
/* estimates probability that a source/dest connection can be made based on the cutline structure */
static float connection_probability_cutlines(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_node_topo_inf &node_topo_inf, Cutline_Structs *cutline_structs,
                                             User_Options *user_opts);



/**** Function Definitions ****/
/* Called when node is popped from expansion queue during topological traversal */
void cutline_node_popped_func(int popped_node, int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data){
	
	//cout << "popped node: " << popped_node << endl;
	Cutline_Structs *cutline_structs = (Cutline_Structs*)user_data;
	add_node_to_cutline_structure(popped_node, node_topo_inf, cutline_structs->cutline_prob_struct);
}

/* Called when topological traversal is iterateing over a node's children */
bool cutline_child_iterated_func(int parent_ind, int parent_edge_ind, int node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
                          e_traversal_dir traversal_dir, int max_path_weight, int from_node_ind, int to_node_ind, User_Options *user_opts, void *user_data){

	bool ignore_node = false;

	set_node_level(parent_ind, node_ind, node_topo_inf);
	
	return ignore_node;
}

/* Called once topological traversal is complete.
   Calculates probability of a source/sink connection being routable */
void cutline_traversal_done_func(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data){
	
	Cutline_Structs *cutline_structs = (Cutline_Structs*)user_data;
	float prob_routable = connection_probability_cutlines(from_node_ind, to_node_ind, rr_node, node_topo_inf, cutline_structs, user_opts);

	cutline_structs->prob_routable = prob_routable;
}


/* adds specified node to the cutline probability structure according to the node's level in the topological traversal */
static void add_node_to_cutline_structure(int node_ind, t_node_topo_inf &node_topo_inf, t_cutline_prob_struct &cutline_probability_struct){
	int node_level = node_topo_inf[node_ind].get_level();
	int max_cutline_level = (int)cutline_probability_struct.size() - 1;

	if (node_level < 0){
		WTHROW(EX_PATH_ENUM, "Got node with topological traversal level less than 0");
	}

	if (node_level - max_cutline_level > 1){
		WTHROW(EX_PATH_ENUM, "Should not get node with topological traversal level 2 (or more) above any nodes encountered so far");
	} else if (node_level - max_cutline_level == 1){
		/* need to create a new level in the cutline structure */
		cutline_probability_struct.push_back( vector<int>() );
	}

	/* put node onto the cutline structure according to its level */
	cutline_probability_struct[node_level].push_back(node_ind);
}


/* sets topological traversal level of specified node according to the parent node */
static void set_node_level(int parent_ind, int node_ind, t_node_topo_inf &node_topo_inf){
	int parent_level = node_topo_inf[parent_ind].get_level();
	int node_level = node_topo_inf[node_ind].get_level();

	if (parent_level == UNDEFINED){
		WTHROW(EX_PATH_ENUM, "Parent level is undefined");
	}

	/* child level according to lowest parent level */
	if (node_level == UNDEFINED || parent_level < node_level){
		node_level = parent_level + 1;
	}
	node_topo_inf[node_ind].set_level( node_level );
//	cout << " parent " << parent_ind << " set level of " << node_ind << " to " << node_level << endl;

	///* child level is highest of parent levels */
	//if (parent_level >= node_level && parent_level != UNDEFINED){
	//	node_level = parent_level + 1;
	//	node_topo_inf[node_ind].set_level( node_level );
	//}
}

/* estimates probability that a source/dest connection can be made based on the cutline structure */
static float connection_probability_cutlines(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_node_topo_inf &node_topo_inf, Cutline_Structs *cutline_structs,
                                             User_Options *user_opts){
	
	t_cutline_prob_struct &cutline_probability_struct = cutline_structs->cutline_prob_struct;
	//int num_levels = (int)cutline_probability_struct.size();
	int num_levels = node_topo_inf[to_node_ind].get_level();	//don't do anything >= than the sink's level

	if (num_levels < 2){
		WTHROW(EX_PATH_ENUM, "Expected at least 2 levels"); 
	}

	float unreachable = 0;
	/* topological traversal starts at 'from' node but never places the 'to' node onto expansion queue.
	   would like to not take into account the 'from'/'to' nodes here */
	for (int ilevel = 1; ilevel < num_levels; ilevel++){
		int num_nodes = (int)cutline_probability_struct[ilevel].size();

		/* skip if there aren't any nodes on this traversal level */
		if (num_nodes == 0){
			continue;
		}


		/* AND probabilities of nodes on the same level */
		float level_prob = 1;
		for (int inode = 0; inode < num_nodes; inode++){
			int node_ind = cutline_probability_struct[ilevel][inode];


			/* skip if this node is the source or target node */
			if (node_ind == from_node_ind || node_ind == to_node_ind){
				WTHROW(EX_PATH_ENUM, "Should not find source/dest nodes on level structure");
			}

			//float node_demand = rr_node[node_ind].get_demand();
			float node_demand = get_node_demand_adjusted_for_path_history(node_ind, rr_node, from_node_ind, to_node_ind, cutline_structs->fill_type, user_opts);

			/* bound probability that node is unavailable to 1 */
			float node_unavailable = min(1.0F, node_demand);

			//if (from_node_ind == 4185 /*&& to_node_ind == 5870*/){
			//	cout << "  to " << to_node_ind << "  level " << ilevel << " node " << node_ind << "  unavailable " << node_unavailable << endl;
			//}

			if (node_unavailable < 0){
				WTHROW(EX_PATH_ENUM, "node unavailable prob smaller than 0??");
			}

			level_prob *= node_unavailable;
		}
		//if (from_node_ind == 4185 /*&& to_node_ind == 5870*/){
		//	cout << "     level prob: " << level_prob << endl;
		//}

		/* OR probabilities of different levels being unavailable */
		unreachable = or_two_probs(level_prob, unreachable);
	}
	//if (from_node_ind == 4185 /*&& to_node_ind == 5870*/){
	//	cout << "unreachable: " << unreachable << endl;
	//}

	//cout << "  unreachable: " << unreachable << endl;

	return (1 - unreachable);
}

