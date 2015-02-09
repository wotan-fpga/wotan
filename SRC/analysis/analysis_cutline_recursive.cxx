/*
	The 'cutline' method of analyzing reachability in a graph is invoked after paths have been enumerated
through the routing resource graph.

	blah blah

	The 'cutline recursive' method of reachability analysis builds on top of a topological traversal of the subraph. Hence
the constituent functions are meant to be passed-in to a topological traversal function (see topological_traversal.h)

*/

#include <algorithm>
#include "analysis_main.h"
#include "analysis_cutline_recursive.h"
#include "topological_traversal.h"
#include "exception.h"
#include "wotan_util.h"

using namespace std;



/**** Function Declarations ****/
/* returns height of node based on node's corresponding ss_distances */
static int get_node_height(SS_Distances &node_ss_distances);
/* returns true if specified node has legal parents (their source-hop number is smaller) at the specified height */
static bool has_parents_of_height(int node_ind, int height, t_rr_node &rr_node, t_ss_distances &ss_distances, e_traversal_dir traversal_dir,
                                  int max_path_weight);
/* adds specified node to the cutline probability structure according to the node's level in the topological traversal */
static void add_node_to_cutline_structure(int node_ind, int level, t_cutline_rec_prob_struct &cutline_probability_struct);
/* estimates probability that a source/dest connection can be made based on the cutline structure. 
   returns UNDEFINED if any one of the levels is empty */
static float connection_probability_cutlines(t_rr_node &rr_node, Cutline_Recursive_Structs *cutline_rec_structs, t_node_topo_inf &node_topo_inf,
                                         t_topo_inf_backups &topo_inf_backups, int recursion_level, t_ss_distances &ss_distances,
					 User_Options *user_opts);



/**** Function Definitions ****/
/* Called when node is popped from expansion queue during topological traversal */
void cutline_recursive_node_popped_func(int popped_node, int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data){

	Cutline_Recursive_Structs *cutline_rec_structs = (Cutline_Recursive_Structs*)user_data;
	int relative_level = UNDEFINED;

	/* get node's height relative to root node of this traversal */
	int node_height = get_node_height(ss_distances[popped_node]);
	int from_height = get_node_height(ss_distances[from_node_ind]);
	int relative_height = node_height - from_height;

	/* get node's source hops relative to root node of this traversal (this isn't necessarily the shortest number of hops from root to this node -- only an approximation) */
	int node_source_hops = ss_distances[popped_node].get_source_hops();
	int from_source_hops = ss_distances[from_node_ind].get_source_hops();
	int relative_source_hops = node_source_hops - from_source_hops;


	/* Error Checks */
	//TODO: remove this after debugging done
	if (relative_source_hops < 0){
		WTHROW(EX_PATH_ENUM, "Seem to have stepped backward from root node");
	}
	if (relative_source_hops >= cutline_rec_structs->bound_source_hops){
		WTHROW(EX_PATH_ENUM, "Seem to have exceeded source-hop bounds");
	}
	if (relative_height < 0){
		WTHROW(EX_PATH_ENUM, "Seem to have stepped down in height");
	}


	/* If node hasn't been 'smoothed' out in a different traversal, then it will be assigned a level.
	   But if node has been smoothed, it doesn't get a level; though its children are still traversed and potentially placed on expansion queue */
	bool node_smoothed = node_topo_inf[popped_node].get_node_smoothed();
	if (!node_smoothed){
		/* set relative node level */
		if (relative_height == 0){
			/* at same height as root --> level is relative hops from root */
			relative_level = relative_source_hops;
		} else {
			if ( has_parents_of_height(popped_node, node_height, rr_node, ss_distances, traversal_dir, max_path_weight) ){
				/* not the first node of its height */
				relative_level = relative_source_hops - relative_height;
			} else {
				/* first node at this height -- some of its descendents need to be smoothed out so that nodes in
				   subgraph can be assigned levels in a more-natural manner */
	
				Cutline_Recursive_Structs new_cutline_rec_structs;
				new_cutline_rec_structs.bound_source_hops = node_source_hops + relative_height + 1;
				new_cutline_rec_structs.recurse_level = cutline_rec_structs->recurse_level + 1;
				new_cutline_rec_structs.cutline_rec_prob_struct.assign( relative_height + 1, vector<int>() );
				new_cutline_rec_structs.source_ind = cutline_rec_structs->source_ind;
				new_cutline_rec_structs.sink_ind = cutline_rec_structs->sink_ind;
				new_cutline_rec_structs.fill_type = cutline_rec_structs->fill_type;

				/* back up topo inf for this node */
				Topo_Inf_Backup node_backup;
				node_backup.backup(popped_node, node_topo_inf);
				node_backup.clear_node_topo_inf(node_topo_inf);

				/* RECURSE on this node */
				do_topological_traversal(popped_node, to_node_ind, rr_node, ss_distances, node_topo_inf, traversal_dir,
							max_path_weight, user_opts, (void*)&new_cutline_rec_structs,
							cutline_recursive_node_popped_func,
						 	cutline_recursive_child_iterated_func,
							cutline_recursive_traversal_done_func);

				/* resture this node's backed up info */
				node_backup.restore(node_topo_inf);

				/* get adjusted demand of this node from passed-in new_cutline-rec_structs */
				float prob_routable = new_cutline_rec_structs.prob_routable;

				if (prob_routable == UNDEFINED){
					node_topo_inf[popped_node].set_node_smoothed(true);
					node_smoothed = true;
				} else {
					float popped_node_demand = get_node_demand_adjusted_for_path_history(popped_node, rr_node, cutline_rec_structs->source_ind,
										 cutline_rec_structs->sink_ind, cutline_rec_structs->fill_type, user_opts);

					float adjusted_demand = or_two_probs(popped_node_demand, 1-prob_routable);
					node_topo_inf[popped_node].set_adjusted_demand( adjusted_demand );
				}

				/* if node wasn't smoothed out as a result of the previous recursive traversal, then it is assigned a level */
				if (!node_smoothed){
					relative_level = relative_source_hops;
				} else {
					relative_level = UNDEFINED;
				}
			}
		}

		if (relative_level > 0){
			/* if the node was assigned a relative level, push it onto the cutline probabilities structure */
			//cout << "relative source hops: " << relative_source_hops << "  relative_height: " << relative_height << endl;
			add_node_to_cutline_structure(popped_node, relative_level, cutline_rec_structs->cutline_rec_prob_struct);
		}

		if (cutline_rec_structs->recurse_level != 0 && popped_node != from_node_ind){
			/* Recursion to higher recurse levels is meant to smooth out nodes. If a node is smoothed
			   out here, it will remain smoothed out in the parent traversal, which is what we want */
			node_topo_inf[popped_node].set_node_smoothed(true);
		}
	}
}


/* Called when topological traversal is iterateing over a node's children */
bool cutline_recursive_child_iterated_func(int parent_ind, int node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
                          e_traversal_dir traversal_dir, int max_path_weight, int from_node_ind, int to_node_ind, User_Options *user_opts, void *user_data){

	Cutline_Recursive_Structs *cutline_rec_structs = (Cutline_Recursive_Structs*)user_data;
	bool ignore_node = false;

	/* check height/levels of children */
	/* get node's height relative to root node of this traversal */
	int node_height = get_node_height(ss_distances[node_ind]);
	int from_height = get_node_height(ss_distances[from_node_ind]);
	int relative_height = node_height - from_height;

	/* get node's source hops relative to root node of this traversal (this isn't necessarily the shortest number of hops from root to this node -- only an approximation) */
	int node_source_hops = ss_distances[node_ind].get_source_hops();
	int from_source_hops = ss_distances[from_node_ind].get_source_hops();
	int relative_source_hops = node_source_hops - from_source_hops;

	//TODO: are these all correct?
	if (relative_source_hops < 0){
		ignore_node = true;
	}
	if (relative_source_hops >= cutline_rec_structs->bound_source_hops){
		ignore_node = true;
	}
	if (relative_height < 0){
		ignore_node = true;
	}

	//node_topo_inf[node_ind].set_was_visited(true);

	/* backup node info if this is not the top-most level of recursion */
	if (cutline_rec_structs->recurse_level > 0 && !ignore_node){
		//TODO: problem. node info needs to be cleared before child traversal function looks at any node inf data...
		//	but only thing child traversal checks is done_from_source/sink. I don't think a parent traversal
		//	would set this value before a child traversal does, so should be OK...

		/* backup child node inf into a map if it's not already there */
		t_topo_inf_backups &topo_inf_backups = cutline_rec_structs->topo_inf_backups;

		if (topo_inf_backups.count(node_ind) == 0){
			/* first time encountering this node, it's not on the backups map yet */
			Topo_Inf_Backup topo_inf_backup;
			topo_inf_backup.backup(node_ind, node_topo_inf);
			topo_inf_backups[node_ind] = topo_inf_backup;
		}

		/* now clear relevant data fields in the node_topo_inf structure (each recursive traversal need to start fresh) */
		topo_inf_backups[node_ind].clear_node_topo_inf( node_topo_inf );
	}

	return ignore_node;
}

/* Called once topological traversal is complete.
   Calculates probability of a source/sink connection being routable */
void cutline_recursive_traversal_done_func(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data){
	/* compute probability that connection is routable. also restore backed-up node inf's */
	Cutline_Recursive_Structs *cutline_rec_structs = (Cutline_Recursive_Structs*)user_data;

	float routable = connection_probability_cutlines(rr_node, cutline_rec_structs, node_topo_inf,
	                                               cutline_rec_structs->topo_inf_backups, cutline_rec_structs->recurse_level, ss_distances, user_opts);
	cutline_rec_structs->prob_routable = routable;
}


/* returns height of node based on node's corresponding ss_distances */
static int get_node_height(SS_Distances &node_ss_distances){
	int height;

	int source_hops = node_ss_distances.get_source_hops();
	int sink_hops = node_ss_distances.get_sink_hops();
	
	if (source_hops < 0 || sink_hops < 0){
		height = UNDEFINED;
		//WTHROW(EX_PATH_ENUM, "Source hops or sink hops is < 0");
	} else {
		height = source_hops + sink_hops;
	}

	return height;
}

/* returns true if specified node has legal parents (their source-hop number is smaller) at the specified height */
static bool has_parents_of_height(int node_ind, int height, t_rr_node &rr_node, t_ss_distances &ss_distances, e_traversal_dir traversal_dir,
                                  int max_path_weight){
	bool result = false;
	int node_source_hops = ss_distances[node_ind].get_source_hops();

	int *edge_list;
	int num_parents;

	/* want to iterate over parent nodes; this depends on direction of traversal */
	if (traversal_dir == FORWARD_TRAVERSAL){
		edge_list = rr_node[node_ind].in_edges;
		num_parents = rr_node[node_ind].get_num_in_edges();
	} else {
		edge_list = rr_node[node_ind].out_edges;
		num_parents = rr_node[node_ind].get_num_out_edges();
	}

	/* traverse all parents */
	for (int inode = 0; inode < num_parents; inode++){
		int parent_ind = edge_list[inode];

		/* skip illegal parents */
		int parent_weight = rr_node[parent_ind].get_weight();
		if ( !ss_distances[parent_ind].is_legal(parent_weight, max_path_weight) ){
			continue;
		}

		/* if this node has a lower source-hop number, then it can be considered a parent */
		int parent_height = get_node_height( ss_distances[parent_ind]);
		if ( ss_distances[parent_ind].get_source_hops() < node_source_hops && parent_height == height){
			result = true;
			break;
		}
	}

	return result;
}

/* adds specified node to the cutline probability structure according to the node's level in the topological traversal */
static void add_node_to_cutline_structure(int node_ind, int level, t_cutline_rec_prob_struct &cutline_probability_struct){

	int num_cutlines = (int)cutline_probability_struct.size();
	if (level >= num_cutlines){
		//WTHROW(EX_PATH_ENUM, "Level of node with index " << node_ind << " exceeds number of cutline levels" << endl <<
		//                     "num_cutlines: " << num_cutlines << "  level: " << level << endl);
		//TODO: figure out how the above condition is possible!
		return;
	}
	if (level < 0){
		WTHROW(EX_PATH_ENUM, "Level of node with index " << node_ind << " is less than 0: " << level);
	}

	/* put node onto the cutline structure according to its level */
	//cout << "  level: " << level << "  num_cutlines: " << num_cutlines << endl;
	cutline_probability_struct[level].push_back(node_ind);
}


/* estimates probability that a source/dest connection can be made based on the cutline structure. 
   returns UNDEFINED if any one of the levels is empty */
static float connection_probability_cutlines(t_rr_node &rr_node, Cutline_Recursive_Structs *cutline_rec_structs, t_node_topo_inf &node_topo_inf,
                                         t_topo_inf_backups &topo_inf_backups, int recursion_level, t_ss_distances &ss_distances,
					 User_Options *user_opts){
	
	t_cutline_rec_prob_struct &cutline_probability_struct = cutline_rec_structs->cutline_rec_prob_struct;

	float reachable = UNDEFINED;
	int num_levels = (int)cutline_probability_struct.size();

	bool empty_level = false;
	float unreachable = 0;
	for (int ilevel = 1; ilevel < num_levels; ilevel++){
		int num_nodes = (int)cutline_probability_struct[ilevel].size();

		/* want to return UNDEFINED if any one level is empty */
		if (num_nodes == 0){
			empty_level = true;
			break;
		}

		/* AND probabilities of nodes on the same level */
		float level_prob = 1;
		for (int inode = 0; inode < num_nodes; inode++){
			int node_ind = cutline_probability_struct[ilevel][inode];

			//commenting because a smoothing traversal smoothes nodes, but still assigns them a level
			///* check that node wasn't smoothed-out */
			//if (node_topo_inf[node_ind].get_node_smoothed()){
			//	WTHROW(EX_PATH_ENUM, "Node with index " << node_ind << " was assigned a level, but apparently is supposed to be smoothed out");
			//}

			/* look at adjusted node demand (node may have been root of a recursed traversal */
			float node_demand = node_topo_inf[node_ind].get_adjusted_demand();
			if (node_demand == UNDEFINED){
				node_demand = get_node_demand_adjusted_for_path_history(node_ind, rr_node, cutline_rec_structs->source_ind,
				                                         cutline_rec_structs->sink_ind, cutline_rec_structs->fill_type, user_opts);
			}

			//cout << "node " << node_ind << " on cutline level " << ilevel << "  demand: " << node_demand << endl;
			//cout << "  source hops: " << ss_distances[node_ind].get_source_hops() << "  sink hops: " << ss_distances[node_ind].get_sink_hops() << endl;

			/* bound probability that node is unavailable to 1 */
			float node_unavailable = min(1.0F, node_demand);

			level_prob *= node_unavailable;

			/* restore this node's topo inf */
			if (recursion_level > 0){
				if (topo_inf_backups.count(node_ind) > 0){
					topo_inf_backups[node_ind].restore( node_topo_inf );
				} else {
					WTHROW(EX_PATH_ENUM, "Node with index " << node_ind << " was not backed up or appeared in more than one cutline level." << endl <<
							"Recursion level: " << recursion_level);
				}
			}
		}

		/* OR probabilities of different levels being unavailable */
		unreachable = or_two_probs(level_prob, unreachable);
	}

	if (!empty_level){
		reachable = 1 - unreachable;
	} else {
		reachable = UNDEFINED;
	}

	return reachable;
}


/* backup topological node info */
void Topo_Inf_Backup::backup(int target_node_ind, t_node_topo_inf &node_topo_inf){
	Node_Topological_Info &topo_inf = node_topo_inf[target_node_ind];
	this->node_ind = target_node_ind;
	this->done_from_source = topo_inf.get_done_from_source();
	this->done_from_sink = topo_inf.get_done_from_sink();
	this->times_visited_from_source = topo_inf.get_times_visited_from_source();
	this->times_visited_from_sink = topo_inf.get_times_visited_from_sink();
	this->num_legal_in_nodes = topo_inf.get_num_legal_in_nodes();
	this->num_legal_out_nodes = topo_inf.get_num_legal_out_nodes();
	this->node_smoothed = topo_inf.get_node_smoothed();
	this->node_waiting_info = topo_inf.node_waiting_info;

}
/* restore topological node info */
void Topo_Inf_Backup::restore(t_node_topo_inf &node_topo_inf){
	Node_Topological_Info &topo_inf = node_topo_inf[this->node_ind];

	topo_inf.set_done_from_source( this->done_from_source );
	topo_inf.set_done_from_sink( this->done_from_sink );
	topo_inf.set_times_visited_from_source( this->times_visited_from_source );
	topo_inf.set_times_visited_from_sink( this->times_visited_from_sink );
	topo_inf.set_num_legal_in_nodes( this->num_legal_in_nodes );
	topo_inf.set_num_legal_out_nodes( this->num_legal_out_nodes );
	topo_inf.set_node_smoothed( this->node_smoothed );
	topo_inf.node_waiting_info = this->node_waiting_info;

}

/* clears relevant node_topo_inf structures (should be backed up before this is called */
void Topo_Inf_Backup::clear_node_topo_inf( t_node_topo_inf &node_topo_inf ){
	
	Node_Topological_Info &topo_inf = node_topo_inf[this->node_ind];
	
	topo_inf.set_done_from_source( false );
	topo_inf.set_done_from_sink( false );
	topo_inf.set_times_visited_from_source( 0 );
	topo_inf.set_times_visited_from_sink( 0 );
	topo_inf.set_num_legal_in_nodes( UNDEFINED );
	topo_inf.set_num_legal_out_nodes( UNDEFINED );
	topo_inf.set_node_smoothed( false );
	topo_inf.node_waiting_info.clear();
}

