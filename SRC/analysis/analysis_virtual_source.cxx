/* NATHAN
	FIX This file contains the functions that obtains the virtual sources for further path enumeration.
*/

#include <vector>
#include "exception.h"
#include "analysis_virtual_source.h"
#include "analysis_main.h"
#include "wotan_types.h"

using namespace std;

void print_ss_dist(t_ss_distances &ss_distances)
{
	for (unsigned int i = 0; i < ss_distances.size(); i++)
	{
		cout << "\ta)" << i << ": " << ss_distances[i].get_source_distance() << endl;
		cout << "\tb)" << i << ": " << ss_distances[i].get_sink_distance() << endl;
	}
}

// Gets sink index on the same logic block
int get_sink_node_ind(Routing_Structs *routing_structs, int current_sink_ind)
{
	t_rr_node_index rr_nodes_ind = routing_structs->rr_node_index;
	RR_Node *node = &routing_structs->rr_node[current_sink_ind];

	int x = node->get_xlow();
	int y = node->get_ylow();
	vector<int> sinks_at_curr_loc = rr_nodes_ind[SINK][x][y];
	unsigned int num_sinks_here = sinks_at_curr_loc.size();
	//cout << "Number of sinks at (" << x << ',' << y << "): " << num_sinks_here << endl;
	if (num_sinks_here <= 0) {
		WTHROW(EX_PATH_ENUM, "No sinks at " << x << ',' << y);
	}

	// Get arbitrary sink index on same logic block
	unsigned int i = 0;
	int new_sink_ind = sinks_at_curr_loc[i];
	while (new_sink_ind == current_sink_ind)
	{
		i += 1;
		if (i >= num_sinks_here) {
			WTHROW(EX_PATH_ENUM, "Only one sink at " << x << ',' << y);
		}
		new_sink_ind = sinks_at_curr_loc[i];
	}

	//cout << "Route to sink: " << new_sink_ind << endl;
	return new_sink_ind;
}

/*
void enumerate_from_virtual_source(int virt_src, int to_node_ind, Analysis_Settings *analysis_settings
								   Arch_Structs *arch_structs, Routing_Structs *routing_structs,
								   t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, int conn_length
								   t_nodes_visited &nodes_visited, User_Options *user_opts)
{
	enumerate_connection_paths(virt_src, to_node_ind, analysis_settings, arch_structs,
							   routing_structs, ss_distances, node_topo_inf, conn_length,
							   nodes_visited, user_opts, (float) UNDEFINED);
	
}
*/

// Fill the virtual_sources vector with sources to do further path enumeration
void propagate_backwards(int from_node_ind, t_rr_node &rr_node, t_node_topo_inf &node_topo_inf, 
						 vector<int> &virtual_sources, float prob_reachable, User_Options *user_opts,
						 t_ss_distances &ss_distances, int max_path_weight, int level)
{
	// For debugging
	//for (int i = 0; i < level; i++)
	//	cout << '\t';
	//cout << from_node_ind << ": " << prob_reachable << endl;

	// Hard code min. threshold for further expansion
	if (prob_reachable < 0.75) {
		return;
	}

	RR_Node *node = &rr_node[from_node_ind];
	int num_in_edges = node->get_num_in_edges();
	// Don't want to expand a virtual source
	if (node->get_is_virtual_source()) {
		return;
	}

	// Don't want to expand a node on an illegal path
	if (!ss_distances[from_node_ind].is_legal(rr_node[from_node_ind].get_weight(), max_path_weight)) {
		cout << "Node not on legal path." << endl;
		return;
	}
	// If we hit a node with no incoming edges, we're done.
	if (num_in_edges == UNDEFINED) {
		//cout << "At source: " << from_node_ind << endl;
		return;
	}

	// If node corresponds to a virtual source, then add to vector
	if (node->get_virtual_source_node_ind() != UNDEFINED)
	{
		int virtual_node_ind = node->get_virtual_source_node_ind();
		virtual_sources.push_back(virtual_node_ind);
		//cout << "Adding " << virtual_node_ind << " to list of virtual sources." << endl;
	}

	// Iterate over the parent nodes (nodes with outgoing edges to "from_node_ind").
	// DFS backwards through the graph to add all virtual sources satisfying the min. threshold
	int *in_edges = node->in_edges;
	for (int i = 0; i < num_in_edges; i++)
	{
		int prev_edge = in_edges[i];
		double *source_buckets = node_topo_inf[prev_edge].buckets.source_buckets;
		int num_source_buckets = node_topo_inf[prev_edge].buckets.get_num_source_buckets();
		float prob_routable = get_prob_reachable(source_buckets, num_source_buckets);
		propagate_backwards(prev_edge, rr_node, node_topo_inf, virtual_sources, prob_routable,
							user_opts, ss_distances, max_path_weight, level+1);
	}
}

float get_prob_reachable(double *source_buckets, int num_source_buckets)
{
	float running_total = 0;
	for (int ibucket = 0; ibucket < num_source_buckets; ibucket++)
	{
		float bucket_value = source_buckets[ibucket];
		if (bucket_value != UNDEFINED) {
			running_total = or_two_probs(running_total, bucket_value);
		}
	}
	return running_total;
}


