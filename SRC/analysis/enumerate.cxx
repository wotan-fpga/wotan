/*
	Enumerates paths from a source node to a destination node. Enumerated paths are limited by a 
maximum bound on their path weight.

	Path enumeration through a subgraph builds on top of a topological traversal function. Hence
the constituent functions are meant to be passed-in to a topological traversal function (see topological_traversal.h)

*/

#include <pthread.h>
#include "enumerate.h"
#include "exception.h"
#include "wotan_util.h"
#include "globals.h"

#include "draw.h"
#include "analysis_main.h"


using namespace std;


/**** Function Declarations ****/
/* propagates path counts stored in the bucket structure of the parent node to the bucket structure of the child node */
static void propagate_path_counts(int parent_ind, int parent_edge_ind, int child_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
			e_traversal_dir traversal_dir, int max_path_weight, e_bucket_mode enumerate_mode, e_self_congestion_mode self_congestion_mode);


/**** Function Definitions ****/
/* Called when node is popped from expansion queue during topological traversal */
void enumerate_node_popped_func(int popped_node, int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data){

	/* increment node demand during forward traversal only */
	if (traversal_dir == FORWARD_TRAVERSAL){
		/* Increment demand of nodes based on paths enumerated through them */
		e_rr_type node_type = rr_node[popped_node].get_rr_type();

		//Note: I added OPIN and IPIN checks below because otherwise high opin demand skews comparisons (I think) unfairly
		//	away from opin-equivalent architectures. (see commit 8392b21)
		if (node_type != SOURCE && node_type != SINK && node_type != OPIN /*&& node_type != IPIN*/){
			int node_weight = rr_node[popped_node].get_weight();
			int dist_to_source = ss_distances[popped_node].get_source_distance();
			float demand_contribution = node_topo_inf[popped_node].buckets.get_num_paths(node_weight, dist_to_source, max_path_weight);

			/* apply the demand multiplier to this node if it is not of type OPIN/IPIN/SOURCE/SINK */
			//commenting this because I want to apply it when we actually use the demand, not when we set it
			//if (node_type != OPIN /*&& node_type != IPIN*/){
			//	demand_contribution *= user_opts->demand_multiplier;
			//}
			rr_node[popped_node].increment_demand( demand_contribution, user_opts->demand_multiplier);

			/* It is possible to keep a history of how many paths there are connecting each source/sink with the
			   nearby nodes. This path count history can be used to later subtract the demand due to a source/sink pair
			   (from nodes being traversed) when analyzing *that specific* source sink pair. Here we make a record
			   of this node's demand that is due to this source/sink pair */
			if (user_opts->self_congestion_mode == MODE_RADIUS){
				e_rr_type type = rr_node[popped_node].get_rr_type();
				if (type == OPIN || type == IPIN || type == CHANX || type == CHANY){
					rr_node[popped_node].increment_path_count_history(demand_contribution, rr_node[from_node_ind]);
					rr_node[popped_node].increment_path_count_history(demand_contribution, rr_node[to_node_ind]);
				}
			}

			//pthread_mutex_lock(&g_mutex);
			//g_enum_nodes_popped++;
			//pthread_mutex_unlock(&g_mutex);
		}

		/* add to existing count of the number of routing nodes (CHANX/CHANY/IPIN/OPIN) in the legal subgraph
		   (this is used for reliability polynomial computations) */
		Enumerate_Structs *enumerate_structs = (Enumerate_Structs *)user_data;
		int popped_node_weight = rr_node[popped_node].get_weight();
		if ( ss_distances[popped_node].is_legal(popped_node_weight, max_path_weight) ){
			if (node_type == CHANX || node_type == CHANY || node_type == IPIN || node_type == OPIN){
				enumerate_structs->num_routing_nodes_in_subgraph++;
			}
		}
	}
}

/* Called when topological traversal is iterateing over a node's children */
bool enumerate_child_iterated_func(int parent_ind, int parent_edge_ind, int node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
                          e_traversal_dir traversal_dir, int max_path_weight, int from_node_ind, int to_node_ind, User_Options *user_opts, void *user_data){
	bool ignore_node = false;

	Enumerate_Structs *enumerate_structs = (Enumerate_Structs *)user_data;

	/* propagate the path counts (stored in the bucket structure) of the parent node to this node */
	//if (enumerate_structs->mode == BY_PATH_HOPS){
	//	cout << "from: " << from_node_ind << "  to: " << to_node_ind << endl;
	//	cout << "child: " << node_ind << "  parent: " << parent_ind << endl;
	//}
	propagate_path_counts(parent_ind, parent_edge_ind, node_ind, rr_node, ss_distances, node_topo_inf, traversal_dir, max_path_weight, enumerate_structs->mode,
	                      user_opts->self_congestion_mode);

	//if (from_node_ind == 5784 && to_node_ind == 6950){
	//	cout << parent_ind << " to " << node_ind << endl;
	//}

	return ignore_node;
}

/* Called once topological traversal is complete. */
void enumerate_traversal_done_func(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data){
	
	/* nothing to be done */
}


/* propagates path counts stored in the bucket structure of the parent node to the bucket structure of the child node */
static void propagate_path_counts(int parent_ind, int parent_edge_ind, int child_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
			e_traversal_dir traversal_dir, int max_path_weight, e_bucket_mode enumerate_mode, e_self_congestion_mode self_congestion_mode){

	if (enumerate_mode != BY_PATH_WEIGHT && enumerate_mode != BY_PATH_HOPS){
		WTHROW(EX_PATH_ENUM, "Unknown enumeration mode: " << enumerate_mode);
	}

	double *parent_buckets;
	double *child_buckets;
	int num_buckets;
	int child_dist_to_target = UNDEFINED;	//minimum distance from child to target node
	int parent_dist_to_start = UNDEFINED;	//minimum distance from parent to start node

	int max_dist = max_path_weight;
	int child_weight = rr_node[child_ind].get_weight();
	if (enumerate_mode == BY_PATH_HOPS){
		child_weight = 1;
		max_dist += 3;
	}
	

	/* get bucket structures according to direction of traversal */
	if (traversal_dir == FORWARD_TRAVERSAL){
		parent_buckets = node_topo_inf[parent_ind].buckets.source_buckets;
		child_buckets = node_topo_inf[child_ind].buckets.source_buckets;
		num_buckets = node_topo_inf[parent_ind].buckets.get_num_source_buckets();

		if (enumerate_mode == BY_PATH_WEIGHT){
			/* path weight to a node already includes the weight of that node */
			child_dist_to_target = ss_distances[child_ind].get_sink_distance();
			parent_dist_to_start = ss_distances[parent_ind].get_source_distance();
		} else {
			/* # hops between two nodes is essentially the number of edges between them */
			child_dist_to_target = ss_distances[child_ind].get_sink_hops() + 1;	//need to account for hop between parent/child
			parent_dist_to_start = ss_distances[parent_ind].get_source_hops();
			//cout << "child to target: " << child_dist_to_target << "  parent to start: " << parent_dist_to_start << endl;
		}
	} else {
		parent_buckets = node_topo_inf[parent_ind].buckets.sink_buckets;
		child_buckets = node_topo_inf[child_ind].buckets.sink_buckets;
		num_buckets = node_topo_inf[parent_ind].buckets.get_num_sink_buckets();

		if (enumerate_mode == BY_PATH_WEIGHT){
			/* path weight to a node already includes the weight of that node */
			child_dist_to_target = ss_distances[child_ind].get_source_distance();
			parent_dist_to_start = ss_distances[parent_ind].get_sink_distance();
		} else {
			child_dist_to_target = ss_distances[child_ind].get_source_hops() + 1;	//need to account for hop between parent/child
			parent_dist_to_start = ss_distances[parent_ind].get_sink_hops();
		}
	}
	
	if (parent_dist_to_start < 0){
		WTHROW(EX_PATH_ENUM, "Parent node has distance to start node of < 0: " << parent_dist_to_start);
	}

	/* now propagate parent path counts to the child */
	for (int ibucket = parent_dist_to_start; ibucket < num_buckets; ibucket++){

		/* we're done if this set of paths cannot possibly reach the target node 
		   in under the minimum allowable path weight */
		if (ibucket + child_dist_to_target > max_dist){
			break;
		}

		/* parent has no paths in this bucket -- nothing to propagate */
		if (parent_buckets[ibucket] == UNDEFINED){
			continue;
		}

		/* bucket into which to propagate probabilities */
		int target_bucket = ibucket + child_weight;

		/* propagate the parent path counts to child */
		if (child_buckets[target_bucket] == UNDEFINED){
			child_buckets[target_bucket] = parent_buckets[ibucket];
		} else {
			child_buckets[target_bucket] += parent_buckets[ibucket];
		}

		if (self_congestion_mode == MODE_PATH_DEPENDENCE){
			if (traversal_dir == FORWARD_TRAVERSAL){
				//keep incremental track of the demands contributed to children (for each possible path weight)
				pthread_mutex_lock(&rr_node[parent_ind].my_mutex);
				rr_node[parent_ind].child_demand_contributions[parent_edge_ind][ibucket] += parent_buckets[ibucket];
				pthread_mutex_unlock(&rr_node[parent_ind].my_mutex);
			}
		}
	}
}

