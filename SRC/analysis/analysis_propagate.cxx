/*
	The 'propagate' method of analyzing reachability in a graph is invoked after paths have been enumerated
through the routing resource graph.

	The 'propagate' method estimates the probability that a given source/sink pair can be routed by propagating
node probabilities from parent nodes to children nodes during traversal. TODO etc


	The 'propagate' method of reachability analysis builds on top of a topological traversal of the subraph. Hence
the constituent functions are meant to be passed-in to a topological traversal function (see topological_traversal.h)

*/

#include "analysis_main.h"
#include "analysis_propagate.h"
#include "exception.h"
#include "wotan_util.h"
#include "globals.h"

using namespace std;



/**** Function Declarations ****/
static void account_for_current_node_probability(int node_ind, int node_weight, float node_demand, t_node_topo_inf &node_topo_inf, t_rr_node &rr_node);
/* propagates path probabilities stored in the bucket structure of the parent node to the bucket structure of the child node */
static void propagate_probabilities(int parent_ind, int parent_edge_ind, int child_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
			e_traversal_dir traversal_dir, int max_path_weight);
/* probability that node with specified buckets is reachable from source */
static float get_prob_reachable( double *source_buckets, int num_source_buckets);



/**** Function Definitions ****/
/* Called when node is popped from expansion queue during topological traversal */
void propagate_node_popped_func(int popped_node, int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data){

	Propagate_Structs *propagate_structs = (Propagate_Structs*)user_data;

	/* the path probabilities have been propagated from upstream nodes to this node, but
	   the probability of *this* node has not yet been factored in. this is done now */
	int node_weight = rr_node[popped_node].get_weight();
	//float node_demand = rr_node[popped_node].get_demand();
	float node_demand = get_node_demand_adjusted_for_path_history(popped_node, rr_node, from_node_ind, to_node_ind, propagate_structs->fill_type, user_opts);
	float adjusted_demand = min(1.0F, node_demand);
	//cout << "   node " << popped_node << "  rr_type: " << rr_node[popped_node].get_rr_type_string() << "  weight: " << node_weight << "  demand: " << node_demand << endl;
	//cout << "       before: " << rr_node[popped_node].get_demand(user_opts) << endl;

	account_for_current_node_probability(popped_node, node_weight, adjusted_demand, node_topo_inf, rr_node);

	//pthread_mutex_lock(&g_mutex);
	//g_prob_nodes_popped++;
	//pthread_mutex_unlock(&g_mutex);
}

/* Called when topological traversal is iterateing over a node's children */
bool propagate_child_iterated_func(int parent_ind, int parent_edge_ind, int node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
                          e_traversal_dir traversal_dir, int max_path_weight, int from_node_ind, int to_node_ind, User_Options *user_opts, void *user_data){
	bool ignore_node = false;

	/* propagate the node probabilities (stores in the bucket structure) of the parent node to this node */
	propagate_probabilities(parent_ind, parent_edge_ind, node_ind, rr_node, ss_distances, node_topo_inf, traversal_dir, max_path_weight);

	return ignore_node;
}

/* Called once topological traversal is complete.
   Calculates probability of a source/sink connection being routable */
void propagate_traversal_done_func(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data){
	Propagate_Structs *propagate_structs = (Propagate_Structs*)user_data;

	double *source_buckets = node_topo_inf[to_node_ind].buckets.source_buckets;
	int num_source_buckets = node_topo_inf[to_node_ind].buckets.get_num_source_buckets();
	propagate_structs->prob_routable = get_prob_reachable(source_buckets, num_source_buckets);
}

/* Probability of a path successfully traversing through a given node is the probability that the path can reach the node AND'ed with the
   probability that the node is uncongested */
static void account_for_current_node_probability(int node_ind, int node_weight, float node_demand, t_node_topo_inf &node_topo_inf, t_rr_node &rr_node){
	double *source_buckets = node_topo_inf[node_ind].buckets.source_buckets;
	int num_source_buckets = node_topo_inf[node_ind].buckets.get_num_source_buckets();

	//TODO: subtract the demand contributed from *valid* parents of this node so we don't count it twice
	//Need to know:
	//	1) The demand contributed by each parent
	//	2) Which parents are valid in the current s-t connection
	//
	//Solution:
	//	1) Keep this in a vector associated with each child node a node has
	//	2) Introduce a variable in the topo inf structure of each node -- how much demand is contributed by a node's parents?
	//		- This can be incremented when a parent visits the child during topological traversal
	//		- PROBLEM: what if parent contributes a small number of paths (say only weight 8) but and the child then applies
	//		  the discount to all paths through it? (i.e. weight 1,2,3,4,5, etc)
	//			- It is clear that discount from parent should *only* be applied to those paths which the parent contributed
	//			- SOLUTION: in topo inf structure, keep another "bucket" to determine demand discounts from parents

	vector <bool> discount_bucket_demand;
	discount_bucket_demand.assign(num_source_buckets, false);

	float demand_discount = 0;
	for (int ibucket = 0; ibucket < num_source_buckets; ibucket++){
		demand_discount += node_topo_inf[node_ind].demand_discounts[ibucket];
		if (node_topo_inf[node_ind].demand_discounts[ibucket] > 0.0){
			discount_bucket_demand[ibucket] = true;
		}

	}

	for (int ibucket = 0; ibucket < num_source_buckets; ibucket++){
		float adjusted_node_demand = node_demand;
		if (discount_bucket_demand[ibucket]){
			adjusted_node_demand -= demand_discount;
		}

		if (source_buckets[ibucket] != UNDEFINED){
			//source_buckets[ibucket] = or_two_probs(source_buckets[ibucket], min(1.0F, node_demand));	//unreachability

			//Basically AND'ing the probability that the node can be reached via a path of a given weight (ibucket) with the
			//probability that the node in question is available
			source_buckets[ibucket] = source_buckets[ibucket] * (1 - min(1.0F, adjusted_node_demand));		//reachability
		}
	}
}


/* propagates path probabilities stored in the bucket structure of the parent node to the bucket structure of the child node */
static void propagate_probabilities(int parent_ind, int parent_edge_ind, int child_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
			e_traversal_dir traversal_dir, int max_path_weight){

	double *parent_buckets;
	double *child_buckets;
	int num_buckets;
	int child_weight = rr_node[child_ind].get_weight();
	int child_path_weight_to_dest;		//the weight of the minimum-weight path from child to the destination node
	int parent_path_weight_to_start;	//the weight of the minimum-weight path from parent to the starting node

	/* get bucket structures according to direction of traversal */
	if (traversal_dir == FORWARD_TRAVERSAL){
		parent_buckets = node_topo_inf[parent_ind].buckets.source_buckets;
		child_buckets = node_topo_inf[child_ind].buckets.source_buckets;
		num_buckets = node_topo_inf[parent_ind].buckets.get_num_source_buckets();

		/* path weight to sink (includes weight of child node) */
		child_path_weight_to_dest = ss_distances[child_ind].get_sink_distance();
		
		parent_path_weight_to_start = ss_distances[parent_ind].get_source_distance();
	} else {
		parent_buckets = node_topo_inf[parent_ind].buckets.sink_buckets;
		child_buckets = node_topo_inf[child_ind].buckets.sink_buckets;
		num_buckets = node_topo_inf[parent_ind].buckets.get_num_sink_buckets();

		/* path weight to source (includes weight of child node) */
		child_path_weight_to_dest = ss_distances[child_ind].get_source_distance();

		parent_path_weight_to_start = ss_distances[parent_ind].get_sink_distance();
	}

	/* now propagate path probabilities. the assumption is that every single path is independent (perhaps not a very good assumption)
	   TODO. add better description */
	for (int ibucket = parent_path_weight_to_start; ibucket < num_buckets; ibucket++){	//parent cannot carry paths of weight smaller than itself
		/* we're done if this set of paths cannot possibly reach the target node 
		   in under the minimum allowable path weight */
		if (ibucket + child_path_weight_to_dest > max_path_weight){
			break;
		}

		/* bucket into which to propagate probabilities */
		int target_bucket = ibucket + child_weight;

		/* propagate the probability of paths *not* being available */
		if (child_buckets[target_bucket] == UNDEFINED){
			if (parent_buckets[ibucket] != UNDEFINED){
				child_buckets[target_bucket] = parent_buckets[ibucket];
			}
		} else {
			if (parent_buckets[ibucket] != UNDEFINED){
				//child_buckets[target_bucket] *= parent_buckets[ibucket];	//unreachability
				child_buckets[target_bucket] = or_two_probs(child_buckets[target_bucket], parent_buckets[ibucket]);	//reachability
			}
		}

		if (traversal_dir == FORWARD_TRAVERSAL){
			node_topo_inf[child_ind].demand_discounts[target_bucket] += rr_node[parent_ind].child_demand_contributions[parent_edge_ind][ibucket];
		}
	}
}

/* probability that node with specified buckets is reachable from source */
static float get_prob_reachable( double *source_buckets, int num_source_buckets){
	
	float running_total = 0;
	for (int ibucket = 0; ibucket < num_source_buckets; ibucket++){
		float bucket_value = source_buckets[ibucket];
		if (bucket_value != UNDEFINED){
			running_total = or_two_probs(running_total, bucket_value);
		}
	}

	return running_total;
}

