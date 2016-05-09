
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <queue>
#include <set>
#include <utility>
#include <functional>
#include <pthread.h>
#include <malloc.h>
#include "analysis_main.h"
#include "wotan_types.h"
#include "exception.h"
#include "wotan_util.h"
#include "draw.h"
#include "topological_traversal.h"
#include "enumerate.h"
#include "analysis_cutline.h"
#include "analysis_cutline_recursive.h"
#include "analysis_propagate.h"
#include "analysis_cutline_simple.h"
#include "analysis_reliability_poly.h"


using namespace std;


/**** Defines ****/
/* Used to set the maximum path weight to be considered for path enumeration & probabilitiy analysis. 
   The maximum pathweight considered for a source-sink pair is (weight from source to sink)*PATH_FLEXIBILITY_FACTOR.
   Note however that there is an additional constraint on maximum path weight set by analysis_settings->get_max_path_weight
     - if (weight from source to sink) exceeds this then the connection simply won't be analyzed. */				//TODO: confusing. can consolidate?
#define PATH_FLEXIBILITY_FACTOR 2.0

/* If core analysis is enabled in user options then probability analysis is only performed for blocks in the region 
   that is >= 'CORE_OFFSET' blocks away from the perimeter */
#define CORE_OFFSET 3

/* Which probability analysis mode should be used? See e_probability_mode for options */
#define PROBABILITY_MODE PROPAGATE //CUTLINE_SIMPLE

/* what percentage of worst node demands to look at? */
//#define WORST_NODE_DEMAND_PERCENTILE 0.05

/* what percentage of worst connection probabilities (at each connection length) to look at? */
#define WORST_ROUTABILITY_PERCENTILE_DRIVERS 0.1		//0.1 driver and 0.5 fanout looked fairly good
#define WORST_ROUTABILITY_PERCENTILE_FANOUT 0.1

/* with what weights should driver & fanout components of the routability metric be combined */
#define DRIVER_PROB_WEIGHT 0.5
#define FANOUT_PROB_WEIGHT 0.0	//TODO: set to 0 and re-check results...
				//TODO: also, I have disabled pin demands. What does 4LUT case look like now? If it looks bad I should re-enable them.
				//After that re-enable pin demands... were my previous tests with or without them? :|
				//TODO: switch to cutline for shits and giggles?

#define FRACTION_CONNS 0.1


/************ Forward-Declarations ************/
class Conn_Info;



/************ Enums ************/
/* specified a mode for topological graph traversal */
enum e_topological_mode{
	ENUMERATE = 0,		/* enumerates paths through each node */
	PROBABILITY		/* calculate probability of reaching the destination node based on already-calculated node demands */
};

/* specifies mode of probability analysis to do								//TODO: outdated comment
   PROPAGATE: probabilities are propagated from source to sink using bucket structures.
   	Can estimate probabilities of reaching a node by looking at the probabilities of reaching
	that node's parents (and so forth)
   CUTLINE: probability of reaching sink is analyzed by looking at probabilities along different
   	levels of a topological traversal through a graph (i.e. can't reach sink if an entire level
	is unavailable for routing) */
enum e_probability_mode{
	PROPAGATE = 0,
	CUTLINE,
	CUTLINE_SIMPLE,
	CUTLINE_RECURSIVE,
	RELIABILITY_POLYNOMIAL
};



/************ Typedefs ************/
/* a t_ss_distances structure for each thread */
typedef vector< t_ss_distances > t_thread_ss_distances;
/* a t_node_topo_inf structure for each thread */
typedef vector< t_node_topo_inf > t_thread_node_topo_inf;
/* a list of nodes visited during path enumeration */
typedef vector< int > t_nodes_visited;
/* a t_nodes_visited structure for each thread */
typedef vector< t_nodes_visited > t_thread_nodes_visited;
/* contains pthread info for each thread */
typedef vector< pthread_t > t_threads;

/* A structure that is used to break cycles during topological traversal. Objects of the
   Node_Waiting class are put on this sorted structure, and if the traditional expansion queue 
   becomes empty during topological traversal, this structure is used to get the next node on which
   to expand */
typedef set< Node_Waiting > t_nodes_waiting;

/* used to analyze reachability by looking at a percentile of the least routable connections at each length */
typedef My_Fixed_Size_PQ< float, less<float> > t_lowest_probs_pq;

/* for each thread, a structure that defines the enumeration problem for said thread */
typedef vector< Conn_Info > t_thread_conn_info;



/************ Classes ************/

/* contains the node indices of a source/sink pair for which a connection should be analyzed */
class Source_Sink_Pair{
public:
	int source_ind;
	int sink_ind;
	int ss_length;		//TODO this and below variable should be computed in "enumerate_paths_from_source". but i was lazy here so i'm wasting memory
	int source_conns_at_length;
};

/* used for multithreading of path enumeration / probability analysis.
   defines the problem parameters for each thread */
class Conn_Info{
public:
	vector<Source_Sink_Pair> source_sink_pairs;
	User_Options *user_opts;
	Analysis_Settings *analysis_settings;
	Arch_Structs *arch_structs;
	Routing_Structs *routing_structs;
	t_ss_distances *ss_distances;
	t_node_topo_inf *node_topo_inf;
	t_nodes_visited *nodes_visited;
	e_topological_mode topological_mode;
};


/* Contains path enumeration & probability analysis results */
class Analysis_Results{
public:
	/* mutex for threads wishing to add to the results */
	pthread_mutex_t thread_mutex;
	pthread_barrier_t thread_barrier;
	int active_threads;

	/* maximum possible total weighted probability is ALL connections have a 100% chance of routing (used to normalize analysis) */
	double max_possible_total_prob_drivers;
	double max_possible_total_prob_fanout;

	/* total weighted probability over all connections */
	double total_prob_drivers;
	double total_prob_fanout;

	/* used to analyze routability by looking at only x% worst possible (least routable) connections at
	   each length. the idea is that bad routability of a minor fraction of all connections is sufficient
	   to make an architecture unroutable */
	vector< t_lowest_probs_pq > lowest_probs_pqs_drivers;	/* for paths enumerated from drivers (i.e. regular sources + opins) */
	vector< t_lowest_probs_pq > lowest_probs_pqs_fanout;	/* for paths enumerated for fanout purposes (via virtual sources */


	/* total number of connections that we WANT to analyze */
	int desired_conns;
	/* total number of connections that we ACTUALLY analyzed (maybe some connections were unroutable so we just couldn't enumerate paths from them, etc) */
	int num_conns;

	/* constructor to initialize constituent variables to 0 */
	Analysis_Results(){

		this->max_possible_total_prob_drivers = 0;
		this->max_possible_total_prob_fanout = 0;
		this->total_prob_drivers = 0;
		this->total_prob_fanout = 0;
		this->desired_conns = 0;
		this->num_conns = 0;
	}
};


/************ File-Scope Variables ************/
/* Structure containing relevant results for path enumeration and routability analysis.
   It can be written to by different threads with the help of the thread_mutex member variable */
static Analysis_Results f_analysis_results = Analysis_Results();


/************ Function Declarations ************/
/* performs routability analysis on an FPGA architecture */
static void analyze_fpga_architecture(User_Options *user_opts, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs, 
			Routing_Structs *routing_structs);

/* performs routability analysis on a simple one-source/one-sink graph */
static void analyze_simple_graph(User_Options *user_opts, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs, 
			Routing_Structs *routing_structs);

/* enumerates paths from test tiles */
void analyze_test_tile_connections(User_Options *user_opts, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs, 
			Routing_Structs *routing_structs, e_topological_mode topological_mode);

/* fills an initially-empty vector with the sink indices to which the source at the specified tile coordinate should connect */
static void get_corresponding_sink_ids(User_Options *user_opts, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs, Routing_Structs *routing_structs,
		int source_node_ind, Coordinate tile_coord, vector<int> &sink_indices, vector<int> &ss_length, vector<int> &source_conns_at_length);

/* launched the specified number of threads to perform path enumeration */
void launch_pthreads(t_thread_conn_info &thread_conn_info, t_threads &threads, int num_threads);

/* enumerate paths from specified node at specified tile.  */
void* enumerate_paths_from_source( void *ptr );

/* allocates source/sink distance vector for each thread */
void alloc_thread_ss_distances(t_thread_ss_distances &thread_ss_distances, int num_threads, int num_nodes);

/* allocates node topological traversal info vector for each thread */
void alloc_thread_node_topo_inf(t_thread_node_topo_inf &thread_node_topo_inf, int num_threads, int max_path_weight_bound, t_rr_node &rr_node, int num_nodes);

/* allocates a t_nodes_visited structure for each thread */
void alloc_thread_nodes_visited(t_thread_nodes_visited &thread_nodes_visited, int num_threads, int num_nodes);

/* allocates a Enumerate_Conn_Info structure for each thread */
void alloc_thread_conn_info(t_thread_conn_info &thread_conn_info, int num_threads);

/* allocates a pthread_t entry for each thread */
void alloc_threads( t_threads &threads, int num_threads );

/* analyzes specified connection between source/sink by calling the 'analyze_connection' function. other than that, 
   this function also computes scaling factors necessary for the call to 'analyze_connection', and updates probability
   metrics as necessary */
static void analyze_connection(int source_node_ind, int sink_node_ind, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs,
			Routing_Structs *routing_structs, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, int conn_length,
			int number_conns_at_length, t_nodes_visited &nodes_visited, e_topological_mode topological_mode, User_Options *user_opts);

/* Enumerates paths between specified source/sink nodes. */
void enumerate_connection_paths(int source_node_ind, int sink_node_ind, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs,
			Routing_Structs *routing_structs, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, int conn_length,
			t_nodes_visited &nodes_visited, User_Options *user_opts, float scaling_factor_for_enumerate);

/* Estimates the likelyhood (based on node demands) that the specified source/sink connection can be routed */
float estimate_connection_probability(int source_node_ind, int sink_node_ind, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs,
			Routing_Structs *routing_structs, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, int conn_length,
			t_nodes_visited &nodes_visited, User_Options *user_opts);

/* fills the t_ss_distances structures according to source & sink distances to intermediate nodes. 
   also returns an adjusted maximum path weight (to be further passed on to path enumeration / probability analysis functions)
   based on the distance from the source to the sink */
void get_ss_distances_and_adjust_max_path_weight(int source_node_ind, int sink_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances,
                                int max_path_weight, t_nodes_visited &nodes_visited, int *adjusted_max_path_weight, int *source_sink_dist);

/* adjusts maximum path weight based on the minimum distance of the current source/sink pair.
   note that the adjusted max path weight SHOULD stay in effect for only the current source/sink pair analysis
   and is reset afterwards. */
int adjust_max_path_weight_based_on_ss_dist(int min_dist_sink, int current_max_path_weight);

/* traverses graph from 'from_node_ind' and for each node traversed, sets distance to the source/sink node from
   which the traversal started (based on traversal_dir) */
void set_node_distances(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances,
			int max_path_weight, e_traversal_dir traversal_dir, t_nodes_visited &nodes_visited);

/* enqueues nodes belonging to specified edge list onto the bonded priority queue. the weight of the 
   enqueued nodes will be base_weight + their own weight */
void put_children_on_pq_and_set_ss_distance(int num_edges, int *edge_list, int base_weight, t_ss_distances &ss_distances,
			int max_path_weight, e_traversal_dir traversal_dir, t_rr_node &rr_node, int to_node_ind, My_Bounded_Priority_Queue<int> *PQ);

/* returns whether or not the specified node has a chance to reach the specified destination node */
bool node_has_chance_to_reach_destination(int node_ind, int destx, int desty, int node_path_weight, int max_path_weight, t_rr_node &rr_node);

/* does BFS over legal subraph from the 'from' node to the 'to' node and sets minimum number of hops
   required to arrive at each legal node from the 'from' node */
void set_node_hops(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances,
			int max_path_weight, e_traversal_dir traversal_dir);

/* resets data structures associated with nodes that have been visited during the previous path traversals */
void clean_node_data_structs(t_nodes_visited &nodes_visited, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, int max_path_weight);

/* clears ss_distances structure according to nodes that have been visited during graph traversal */
void clean_ss_distances(t_ss_distances &ss_distances, t_nodes_visited &nodes_visited);

/* clears node_buckets structure according to nodes that have been visited during graph traversal */
void clean_node_topo_inf(t_node_topo_inf &node_topo_inf, t_nodes_visited &nodes_visited, int max_path_weight);

/* returns the sum of pin probabilities over all the pins that the specified source node represents */
void get_sum_of_source_probabilities(int source_node_ind, t_rr_node &rr_node, t_prob_list &pin_probs,
				Physical_Type_Descriptor &fill_block_type, float *sum_probabilities, float *one_pin_prob);

/* returns number of sinks corresponding to the specified super-sink node */
int get_num_sinks(int sink_node_ind, t_rr_node &rr_node, Physical_Type_Descriptor &fill_block_type);
/* returns number of sources corresponding to the specified super-source node */
int get_num_sources(int source_node_ind, t_rr_node &rr_node, Physical_Type_Descriptor &fill_block_type);

/* function for a thread to increment the probability metric */
void increment_probability_metric(float probability_increment, int connection_length, int source_node_ind, int sink_node_ind,
				int num_subsources, int num_subsinks, e_pin_type source_pin_type);
/* returns the number of CHANX/CHANY nodes in the graph */
static int get_num_routing_nodes(t_rr_node &rr_node);
/* returns a 'reachability' metric based on routing node demands */
static float node_demand_metric(User_Options *user_opts, t_rr_node &rr_node);
/* returns from_x/to_x/from_y/to_y iteration limits of a 'core' FPGA region, according to CORE_OFFSET */
static void get_prob_analysis_tile_region(User_Options *user_opts, int grid_size_x, int grid_size_y, int *from_x, int *from_y, int *to_x, int *to_y);
/* currently returns the total number of connections at each connection length <= maximum connection length */
static void get_conn_length_stats(User_Options *user_opts, Analysis_Settings *analysis_settings, Routing_Structs *routing_structs, 
                        Arch_Structs *arch_structs, e_pin_type enumerate_type, vector<int> &conns_at_length);
/* returns number of connections from tile at the specified coordinates at specified length */
static int conns_at_distance_from_tile(int tile_x, int tile_y, int length, t_grid &grid, 
				int grid_size_x, int grid_size_y, t_block_type &block_type, int fill_type_ind);
/* at each length, sums the probabilities of the x% worst possible connections */
static float analyze_lowest_probs_pqs( vector< t_lowest_probs_pq > &lowest_probs_pqs);


/************ Function Definitions ************/
/* the entry function to performing routability analysis */
void run_analysis(User_Options *user_opts, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs, 
			Routing_Structs *routing_structs){

	switch( user_opts->rr_structs_mode ){
		case RR_STRUCTS_VPR:
			analyze_fpga_architecture(user_opts, analysis_settings, arch_structs, routing_structs);
			break;
		case RR_STRUCTS_SIMPLE:
			analyze_simple_graph(user_opts, analysis_settings, arch_structs, routing_structs);
			break;
		default:
			WTHROW(EX_PATH_ENUM, "Encountered unrecognized rr_structs_mode: " << user_opts->rr_structs_mode); 
	}
}

/* performs routability analysis on an FPGA architecture */
static void analyze_fpga_architecture(User_Options *user_opts, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs, 
			Routing_Structs *routing_structs){

	srand(3);

	vector<int> driver_conns_at_length;
	vector<int> receiver_conns_at_length;

	/* create the lowest probability priority queues (for pessimistic routability analysis of some percentile of worst connections at each length) */
	f_analysis_results.lowest_probs_pqs_drivers.assign( user_opts->max_connection_length+1, t_lowest_probs_pq() );
	f_analysis_results.lowest_probs_pqs_fanout.assign( user_opts->max_connection_length+1, t_lowest_probs_pq() );
	get_conn_length_stats(user_opts, analysis_settings, routing_structs, arch_structs, DRIVER, driver_conns_at_length);	//for paths enumerated *from* sources
	get_conn_length_stats(user_opts, analysis_settings, routing_structs, arch_structs, RECEIVER, receiver_conns_at_length);	//for paths enumerated *from* sinks (for fanout stuff)
	for(int ilen = 0; ilen < user_opts->max_connection_length+1; ilen++){
		/* set the bounded priority queue entries limit w.r.t. to the "..._conns_at_length" stats */
		if (driver_conns_at_length[ilen] > 0){
			//int driver_entries_limit = driver_conns_at_length[ilen] * WORST_ROUTABILITY_PERCENTILE_DRIVERS * FRACTION_CONNS;		//XXX
			int driver_entries_limit = driver_conns_at_length[ilen] * WORST_ROUTABILITY_PERCENTILE_DRIVERS * user_opts->length_probabilities[ilen];
			cout << "len" << ilen << " entries " << driver_entries_limit << endl;
			f_analysis_results.lowest_probs_pqs_drivers[ilen].set_properties( driver_entries_limit );
		}
		if (receiver_conns_at_length[ilen] > 0){
			int receiver_entries_limit = receiver_conns_at_length[ilen] * WORST_ROUTABILITY_PERCENTILE_FANOUT;
			f_analysis_results.lowest_probs_pqs_fanout[ilen].set_properties( receiver_entries_limit );
		}
	}

	analyze_test_tile_connections(user_opts, analysis_settings, arch_structs, routing_structs, ENUMERATE);

	analyze_test_tile_connections(user_opts, analysis_settings, arch_structs, routing_structs, PROBABILITY);

	cout << "demand contributions " << f_demand_contributions << endl;

	update_screen(routing_structs, arch_structs, user_opts);
}

/* performs routability analysis on a simple one-source/one-sink graph */
static void analyze_simple_graph(User_Options *user_opts, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs, 
			Routing_Structs *routing_structs){

	t_rr_node &rr_node = routing_structs->rr_node;
	int num_rr_nodes = routing_structs->get_num_rr_nodes();

	int source_node_ind = UNDEFINED;
	int sink_node_ind = UNDEFINED;

	int large_connection_length = 1000;
	int large_max_path_weight = 1000;

	/* figure out which node is the source and which node is the sink */
	for (int inode = 0; inode < num_rr_nodes; inode++){
		e_rr_type node_type = rr_node[inode].get_rr_type();

		/* if this node is a source or sink, record the corresponding node index.
		   currently only one source and one sink node is allowed for this 'simple graph' analysis, so if 
		   more than one source or sink exists, throw exception */
		if (node_type == SOURCE){
			if (source_node_ind == UNDEFINED){
				source_node_ind = inode;
			} else {
				WTHROW(EX_PATH_ENUM, "Expected to only find one source node");
			}
		} else if (node_type == SINK){
			if (sink_node_ind == UNDEFINED){
				sink_node_ind = inode;
			} else {
				WTHROW(EX_PATH_ENUM, "Expected to only find one sink node");
			}
		} else {
			/* nothing */
		}
	}

	/* allocate structures for getting source/sink distances */
	t_nodes_visited nodes_visited;
	nodes_visited.reserve(num_rr_nodes);
	t_ss_distances ss_distances;
	ss_distances.assign(num_rr_nodes, SS_Distances());

	/* allocate structures for topological traversal */
	t_node_topo_inf node_topo_inf;
	node_topo_inf.assign(num_rr_nodes, Node_Topological_Info());
	for (int inode = 0; inode < num_rr_nodes; inode++){
		node_topo_inf[inode].buckets.alloc_source_sink_buckets(large_max_path_weight+1, large_max_path_weight+1);
		node_topo_inf[inode].demand_discounts.assign(large_max_path_weight+1, 0.0);
		rr_node[inode].alloc_child_demand_contributions(large_max_path_weight+1);
	}

	/* perform path enumeration */
	enumerate_connection_paths(source_node_ind, sink_node_ind, analysis_settings, arch_structs, routing_structs, ss_distances,
	                     node_topo_inf, large_connection_length, nodes_visited, user_opts, (float)UNDEFINED);

	/* print how many paths run through each node */
	cout << "Node paths: " << endl;
	for (int inode = 0; inode < num_rr_nodes; inode++){
		e_rr_type rr_type = rr_node[inode].get_rr_type();
		int node_weight = rr_node[inode].get_weight();
		int node_dist_to_source = ss_distances[inode].get_source_distance();

		int num_node_paths = node_topo_inf[inode].buckets.get_num_paths(node_weight, node_dist_to_source, large_max_path_weight);

		cout << inode << ": " << g_rr_type_string[rr_type] << ", " << num_node_paths << " paths" << endl;
	}


	/* clean structures in preparation for probability estimation */
	clean_node_data_structs(nodes_visited, ss_distances, node_topo_inf, large_max_path_weight);

	/* estimate probability of routing from source to sink */
	float connection_probability = estimate_connection_probability(source_node_ind, sink_node_ind, analysis_settings, arch_structs,
	                                                   routing_structs, ss_distances, node_topo_inf, large_connection_length,
							   nodes_visited, user_opts);

	/* print connection probability */
	cout << "Connection probability: " << connection_probability << endl;
}

/* enumerates paths from test tiles. typically path eneumeration would involve enumerating paths from		//TODO: outdated comment
   sources to sinks, but there are a few caveats that can't be easily intuited. specifically:
	- A source or sink *node* is, in actually a super-source or super-sink; it is actually a collection of 
	  sources (sinks) bundled together. This collection of sources (sinks) has an implicit crossbar
	  structure connecting them to pins, and in the case of opins the probabilities defined for opins
	  are actually the probabilities for those individual sources internal to the logic block. I
	  assume that thre is a source for each opin and a sink for each ipin -- in reality this is not
	  necessarily the case.
		- this means that when we enumerate paths from a source node to a sink node, the number
		  of enumerated paths have to be scaled up to account for the actual number of sources/sinks
		  represented by said nodes
		- *ideally* there would actually be individual sources/sinks and explicit crossbar structures
		  to connect them to pins (or to do internal feedback paths in a logic block). maybe this will
		  happen in the future.
	- Enumerating paths from ipins is Wotan's way of accounting for fanout. However in the current graph
	  (read through VPR) there are no sources attached to ipins, and even if said sources could be attached,
	  they would not fit into the pin-track-class scheme used by the rr node indices structure. So routing
	  from ipins is actually a bit of a hack. */
void analyze_test_tile_connections(User_Options *user_opts, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs, 
			Routing_Structs *routing_structs, e_topological_mode topological_mode){

	int fill_type_ind = arch_structs->get_fill_type_index();
	Physical_Type_Descriptor *fill_type = &arch_structs->block_type[fill_type_ind];
	int grid_size_x, grid_size_y;
	arch_structs->get_grid_size(&grid_size_x, &grid_size_y);

	string fill_type_name = fill_type->get_name();

	cout << "Enumerating paths for physical block type named '" << fill_type->get_name() << "'" << endl;

	/* allocate appropriate data structures for each thread */
	int max_path_weight_bound = analysis_settings->get_max_path_weight( user_opts->max_connection_length ) * PATH_FLEXIBILITY_FACTOR;
	int num_threads = user_opts->num_threads;
	t_thread_ss_distances thread_ss_distances;
	t_thread_node_topo_inf thread_node_topo_inf;
	t_thread_nodes_visited thread_nodes_visited;
	t_thread_conn_info thread_conn_info;
	t_threads threads;

	cout << "absolute max possible path weight is: " << max_path_weight_bound << endl;

	alloc_thread_ss_distances(thread_ss_distances, num_threads, (int)routing_structs->get_num_rr_nodes());
	alloc_thread_node_topo_inf(thread_node_topo_inf, num_threads, max_path_weight_bound, routing_structs->rr_node, (int)routing_structs->get_num_rr_nodes());
	alloc_thread_nodes_visited(thread_nodes_visited, num_threads, (int)routing_structs->get_num_rr_nodes());
	alloc_thread_conn_info(thread_conn_info, num_threads);
	alloc_threads(threads, num_threads);

	/* set parameters that will not change for each thread */
	for (int ithread = 0; ithread < num_threads; ithread++){
		thread_conn_info[ithread].user_opts = user_opts;
		thread_conn_info[ithread].analysis_settings = analysis_settings;
		thread_conn_info[ithread].arch_structs = arch_structs;
		thread_conn_info[ithread].routing_structs = routing_structs;
		thread_conn_info[ithread].ss_distances = &thread_ss_distances[ithread];
		thread_conn_info[ithread].node_topo_inf = &thread_node_topo_inf[ithread];
		thread_conn_info[ithread].nodes_visited = &thread_nodes_visited[ithread];
		thread_conn_info[ithread].topological_mode = topological_mode;
	}

	int ithread_source = 0;
	int ithread_sink = 0;
	/* for each test tile */
	vector< Coordinate >::const_iterator it;
	for (it = analysis_settings->test_tile_coords.begin(); it != analysis_settings->test_tile_coords.end(); it++){

		Coordinate tile_coord = (*it);

		/* the user may have specified that only the core region of the FPGA is to be used for probability analysis. in that case
		   probability analysis will be performed for all tiles that are within the region that is CORE_OFFSET tiles from the FPGA perimeter */
		if (user_opts->analyze_core){
			if (topological_mode == PROBABILITY){
				int from_x, to_x, from_y, to_y;
				get_prob_analysis_tile_region(user_opts, grid_size_x, grid_size_y, &from_x, &from_y, &to_x, &to_y);

				if (tile_coord.x < from_x || tile_coord.x > to_x || tile_coord.y < from_y || tile_coord.y > to_y){
					continue;
				}
			}
		}


		Grid_Tile *test_tile = &arch_structs->grid[tile_coord.x][tile_coord.y];
		Physical_Type_Descriptor *tile_type= &arch_structs->block_type[ test_tile->get_type_index() ];

		/* for each source of the test tile */
		for (int iclass = 0; iclass < (int)tile_type->class_inf.size(); iclass++){
			Pin_Class *pin_class = &tile_type->class_inf[iclass];

			if (pin_class->get_pin_type() == DRIVER){
				/* enumerating from opins basically involves enumerating from the corresponding
				   source */

				int source_node_index = routing_structs->rr_node_index[SOURCE][tile_coord.x][tile_coord.y][iclass];

				vector<int> sink_indices;
				vector<int> ss_length;
				vector<int> source_conns_at_length;
				get_corresponding_sink_ids(user_opts, analysis_settings, arch_structs, routing_structs, source_node_index, tile_coord, sink_indices,
								ss_length, source_conns_at_length);

				for (int isink = 0; isink < (int)sink_indices.size(); isink++){
					Source_Sink_Pair ss_pair;
					ss_pair.source_ind = source_node_index;
					ss_pair.sink_ind = sink_indices[isink];
					ss_pair.ss_length = ss_length[isink];
					ss_pair.source_conns_at_length = source_conns_at_length[isink];
					
					thread_conn_info[ithread_source].source_sink_pairs.push_back(ss_pair);
				}

				ithread_source++;	
				if (ithread_source == num_threads){
					ithread_source = 0;
				}

			} else if (pin_class->get_pin_type() == RECEIVER){
				/* enumerating from ipins is Wotan's way of accounting for fanout and this is slightly trickier.
				   in wotan_init.cxx virtual sources were created for every sink and attached into the wires 
				   that connect into the sink's ipins. these virtual sources are used to enumerate fanout paths */

				int sink_node_index = routing_structs->rr_node_index[SOURCE][tile_coord.x][tile_coord.y][iclass];
				int virtual_source_ind = routing_structs->rr_node[sink_node_index].get_virtual_source_node_ind();

				vector<int> sink_indices;
				vector<int> ss_length;
				vector<int> source_conns_at_length;
				get_corresponding_sink_ids(user_opts, analysis_settings, arch_structs, routing_structs, virtual_source_ind, tile_coord, sink_indices,
								ss_length, source_conns_at_length);

				for (int isink = 0; isink < (int)sink_indices.size(); isink++){
					Source_Sink_Pair ss_pair;
					ss_pair.source_ind = virtual_source_ind;
					ss_pair.sink_ind = sink_indices[isink];
					ss_pair.ss_length = ss_length[isink];
					ss_pair.source_conns_at_length = source_conns_at_length[isink];

					thread_conn_info[ithread_sink].source_sink_pairs.push_back(ss_pair);
				}

				ithread_sink++;
				if (ithread_sink == num_threads){
					ithread_sink = 0;
				}
			} else {
				WTHROW(EX_PATH_ENUM, "Unexpected pin type: " << pin_class->get_pin_type());
			}
		}
	}

	f_analysis_results.active_threads = num_threads;
	/* initialize mutex that will be used for synchronizing threads' updates to shared variables */
	pthread_mutex_init(&f_analysis_results.thread_mutex, NULL);
	/* initialize thread semaphore */
	pthread_barrier_init(&f_analysis_results.thread_barrier, 0, f_analysis_results.active_threads);

	/* launch the threads */
	launch_pthreads(thread_conn_info, threads, num_threads);

	pthread_mutex_destroy(&f_analysis_results.thread_mutex);
	pthread_barrier_destroy(&f_analysis_results.thread_barrier);


	
	/* calculate metrics */

	double total_demand = 0;
	double squared_demand = 0;

	/* calculate some node demand-related metrics */
	int num_nodes = routing_structs->get_num_rr_nodes();
	int num_routing_nodes = 0;
	for (int inode = 0; inode < num_nodes; inode++){
		RR_Node &node = routing_structs->rr_node[inode];
		e_rr_type type = node.get_rr_type();
		if (type == CHANX || type == CHANY){
				double demand = node.get_demand(user_opts);
				//cout << " n" << inode << " demand: " << demand << endl;
				total_demand += demand;
				squared_demand += demand*demand;

				num_routing_nodes++;
		}
	}
	
	/* end of analysis -- print results */
	if (topological_mode == ENUMERATE){
		cout << "desired conns: " << f_analysis_results.desired_conns << endl;
		cout << "enumerated: " << f_analysis_results.num_conns << endl;

		float normalized_demand = node_demand_metric(user_opts, routing_structs->rr_node);
		cout << "fraction enumerated: " << (float)f_analysis_results.num_conns / (float)f_analysis_results.desired_conns << endl;
		cout << "Total demand: " << total_demand << endl;
		cout << "Total squared demand: " << squared_demand << endl;
		cout << "Normalized demand: " << normalized_demand << endl; //total_demand / (double)num_routing_nodes << endl;
		cout << "  num routing nodes: " << num_routing_nodes << endl;
		cout << "Normalized squared demand: " << squared_demand / (double)num_routing_nodes << endl;
		cout << endl;
	} else {
		float opin_prob = user_opts->opin_probability;
		float ipin_prob = user_opts->ipin_probability;

		cout << "Demand multiplier: " << user_opts->demand_multiplier << endl;

		cout.setf(ios::fixed);
		cout.precision(4);

		/* compute the two parts of the routability metric (driver & fanout) */
		float worst_probabilities_driver = 0;
		float worst_probabilities_fanout = 0;
		float driver_prob_metric = 0;
		float fanout_prob_metric = 0;
		if (opin_prob != 0){
			worst_probabilities_driver = analyze_lowest_probs_pqs( f_analysis_results.lowest_probs_pqs_drivers );
			driver_prob_metric = worst_probabilities_driver / (f_analysis_results.max_possible_total_prob_drivers * WORST_ROUTABILITY_PERCENTILE_DRIVERS);
		}

		if (ipin_prob != 0){
			worst_probabilities_fanout = analyze_lowest_probs_pqs( f_analysis_results.lowest_probs_pqs_fanout );
			fanout_prob_metric = worst_probabilities_fanout / (f_analysis_results.max_possible_total_prob_fanout * WORST_ROUTABILITY_PERCENTILE_FANOUT);
		}

		cout << "Driver metric: " << driver_prob_metric << endl;
		cout << "Fanout metric: " << fanout_prob_metric << endl;

		/* combine the two parts of the routability metric into a single number */
		float driver_prob_weight = 1;
		float fanout_prob_weight = 1;
		if (opin_prob > 0 && ipin_prob > 0){
			driver_prob_weight = DRIVER_PROB_WEIGHT;
			fanout_prob_weight = FANOUT_PROB_WEIGHT;
		}
		
		float routability_metric = (driver_prob_weight * driver_prob_metric) + (fanout_prob_weight * fanout_prob_metric);

		cout << "Routability metric: " << routability_metric << endl;
	}

	malloc_trim(0);
}


/* fills an initially-empty vector with the sink indices to which the source at the specified tile coordinate should connect */
static void get_corresponding_sink_ids(User_Options *user_opts, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs, Routing_Structs *routing_structs,
		int source_node_ind, Coordinate tile_coord, vector<int> &sink_indices, vector<int> &ss_length, vector<int> &source_conns_at_length){

	if (sink_indices.size() != 0){
		WTHROW(EX_PATH_ENUM, "Sink indices vector must initially be empty");
	}

	t_grid &grid = arch_structs->grid;
	t_block_type &block_type = arch_structs->block_type;
	int grid_size_x, grid_size_y;
	arch_structs->get_grid_size(&grid_size_x, &grid_size_y);

	Grid_Tile *test_tile = &grid[tile_coord.x][tile_coord.y];

	Physical_Type_Descriptor *test_tile_type = &block_type[test_tile->get_type_index()];

	/* get pin and length probabilities */
	t_prob_list &length_prob = analysis_settings->length_probabilities;

	/* check probability of source node. if it's 0, then no point in enumerating from it */
	float sum_of_source_probabilities;
	get_sum_of_source_probabilities(source_node_ind, routing_structs->rr_node, analysis_settings->pin_probabilities, *test_tile_type,
				&sum_of_source_probabilities, NULL);
	if (sum_of_source_probabilities == 0){
		return;
	}

	/* make sure specified tile is of 'fill' type */
	int fill_type_ind = arch_structs->get_fill_type_index();
	if (fill_type_ind != test_tile->get_type_index()){
		WTHROW(EX_PATH_ENUM, "Attempting to analyze source in a block that's not of fill type.");
	}
	
	/* make sure the current grid tile is not at an offset */
	if (test_tile->get_width_offset() != 0 || test_tile->get_height_offset() != 0){
		WTHROW(EX_PATH_ENUM, "Fill type block with name '" << test_tile_type->get_name() << "' has non-zero width/height offset. " <<
				"This sort of logic block is not currently allowed.");
	}

	/* make sure the test tile has blocks at each possible connection length away from it. the furthest block from the test tile
	   is basically the distance to the farthest legal corner of the FPGA */
	int max_conn_length = user_opts->max_connection_length;
	/* offset from perimeter because we don't want I/O blocks */
	int max_block_dist = max( tile_coord.get_dx_plus_dy(1,1), tile_coord.get_dx_plus_dy(1, grid_size_y-2) );
	max_block_dist = max( max_block_dist, tile_coord.get_dx_plus_dy(grid_size_x-2, grid_size_y-2) );
	max_block_dist = max( max_block_dist, tile_coord.get_dx_plus_dy(grid_size_x-2, 1) );

	if (max_block_dist < max_conn_length){
		WTHROW(EX_PATH_ENUM, "It is not possible to connect test tile at coordinate " << tile_coord << 
				     " to any blocks a manhattan distance " << max_conn_length << " away");
	}


	/* get sinks at neighboring tiles */
	for (int ilen = 1; ilen <= max_conn_length; ilen++){
		if (length_prob[ilen] == 0){
			continue;
		}

		int num_conns_at_length = conns_at_distance_from_tile(tile_coord.x, tile_coord.y, ilen, grid,
					      grid_size_x, grid_size_y, block_type, fill_type_ind);

		/* traverse a list of blocks that is a distance 'ilen' away from the test tile.
		   here we want to consider each combination of dx and dy who's (individually absolute) sum adds up
		   to ilen */
		for (int idx = -ilen; idx <= ilen; idx++){
			int y_distance = ilen - abs(idx);
			for (int idy = -y_distance; idy <= y_distance; idy += max(2*y_distance, 1)){	//max() in case y_distance=0
				int dest_x = tile_coord.x + idx;
				int dest_y = tile_coord.y + idy;
				

				/* check if this block is within grid bounds */
				if ( (dest_x > 0 && dest_x < grid_size_x-1) &&
				     (dest_y > 0 && dest_y < grid_size_y-1) ){

					Grid_Tile *dest_tile = &grid[dest_x][dest_y];
					int dest_type_ind = dest_tile->get_type_index();


					Physical_Type_Descriptor *dest_type = &block_type[dest_type_ind];

					/* iterate over each pin class*/
					for (int iclass = 0; iclass < (int)dest_type->class_inf.size(); iclass++){
						Pin_Class *pin_class = &dest_type->class_inf[iclass];
						
						/* only want classes that represent receiver pins. also must actually have pins */
						if (pin_class->get_pin_type() != RECEIVER || pin_class->get_num_pins() == 0){
							continue;
						}

						/* do not want global pins */
						int sample_pin = pin_class->pinlist[0];
						if (dest_type->is_global_pin[sample_pin]){
							continue;
						}

						/* get node corresponding to this sink */	
						int sink_node_ind = routing_structs->rr_node_index[SINK][dest_x][dest_y][iclass];

						//XXX
						double rand_value = (double)rand() / (double)(RAND_MAX);
						//if (rand_value > FRACTION_CONNS){
						if (rand_value > user_opts->length_probabilities[ilen]){
							continue;
						}

						sink_indices.push_back( sink_node_ind );
						ss_length.push_back( ilen );
						source_conns_at_length.push_back( num_conns_at_length );

						f_analysis_results.desired_conns++;
					}
				}
			}
		}
	}
}


/* returns the number of CHANX/CHANY nodes in the graph */
static int get_num_routing_nodes(t_rr_node &rr_node){
	int num_routing_nodes = 0;

	int num_nodes = (int)rr_node.size();
	for (int inode = 0; inode < num_nodes; inode++){
		e_rr_type node_type = rr_node[inode].get_rr_type();
		if (node_type == CHANX || node_type == CHANY){
			num_routing_nodes++;
		}
	}
	return num_routing_nodes;
}

/* returns a 'reachability' metric based on routing node demands */
static float node_demand_metric(User_Options *user_opts, t_rr_node &rr_node){
	int num_nodes = (int)rr_node.size();
	int num_routing_nodes = get_num_routing_nodes(rr_node);
	int node_num_limit = num_routing_nodes * 0.05;

	if (node_num_limit <= 0){
		WTHROW(EX_PATH_ENUM, "Asked to analyze demand of <= 0 nodes...");
	}

	//set< pair<float, int> > analysis_nodes;
	My_Fixed_Size_PQ< float, greater<float> > analysis_nodes( node_num_limit );

	/* go over each node and put it into the analysis_nodes set. if the size of the set grows > node_num_limit,
	   pop off nodes with lowest demands accordingly. so this is basically a priority queue with a fixed number
	   of elements */
	for (int inode = 0; inode < num_nodes; inode++){
		RR_Node &node = rr_node[inode];
		e_rr_type node_type = node.get_rr_type();

		/* only want to record demand for routing nodes (of CHANX/CHANY type) */
		if (node_type != CHANX && node_type != CHANY){
			continue;
		}

		analysis_nodes.push( node.get_demand(user_opts) );
	}

	/* now we have a set of x% largest-demand nodes. add up that demand */
	int num_elements = analysis_nodes.size();
	float summed_demand = 0;
	for (int i = 0; i < num_elements; i++){
		float node_demand = analysis_nodes.top();
		summed_demand += node_demand;
		//cout << " demand " << node_demand << endl;
		analysis_nodes.pop();
	}

	float normalized_demand = summed_demand / (float)num_elements;

	return normalized_demand;
}

/* launched the specified number of threads to perform path enumeration */
void launch_pthreads(t_thread_conn_info &thread_conn_info, t_threads &threads, int num_threads){

	/* create num_threads-1 threads (the remaining thread is executed in the current context) */
	for (int ithread = 0; ithread < num_threads-1; ithread++){
		/* create pthread with default attributes */
		int result = pthread_create(&threads[ithread], NULL, enumerate_paths_from_source, (void*) &thread_conn_info[ithread]);
		if (result != 0){
			WTHROW(EX_PATH_ENUM, "Failed to create thread!");
		}
	}

	/* the last thread is launched here */
	enumerate_paths_from_source( (void*) &thread_conn_info[num_threads-1] );

	/* wait for threads to complete */
	for (int ithread = 0; ithread < num_threads-1; ithread++){
		int result = pthread_join(threads[ithread], NULL);
		if (result != 0){
			WTHROW(EX_PATH_ENUM, "Failed to join thread!");
		}
	}
}


/* enumerate paths from specified node at specified tile.  */
void* enumerate_paths_from_source( void *ptr ){

	Conn_Info *conn_info = (Conn_Info*)ptr;
	
	vector<Source_Sink_Pair> &source_sink_pairs = conn_info->source_sink_pairs;
	User_Options *user_opts = conn_info->user_opts;
	Analysis_Settings *analysis_settings = conn_info->analysis_settings;
	Arch_Structs *arch_structs = conn_info->arch_structs;
	Routing_Structs *routing_structs = conn_info->routing_structs;
	t_ss_distances &ss_distances = (*conn_info->ss_distances);
	t_node_topo_inf &node_topo_inf = (*conn_info->node_topo_inf);
	t_nodes_visited &nodes_visited = (*conn_info->nodes_visited);
	e_topological_mode topological_mode = conn_info->topological_mode;


	int connections_done = 0;

	random_shuffle(source_sink_pairs.begin(), source_sink_pairs.end());

	for (int ipair = 0; ipair < (int)source_sink_pairs.size(); ipair++){
		Source_Sink_Pair ss_pair = source_sink_pairs[ipair];
		int source_node_ind = ss_pair.source_ind;
		int sink_node_ind = ss_pair.sink_ind;
		int ss_length = ss_pair.ss_length;
		int source_conns_at_length = ss_pair.source_conns_at_length;

		/* analyze this source/sink connection */
		analyze_connection(source_node_ind, sink_node_ind, analysis_settings, arch_structs, 
					routing_structs, ss_distances, node_topo_inf, ss_length, 
					source_conns_at_length, nodes_visited, topological_mode, user_opts);




		///***** WEIRD THREAD SYNCHRONIZATION -- Updating Node Weights *****/
		///* threads will exit at the same time when the active thread count drops to 0 */
		//bool thread_done = false;
		//if (ipair == (int)source_sink_pairs.size()-1){
		//	thread_done = true;
		//	pthread_mutex_lock(&f_analysis_results.thread_mutex);
		//	f_analysis_results.active_threads--;
		//	pthread_mutex_unlock(&f_analysis_results.thread_mutex);
		//}

		///* do-while makes sure that all threads exit at the same time; otherwise pthread barrier will get messed up */
		//do{

		//	/* Node weights are updated after each thread goes through a certain number of connections */
		//	if (connections_done == 300 || thread_done){

		//		pthread_barrier_wait(&f_analysis_results.thread_barrier);

		//		/* update node weights based on their demands; synchronize between threads */
		//		if (!pthread_mutex_trylock(&f_analysis_results.thread_mutex)){
		//			/* this thread will do the node weight updates */				
		//	
		//			for (int inode = 0; inode < routing_structs->get_num_rr_nodes(); inode++){
		//				routing_structs->rr_node[inode].set_weight();
		//			}

		//			pthread_mutex_unlock(&f_analysis_results.thread_mutex);
		//		} else {
		//			/* all other threads synchronize here */
		//			pthread_mutex_lock(&f_analysis_results.thread_mutex);
		//			pthread_mutex_unlock(&f_analysis_results.thread_mutex);
		//		}
		//		connections_done = 0;

		//	}

		//	connections_done++;

		//} while (thread_done && f_analysis_results.active_threads != 0);
	}

	return (void*) NULL;
}


/* returns from_x/to_x/from_y/to_y iteration limits (inclusive) of a 'core' FPGA region that is CORE_OFFSET tiles away from the FPGA perimeter */
static void get_prob_analysis_tile_region(User_Options *user_opts, int grid_size_x, int grid_size_y, int *from_x, int *from_y, int *to_x, int *to_y){

	if (user_opts->analyze_core){
		*from_x = CORE_OFFSET;
		*to_x = grid_size_x - 1 - CORE_OFFSET;
		*from_y = CORE_OFFSET;
		*to_y = grid_size_y - 1 - CORE_OFFSET;
	} else {
		/* if user has not enabled analysis of the core region, return normal iteration limits of the FPGA tiles (which exclude the perimeter IOs) */
		*from_x = 1;
		*to_x = grid_size_x-2;
		*from_y = 1;
		*to_y = grid_size_y-2; 
	}
}


/* currently returns the total number of connections at each connection length <= maximum connection length */
static void get_conn_length_stats(User_Options *user_opts, Analysis_Settings *analysis_settings, Routing_Structs *routing_structs, 
                        Arch_Structs *arch_structs, e_pin_type enumerate_type, vector<int> &conns_at_length){

	int max_conn_length = user_opts->max_connection_length;
	t_grid &grid = arch_structs->grid;
	t_block_type &block_type = arch_structs->block_type;

	int grid_size_x, grid_size_y;
	arch_structs->get_grid_size(&grid_size_x, &grid_size_y);

	int fill_type_ind = arch_structs->get_fill_type_index();
	Physical_Type_Descriptor *fill_type = &block_type[fill_type_ind];

	/* 0..max_conn_length possible connection lengths */
	conns_at_length.assign(max_conn_length + 1, 0);

	/* determine the iteration limits for the region of the FPGA which we want to analyze */
	int from_x, to_x, from_y, to_y;
	get_prob_analysis_tile_region(user_opts, grid_size_x, grid_size_y, &from_x, &from_y, &to_x, &to_y);

	/* calculate number of sources in a tile of fill type */
	int num_tile_pins = fill_type->get_num_pins();
	int num_tile_sources = 0;
	for (int ipin = 0; ipin < num_tile_pins; ipin++){
		/* skip global pins */
		if (fill_type->is_global_pin[ipin]){
			continue;
		}

		/* get type of this pin */
		int pin_class_ind = fill_type->pin_class[ipin];
		e_pin_type pin_type = fill_type->class_inf[pin_class_ind].get_pin_type();

		if (pin_type == enumerate_type){
			/* get probabiity of this pin being used as a source */
			float pin_prob = analysis_settings->pin_probabilities[ipin];
			if (pin_prob > 0){
				num_tile_sources++;
			}
		}
	}

	/* iterate over the FPGA tiles as per 'get_prob_analysis_tile_region' */
	for (int ix = from_x; ix <= to_x; ix++){
		for (int iy = from_y; iy <= to_y; iy++){
			int block_type_ind = grid[ix][iy].get_type_index();
			int width_offset = grid[ix][iy].get_width_offset();
			int height_offset = grid[ix][iy].get_height_offset();

			/* error checks */
			if (block_type_ind != fill_type_ind){
				WTHROW(EX_PATH_ENUM, "Expected logic block type");
			}
			if (width_offset > 0 || height_offset > 0){
				WTHROW(EX_PATH_ENUM, "Didn't expect logic block to have > 0 width/height offset");
			}

			/* for each legal length */
			for (int ilen = 1; ilen <= max_conn_length; ilen++){
				conns_at_length[ilen] += num_tile_sources * conns_at_distance_from_tile(ix, iy, ilen, grid, grid_size_x, grid_size_y,
				                                                                       block_type, fill_type_ind);
			}
		}
	}
}


/* returns number of connections from tile at the specified coordinates at specified length.
   this is basically a sum of the number of input pins for each tile 'length' away from this one */
static int conns_at_distance_from_tile(int tile_x, int tile_y, int length, t_grid &grid, 
				int grid_size_x, int grid_size_y, t_block_type &block_type, int fill_type_ind){

	int num_conns = 0;

	/* traverse a list of blocks that is a distance 'length' away from the test tile.
	   here we want to consider each combination of dx and dy who's (individually absolute) sum adds up
	   to length */
	for (int idx = -length; idx <= length; idx++){
		int y_distance = length - abs(idx);
		for (int idy = -y_distance; idy <= y_distance; idy += max(2*y_distance, 1)){	//max() in case y_distance=0
			int dest_x = tile_x + idx;
			int dest_y = tile_y + idy;

			/* check if this block is within grid bounds */
			if ( (dest_x > 0 && dest_x < grid_size_x-1) &&
			     (dest_y > 0 && dest_y < grid_size_y-1) ){

				Grid_Tile *dest_tile = &grid[dest_x][dest_y];
				int dest_type_ind = dest_tile->get_type_index();

				if (dest_type_ind != fill_type_ind){
					WTHROW(EX_PATH_ENUM, "Encountered block that isn't of fill type (i.e. not a logic block)");
				}

				int num_input_pins = block_type[dest_type_ind].get_num_receivers();

				num_conns += num_input_pins;
			}
		}
	}

	return num_conns;
}


/* allocates source/sink distance vector for each thread */
void alloc_thread_ss_distances(t_thread_ss_distances &thread_ss_distances, int num_threads, int num_nodes){
	thread_ss_distances.assign(num_threads, t_ss_distances(num_nodes, SS_Distances()));
}


/* allocates node topological traversal info vector for each thread */
void alloc_thread_node_topo_inf(t_thread_node_topo_inf &thread_node_topo_inf, int num_threads, int max_path_weight_bound, t_rr_node &rr_node, int num_nodes){
	thread_node_topo_inf.assign(num_threads, t_node_topo_inf(num_nodes, Node_Topological_Info()));
	for (int inode = 0; inode < num_nodes; inode++){
		for (int ithread = 0; ithread < num_threads; ithread++){
			thread_node_topo_inf[ithread][inode].buckets.alloc_source_sink_buckets(max_path_weight_bound+1, max_path_weight_bound+1);
			thread_node_topo_inf[ithread][inode].demand_discounts.assign(max_path_weight_bound+1, 0.0);
		}
		//FIXME: this should be in a separate function
		rr_node[inode].alloc_child_demand_contributions(max_path_weight_bound+1);
	}
}


/* allocates a t_nodes_visited structure for each thread */
void alloc_thread_nodes_visited(t_thread_nodes_visited &thread_nodes_visited, int num_threads, int num_nodes){
	thread_nodes_visited.assign(num_threads, t_nodes_visited());
	for (int ithread = 0; ithread < num_threads; ithread++){
		thread_nodes_visited[ithread].reserve(num_nodes);
	}
}


/* allocates a Enumerate_Conn_Info structure for each thread */
void alloc_thread_conn_info(t_thread_conn_info &thread_conn_info, int num_threads){
	thread_conn_info.assign(num_threads, Conn_Info());
}


/* allocates a pthread_t entry for each thread */
void alloc_threads( t_threads &threads, int num_threads ){
	threads.assign(num_threads, pthread_t());
}


/* analyzes specified connection between source/sink by calling the 'analyze_connection' function. other than that, 
   this function also computes scaling factors necessary for the call to 'analyze_connection', and updates probability
   metrics as necessary */
static void analyze_connection(int source_node_ind, int sink_node_ind, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs,
			Routing_Structs *routing_structs, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, int conn_length,
			int number_conns_at_length, t_nodes_visited &nodes_visited, e_topological_mode topological_mode, User_Options *user_opts){

	t_rr_node &rr_node = routing_structs->rr_node;

	/* get pin and length probabilities */
	float length_prob = analysis_settings->length_probabilities[conn_length];
	t_prob_list &pin_probs = analysis_settings->pin_probabilities;

	/* if the length probability corresponding to this connection is 0, then this connection won't contribute
	   anything to the routability metric -- no point looking at it */
	if ( PROBS_EQUAL(length_prob, 0.0) ){
		return;
	}

	/* get the fill type descriptor (the most common block in the architecture) */
	int fill_type_ind = arch_structs->get_fill_type_index();
	Physical_Type_Descriptor &fill_block_type = arch_structs->block_type[fill_type_ind];

	//TODO: comment. the basic gist (as I remember it) is that a single source/sink can represent multiple
	//	sources/sinks in reality (as in the case of pin equivalence). in that case the scaling factors during path
	//	enumeration, and after probability analysis, have to be adjusted accordingly
	float sum_of_source_probabilities;
	float one_pin_prob;
	get_sum_of_source_probabilities(source_node_ind, rr_node, pin_probs, fill_block_type, &sum_of_source_probabilities, &one_pin_prob);
	int num_sinks = get_num_sinks(sink_node_ind, rr_node, fill_block_type);
	int num_sources = get_num_sources(source_node_ind, rr_node, fill_block_type);

	float source_probability;
	//if (topological_mode == ENUMERATE){
		source_probability = sum_of_source_probabilities;
	//} else {
	//	source_probability = one_pin_prob;
	//}


	if (topological_mode == ENUMERATE){
		/* enumerate connection paths */

		float scaling_factor_for_enumerate = (float)num_sinks * source_probability * length_prob / (float)number_conns_at_length;
		//cout << "source prob: " << source_probability << "  source node: " << source_node_ind << "  conns at length: " << number_conns_at_length << endl;
		//cout << "\t scaling fac: " << scaling_factor_for_enumerate << endl;
		//cout << "\t" << num_sinks << " " << num_sources << " " << source_probability << " " << length_prob << " " << number_conns_at_length << endl;
		enumerate_connection_paths(source_node_ind, sink_node_ind, analysis_settings, arch_structs, 
							routing_structs, ss_distances, node_topo_inf, conn_length, 
							nodes_visited, user_opts,
							scaling_factor_for_enumerate);

	} else if (topological_mode == PROBABILITY){
		/* check whether this source node corresponds to pins of 'driver' or 'receiver' type to figure out which part of the reachability
		   metric this connection applies to */
		int source_ptc = rr_node[source_node_ind].get_ptc_num();
		Pin_Class &source_pin_class = fill_block_type.class_inf[source_ptc];
		e_pin_type source_pin_type = source_pin_class.get_pin_type();

		/* estimate probability of connection being routable and increment the probability metric */
		float probability_connection_routable = estimate_connection_probability(source_node_ind, sink_node_ind, analysis_settings, arch_structs, 
							routing_structs, ss_distances, node_topo_inf, conn_length, 
							nodes_visited, user_opts);

		/* increment the probability metric */
		if (probability_connection_routable >= 0){
			float scaling_factor = (float)num_sinks * source_probability * length_prob / (float)number_conns_at_length;
			float probability_increment = scaling_factor * probability_connection_routable;

			/* increment probability metric */
			int num_subsources = num_sources;
			int num_subsinks = num_sinks;
			increment_probability_metric(probability_increment, conn_length, source_node_ind, sink_node_ind, num_subsources, num_subsinks, source_pin_type);

			/* add this connection's ideal probability to the running total (for normalizing later) */
			pthread_mutex_lock(&f_analysis_results.thread_mutex);
			if (source_pin_type == DRIVER){
				f_analysis_results.max_possible_total_prob_drivers += scaling_factor * 1.0;	//1.0 because that's the max probability a connection can have
			} else if (source_pin_type == RECEIVER){
				f_analysis_results.max_possible_total_prob_fanout += scaling_factor * 1.0;
				//cout << probability_connection_routable << " " << probability_increment << endl;
			} else {
				WTHROW(EX_PATH_ENUM, "Unexpected source pin type: " << source_pin_type);
			}
			pthread_mutex_unlock(&f_analysis_results.thread_mutex);
		} else {
			WTHROW(EX_PATH_ENUM, "Got negative connection probability: " << probability_connection_routable);
		}
	}

	int max_path_weight = analysis_settings->get_max_path_weight(conn_length);
	clean_node_data_structs(nodes_visited, ss_distances, node_topo_inf, max_path_weight);
}


/* Enumerates paths between specified source/sink nodes. */
void enumerate_connection_paths(int source_node_ind, int sink_node_ind, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs,
			Routing_Structs *routing_structs, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, int conn_length,
			t_nodes_visited &nodes_visited, User_Options *user_opts,
			float scaling_factor_for_enumerate){

	t_rr_node &rr_node = routing_structs->rr_node;
	/* get maximum allowable path weight of this connection */
	int max_path_weight = analysis_settings->get_max_path_weight(conn_length);
	int min_dist = UNDEFINED;

	/* set node distances for potentially relevant portion of graph */
	set_node_distances(source_node_ind, sink_node_ind, rr_node, ss_distances, max_path_weight, FORWARD_TRAVERSAL, nodes_visited);
	set_node_distances(sink_node_ind, source_node_ind, rr_node, ss_distances, max_path_weight, BACKWARD_TRAVERSAL, nodes_visited);

	get_ss_distances_and_adjust_max_path_weight(source_node_ind, sink_node_ind, rr_node, ss_distances, max_path_weight,
					nodes_visited, &max_path_weight, &min_dist);

	/* perform path enumeration */
	if (max_path_weight > 0 && min_dist > 0){

		Enumerate_Structs enumerate_structs;
		enumerate_structs.mode = BY_PATH_WEIGHT;

		/* enumerate paths from sink */
		node_topo_inf[sink_node_ind].buckets.sink_buckets[0] = 1;
		do_topological_traversal(sink_node_ind, source_node_ind, rr_node, ss_distances, node_topo_inf, BACKWARD_TRAVERSAL,
					max_path_weight, user_opts, (void*)&enumerate_structs,
					enumerate_node_popped_func,
					enumerate_child_iterated_func,
					enumerate_traversal_done_func);

		/* compute the number of paths to be enumerated from source (which accounts for the scaling factor) */
		int source_node_weight = rr_node[source_node_ind].get_weight();
		node_topo_inf[source_node_ind].buckets.source_buckets[0] = 1;
		float num_enumerated = node_topo_inf[source_node_ind].buckets.get_num_paths(source_node_weight, 0, max_path_weight);


		float scaled_starting_source_paths;
		if (num_enumerated > 0){
			if (scaling_factor_for_enumerate != UNDEFINED){
				scaled_starting_source_paths = scaling_factor_for_enumerate / num_enumerated;
			} else {
				scaled_starting_source_paths = 1;
			}
		} else {
			scaled_starting_source_paths = 0;
		}

		/* enumerate paths from source */
		enumerate_structs.num_routing_nodes_in_subgraph = 0;
		node_topo_inf[source_node_ind].buckets.source_buckets[0] = scaled_starting_source_paths;
		do_topological_traversal(source_node_ind, sink_node_ind, rr_node, ss_distances, node_topo_inf, FORWARD_TRAVERSAL,
					max_path_weight, user_opts, (void*)&enumerate_structs,
					enumerate_node_popped_func,
					enumerate_child_iterated_func,
					enumerate_traversal_done_func);

		/* increment number of connections for which paths have so far been enumerated */
		pthread_mutex_lock(&f_analysis_results.thread_mutex);
		f_analysis_results.num_conns++;
		pthread_mutex_unlock(&f_analysis_results.thread_mutex);
	}
}


/* Estimates the likelyhood (based on node demands) that the specified source/sink connection can be routed */
float estimate_connection_probability(int source_node_ind, int sink_node_ind, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs,
			Routing_Structs *routing_structs, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, int conn_length,
			t_nodes_visited &nodes_visited, User_Options *user_opts){
	
	//float probability_sink_reachable = UNDEFINED;	//some sources/sinks just have no chance of connecting within specified max_path_weight. in that case want to return 0
	float probability_sink_reachable = 0;

	t_rr_node &rr_node = routing_structs->rr_node;
	/* get maximum allowable path weight of this connection */
	int max_path_weight = analysis_settings->get_max_path_weight(conn_length);
	int min_dist = UNDEFINED;

	get_ss_distances_and_adjust_max_path_weight(source_node_ind, sink_node_ind, rr_node, ss_distances, max_path_weight,
					nodes_visited, &max_path_weight, &min_dist);

	/* Get a pointer to the fill type block descriptor -- the one that describes a regular logic block.
	   If a fill type descriptor has never been set (such as when the graph read-in by Wotan is 'simple' and
	   doesn't represent an FPGA), the fill type pointer is set to NULL */
	Physical_Type_Descriptor *fill_type = NULL;
	int fill_type_index = arch_structs->get_fill_type_index();
	if (fill_type_index != UNDEFINED){
		fill_type = &arch_structs->block_type[ fill_type_index ];
	} else {
		//if we are analyzing a simple graph (i.e. just a set of nodes, with no semblance of FPGA architecture) then
		// we want the fill_type variable to be NULL for functions that compute routing probability	
	}

	if (max_path_weight > 0 && min_dist > 0){

		/* The probability analysis can be added on top of path enumeration, or be run by itself with 
		   each node having been assigned a probability by the user during program initialization.
		   In either case, probability analysis returns an estimate of the probability of this source/sink
		   connection being routable. If any scaling to probabilities is desired, it should be done outside this
		   function */

		if ( PROBABILITY_MODE == CUTLINE ){
			node_topo_inf[source_node_ind].set_level( 0 );

			Cutline_Structs cutline_structs;
			cutline_structs.fill_type = fill_type;
			do_topological_traversal(source_node_ind, sink_node_ind, rr_node, ss_distances, node_topo_inf, FORWARD_TRAVERSAL,
						max_path_weight, user_opts, (void*)&cutline_structs,
						cutline_node_popped_func,
						cutline_child_iterated_func,
						cutline_traversal_done_func);

			probability_sink_reachable = cutline_structs.prob_routable;

		} else if ( PROBABILITY_MODE == CUTLINE_SIMPLE ){
			set_node_hops(source_node_ind, sink_node_ind, rr_node, ss_distances, max_path_weight, FORWARD_TRAVERSAL);
			set_node_hops(sink_node_ind, source_node_ind, rr_node, ss_distances, max_path_weight, BACKWARD_TRAVERSAL);

			/* get hops from source to sink; size the cutline prob struct vector based on that */
			int source_sink_hops = ss_distances[source_node_ind].get_sink_hops();	//hops from sink

			Cutline_Simple_Structs cutline_simple_structs;
			cutline_simple_structs.cutline_simple_prob_struct.assign(source_sink_hops-1, vector<int>());
			cutline_simple_structs.fill_type = fill_type;
			
			do_topological_traversal(source_node_ind, sink_node_ind, rr_node, ss_distances, node_topo_inf, FORWARD_TRAVERSAL,
						max_path_weight, user_opts, (void*)&cutline_simple_structs,
						cutline_simple_node_popped_func,
						cutline_simple_child_iterated_func,
						cutline_simple_traversal_done_func);

			probability_sink_reachable = cutline_simple_structs.prob_routable;

		} else if ( PROBABILITY_MODE == CUTLINE_RECURSIVE ){
			//cout << "from: " << source_node_ind << "  to: " << sink_node_ind << endl;
			//cout << "  max path weight: " << max_path_weight << endl;
			set_node_hops(source_node_ind, sink_node_ind, rr_node, ss_distances, max_path_weight, FORWARD_TRAVERSAL);
			set_node_hops(sink_node_ind, source_node_ind, rr_node, ss_distances, max_path_weight, BACKWARD_TRAVERSAL);

			Cutline_Recursive_Structs cutline_rec_structs;

			//cout << "  min dist: " << min_dist << "  max path weight: " << max_path_weight << endl;

			int source_hops = ss_distances[sink_node_ind].get_source_hops();
			//cout << "  source hops: " << source_hops << endl;
			cutline_rec_structs.bound_source_hops = source_hops;
			cutline_rec_structs.recurse_level = 0;
			cutline_rec_structs.cutline_rec_prob_struct.assign( source_hops, vector<int>() );
			cutline_rec_structs.source_ind = source_node_ind;
			cutline_rec_structs.sink_ind = sink_node_ind;
			cutline_rec_structs.fill_type = fill_type;

			do_topological_traversal(source_node_ind, sink_node_ind, rr_node, ss_distances, node_topo_inf, FORWARD_TRAVERSAL,
						max_path_weight, user_opts, (void*)&cutline_rec_structs,
						cutline_recursive_node_popped_func,
						cutline_recursive_child_iterated_func,
						cutline_recursive_traversal_done_func);

			probability_sink_reachable = cutline_rec_structs.prob_routable;

		} else if ( PROBABILITY_MODE == PROPAGATE ){
			node_topo_inf[source_node_ind].buckets.source_buckets[0] = 1;

			Propagate_Structs propagate_structs;
			propagate_structs.fill_type = fill_type;
			do_topological_traversal(source_node_ind, sink_node_ind, rr_node, ss_distances, node_topo_inf, FORWARD_TRAVERSAL,
						max_path_weight, user_opts, (void*)&propagate_structs,
						propagate_node_popped_func,
						propagate_child_iterated_func,
						propagate_traversal_done_func);
			

			probability_sink_reachable = propagate_structs.prob_routable;

			//cout << "prob reachable: " << probability_sink_reachable << endl;
		} else if ( PROBABILITY_MODE == RELIABILITY_POLYNOMIAL ){
			if (user_opts->use_routing_node_demand == UNDEFINED){
				WTHROW(EX_PATH_ENUM, "Probability mode was set to RELIABILITY_POLYNOMIAL. But user_opts->use_routing_node_demand was not set!");
			}

			set_node_hops(source_node_ind, sink_node_ind, rr_node, ss_distances, max_path_weight, FORWARD_TRAVERSAL);
			set_node_hops(sink_node_ind, source_node_ind, rr_node, ss_distances, max_path_weight, BACKWARD_TRAVERSAL);

			/* enumerate paths from source */
			/* note -- this increments node demands a second time. but since we will be ignoring node demands completely, this is fine */
			Enumerate_Structs enumerate_structs;
			enumerate_structs.mode = BY_PATH_HOPS;

			//got source_sink_hops of -1??

			node_topo_inf[source_node_ind].buckets.source_buckets[0] = 1;	//one path at bucket 0 -- gotta start with something
			do_topological_traversal(source_node_ind, sink_node_ind, rr_node, ss_distances, node_topo_inf, FORWARD_TRAVERSAL,
						max_path_weight, user_opts, (void*)&enumerate_structs,
						enumerate_node_popped_func,
						enumerate_child_iterated_func,
						enumerate_traversal_done_func);

			//cout << "max path weight: " << max_path_weight << endl;

			int source_sink_hops = ss_distances[sink_node_ind].get_source_hops();
			Node_Buckets &sink_node_buckets = node_topo_inf[sink_node_ind].buckets;
			double *source_buckets = sink_node_buckets.source_buckets;
			int num_source_buckets = sink_node_buckets.get_num_source_buckets();

			probability_sink_reachable = analyze_reliability_polynomial(source_sink_hops, source_buckets, num_source_buckets,
									enumerate_structs.num_routing_nodes_in_subgraph, 1-user_opts->use_routing_node_demand);

			//cout << "probability: " << probability_sink_reachable << endl;
		} else {
			WTHROW(EX_PATH_ENUM, "Unknown probability mode: " << PROBABILITY_MODE);
		}

		if (probability_sink_reachable > 1){
			WTHROW(EX_PATH_ENUM, "Got a probability > 1: " << probability_sink_reachable);
		} else if (probability_sink_reachable < 0){
			WTHROW(EX_PATH_ENUM, "Got a probability < 0: " << probability_sink_reachable);
		}
	}

	return probability_sink_reachable;
}


/* fills the t_ss_distances structures according to source & sink distances to intermediate nodes. 
   also returns an adjusted maximum path weight (to be further passed on to path enumeration / probability analysis functions)
   based on the distance from the source to the sink */
void get_ss_distances_and_adjust_max_path_weight(int source_node_ind, int sink_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances,
                                int max_path_weight, t_nodes_visited &nodes_visited, int *adjusted_max_path_weight, int *source_sink_dist){
	
	/* set node distances for potentially relevant portion of graph */
	set_node_distances(source_node_ind, sink_node_ind, rr_node, ss_distances, max_path_weight, FORWARD_TRAVERSAL, nodes_visited);
	set_node_distances(sink_node_ind, source_node_ind, rr_node, ss_distances, max_path_weight, BACKWARD_TRAVERSAL, nodes_visited);

	/* adjust maximum allowable path weight based on minimum distance. FIXME. this may not work well for multiple wirelengths */
	int min_dist_sink = ss_distances[sink_node_ind].get_source_distance();
	int min_dist_source = ss_distances[source_node_ind].get_sink_distance();
	if (min_dist_sink != min_dist_source){
		//WTHROW(EX_PATH_ENUM, "Distance to source doesn't match distance to sink. " << min_dist_source << " vs " << min_dist_sink << endl);
	}

	max_path_weight = adjust_max_path_weight_based_on_ss_dist(min_dist_sink, max_path_weight);
	//max_path_weight = min((int)ceil(min_dist_sink * PATH_FLEXIBILITY_FACTOR), max_path_weight);	//adjust_max_path_weight_based_on_ss_dist

	(*adjusted_max_path_weight) = max_path_weight;
	(*source_sink_dist) = min_dist_sink;
}

/* adjusts maximum path weight based on the minimum distance of the current source/sink pair.
   note that the adjusted max path weight SHOULD stay in effect for only the current source/sink pair analysis
   and is reset afterwards. */
int adjust_max_path_weight_based_on_ss_dist(int min_dist_sink, int current_max_path_weight){
	int adjusted_max_path_weight = UNDEFINED;

	adjusted_max_path_weight = min((int)ceil(min_dist_sink * PATH_FLEXIBILITY_FACTOR), current_max_path_weight);

	return adjusted_max_path_weight;
}


/* traverses graph from 'from_node_ind' and for each node traversed, sets distance to the source/sink node from
   which the traversal started (based on traversal_dir) */
void set_node_distances(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances,
			int max_path_weight, e_traversal_dir traversal_dir, t_nodes_visited &nodes_visited){
	
	//cout << "Distance traversal " << traversal_dir << " from " << from_node_ind << " at " << rr_node[from_node_ind].get_xlow() << "," << rr_node[from_node_ind].get_ylow() <<
	//	" to " << to_node_ind << " at " << rr_node[to_node_ind].get_xlow() << "," << rr_node[to_node_ind].get_ylow() << endl;

	/* define a bounded-height priority queue in which to store nodes during traversal */
	My_Bounded_Priority_Queue< int > PQ( max_path_weight*2 );
	int *edge_list;
	int num_children;

	PQ.push(from_node_ind, 0);

	/* mark 'from' node as visited */
	if (traversal_dir == FORWARD_TRAVERSAL){
		ss_distances[from_node_ind].set_source_distance(0);
		ss_distances[from_node_ind].set_visited_from_source(true);
	} else {
		ss_distances[from_node_ind].set_sink_distance(0);
		ss_distances[from_node_ind].set_visited_from_sink(true);
	}
	
	/* and now perform dijkstra's algorithm */
	while(PQ.size() != 0){
		/* get node which terminates the lowest-weight path */
		int node_ind = PQ.top();
		int node_path_weight = PQ.top_weight();	//should match the distance from this node to source/sink (if doing forward/backward traversal)
		PQ.pop();


		if (node_ind == to_node_ind){
			/* adjust max path weight (determines which nodes will be considered during this traversal) */
			max_path_weight = adjust_max_path_weight_based_on_ss_dist(node_path_weight, max_path_weight);
		}

		if (traversal_dir == FORWARD_TRAVERSAL){
			/* expand along outgoing edges */
			edge_list = rr_node[node_ind].out_edges;
			num_children = rr_node[node_ind].get_num_out_edges();
		} else {
			/* expand along incoming edges */
			edge_list = rr_node[node_ind].in_edges;
			num_children = rr_node[node_ind].get_num_in_edges();
		}

		/* now iterate over children of this node and selectively push them onto the queue */
		put_children_on_pq_and_set_ss_distance(num_children, edge_list, node_path_weight, ss_distances, max_path_weight, 
						traversal_dir, rr_node, to_node_ind, &PQ);

		nodes_visited.push_back( node_ind );
	}
}

/* enqueues nodes belonging to specified edge list onto the bounded priority queue. the weight of the 
   enqueued nodes will be base_weight + their own weight.
   also... TODO */
void put_children_on_pq_and_set_ss_distance(int num_edges, int *edge_list, int base_weight, t_ss_distances &ss_distances,
		int max_path_weight, e_traversal_dir traversal_dir, t_rr_node &rr_node, int to_node_ind, My_Bounded_Priority_Queue<int> *PQ){

	int dest_xlow, dest_xhigh, dest_ylow, dest_yhigh;
	
	dest_xlow = rr_node[to_node_ind].get_xlow();
	dest_xhigh = rr_node[to_node_ind].get_xhigh();
	dest_ylow = rr_node[to_node_ind].get_ylow();
	dest_yhigh = rr_node[to_node_ind].get_yhigh();

	/* expecting the destination node to not be localized to one tile only */
	if (dest_xlow != dest_xhigh || dest_ylow != dest_yhigh){
		WTHROW(EX_PATH_ENUM, "Expected destination node to be localized to a single tile");
	}

	int destx = dest_xlow;
	int desty = dest_ylow;

	for (int iedge = 0; iedge < num_edges; iedge++){
		int node_ind = edge_list[iedge];
		e_rr_type node_type = rr_node[node_ind].get_rr_type();

		/* check if node has already been visited */
		if (traversal_dir == FORWARD_TRAVERSAL){
			if (ss_distances[node_ind].get_visited_from_source()){
				continue;
			}
		} else {
			if (ss_distances[node_ind].get_visited_from_sink()){
				continue;
			}
		}
		
		int node_weight = rr_node[node_ind].get_weight();
		int path_weight = base_weight + node_weight;

		/* mark node as visited */
		if (traversal_dir == FORWARD_TRAVERSAL){
			if (node_type == IPIN){
				if (rr_node[node_ind].get_xlow() != dest_xlow && rr_node[node_ind].get_ylow() != dest_ylow){
					continue;
				}
			}

			/* on forward traversal, skip nodes that have no chance to reach the destination in the maximum allowed path weight */
			if (!node_has_chance_to_reach_destination(node_ind, destx, desty, path_weight, max_path_weight, rr_node)){
				continue;
			}


			ss_distances[node_ind].set_source_distance( path_weight );
			ss_distances[node_ind].set_visited_from_source(true);
		} else {
			if (node_type == OPIN){
				if (rr_node[node_ind].get_xlow() != dest_xlow && rr_node[node_ind].get_ylow() != dest_ylow){
					continue;
				}
			}

			ss_distances[node_ind].set_sink_distance( path_weight );
			ss_distances[node_ind].set_visited_from_sink(true);

			//TODO: seeing a slight difference in total probability (but not total demand) with this method
			/* on backward traversal, skip nodes that definitely can't reach the destination (the 'to' node ) */
			if (!ss_distances[node_ind].is_legal( node_weight, max_path_weight) ){
				ss_distances[node_ind].set_sink_distance( UNDEFINED );
				ss_distances[node_ind].set_visited_from_sink(false);
				continue;
			}
		}

		PQ->push(node_ind, path_weight);

	}
}


/* returns whether or not the specified node has a chance to reach the specified destination node. the node terminates a path
   of weight 'node_path_weight' (weight of node is included here); the maximum allowable path weight is 'max_path_weight'.

   right now this function is based on geometric properties of island-style FPGAs. TODO: explain more */
bool node_has_chance_to_reach_destination(int node_ind, int destx, int desty, int node_path_weight, int max_path_weight, t_rr_node &rr_node){
	bool has_chance_to_reach = false;
	int remaining_lower_bound;

	//if (node_path_weight <= max_path_weight){
	//	has_chance_to_reach = true;
	//}
	int node_xlow, node_xhigh, node_ylow, node_yhigh;

	node_xlow = rr_node[node_ind].get_xlow();
	node_xhigh = rr_node[node_ind].get_xhigh();
	node_ylow = rr_node[node_ind].get_ylow();
	node_yhigh = rr_node[node_ind].get_yhigh();

	int x_diff, y_diff;
	if (node_xlow == node_xhigh){
		/* node spans in y-direction */
		if (desty <= node_yhigh && desty >= node_ylow){
			x_diff = abs(destx - node_xlow);
			y_diff = 0;
		} else if (desty > node_yhigh){
			x_diff = abs(destx - node_xlow);
			y_diff = desty - node_yhigh;
		} else {
			x_diff = abs(destx - node_xlow);
			y_diff = node_ylow - desty;
		}
	} else if (node_ylow == node_yhigh){
		/* node spans in x-direction */
		if (destx <= node_xhigh && destx >= node_xlow){
			x_diff = 0;
			y_diff = abs(desty -node_ylow) - 1;
		} else if (destx > node_xhigh){
			x_diff = destx - node_xhigh;
			y_diff = abs(desty - node_ylow);
		} else {
			x_diff = node_xlow - destx;
			y_diff = abs(desty - node_ylow);
		}
		
	} else {
		WTHROW(EX_PATH_ENUM, "Node has a span in both the x and y directions");
	}
	remaining_lower_bound = max(x_diff + y_diff-1, 0);

	if (node_path_weight + remaining_lower_bound <= max_path_weight){
		has_chance_to_reach = true;
	} else {
		has_chance_to_reach = false;
	}
	
	return has_chance_to_reach;
}


/* does BFS over legal subraph from the 'from' node to the 'to' node and sets minimum number of hops
   required to arrive at each legal node from the 'from' node (along either the forward or reverse edges
   as determined by traversal_dir) */
void set_node_hops(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances,
			int max_path_weight, e_traversal_dir traversal_dir){

	queue<int> Q;
	Q.push( from_node_ind );

	if (traversal_dir == FORWARD_TRAVERSAL){
		ss_distances[from_node_ind].set_source_hops(0);
	} else {
		ss_distances[from_node_ind].set_sink_hops(0);
	}

	while( !Q.empty() ){
		int node_ind = Q.front();
		Q.pop();

		int *edge_list;
		int num_children;
		int node_hops;

		/* get edges over which to expand and mark the current node as done */
		if (traversal_dir == FORWARD_TRAVERSAL){
			edge_list = rr_node[node_ind].out_edges;
			num_children = rr_node[node_ind].get_num_out_edges();
			ss_distances[node_ind].set_visited_from_source_hops(true);
			node_hops = ss_distances[node_ind].get_source_hops();
		} else {
			edge_list = rr_node[node_ind].in_edges;
			num_children = rr_node[node_ind].get_num_in_edges();
			ss_distances[node_ind].set_visited_from_sink_hops(true);
			node_hops = ss_distances[node_ind].get_sink_hops();
		}


		/* expand over edges */
		for (int iedge = 0; iedge < num_children; iedge++){
			int child_ind = edge_list[iedge];
			int child_weight = rr_node[child_ind].get_weight();

			/* check that child is legal */
			if (!ss_distances[child_ind].is_legal(child_weight, max_path_weight)){
				continue;
			}

			/* check that child node hasn't already been visited */
			bool already_visited = false;
			if (traversal_dir == FORWARD_TRAVERSAL){
				already_visited = ss_distances[child_ind].get_visited_from_source_hops();
			} else {
				already_visited = ss_distances[child_ind].get_visited_from_sink_hops();
			}
			if (already_visited){
				continue;
			}

			/* set # hops from 'from_node_ind' node and add child to expansion queue */
			if (traversal_dir == FORWARD_TRAVERSAL){
				ss_distances[child_ind].set_visited_from_source_hops(true);
				ss_distances[child_ind].set_source_hops(node_hops + 1);
			} else {
				ss_distances[child_ind].set_sink_hops(node_hops + 1);
				ss_distances[child_ind].set_visited_from_sink_hops(true);
			}
			Q.push( child_ind );
		}
	}
}


/* resets data structures associated with nodes that have been visited during the previous path traversals */
void clean_node_data_structs(t_nodes_visited &nodes_visited, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, int max_path_weight){

	clean_ss_distances(ss_distances, nodes_visited);
	clean_node_topo_inf(node_topo_inf, nodes_visited, max_path_weight);

	nodes_visited.clear();
}

/* clears ss_distances structure according to nodes that have been visited during graph traversal */
void clean_ss_distances(t_ss_distances &ss_distances, t_nodes_visited &nodes_visited){
	int num_nodes_visited = (int)nodes_visited.size();

	for (int inode = 0; inode < num_nodes_visited; inode++){
		int node_ind = nodes_visited[inode];

		/* clear distances of node to source/sink */
		if (ss_distances[node_ind].get_visited_from_source() || 
		    ss_distances[node_ind].get_visited_from_sink()){
			ss_distances[node_ind].clear();
		}
	}
}

/* clears node_buckets structure according to nodes that have been visited during graph traversal */
void clean_node_topo_inf(t_node_topo_inf &node_topo_inf, t_nodes_visited &nodes_visited, int max_path_weight){
	int num_nodes_visited = (int)nodes_visited.size();

	for (int inode = 0; inode < num_nodes_visited; inode++){
		int node_ind = nodes_visited[inode];

		/* clear node buckets */
		if (node_topo_inf[node_ind].get_was_visited()){
			//node_topo_inf[node_ind].buckets.clear_up_to(max_path_weight);
			node_topo_inf[node_ind].clear();
		} 
	}
}

/* returns the sum of pin probabilities over all the pins that the specified source node represents */
void get_sum_of_source_probabilities(int source_node_ind, t_rr_node &rr_node, t_prob_list &pin_probs,
				Physical_Type_Descriptor &fill_block_type, float *sum_probabilities, float *one_pin_prob){
	(*sum_probabilities) = 0;

	e_rr_type node_type = rr_node[source_node_ind].get_rr_type();
	int node_ptc = rr_node[source_node_ind].get_ptc_num();

	if (node_type == SOURCE){
		/* sum of the probabilities of the constituent pins */
		Pin_Class &pin_class = fill_block_type.class_inf[node_ptc];
		int num_pins = pin_class.get_num_pins();

		float this_pin_prob = UNDEFINED;
		for (int ipin = 0; ipin < num_pins; ipin++){
			int pin = pin_class.pinlist[ipin];

			//cout << "pin " << pin << ": " << pin_probs[pin] << "  this_pin_prob: " << this_pin_prob << endl;

			if (this_pin_prob == UNDEFINED){
				this_pin_prob = pin_probs[pin];
			} else {
				if ( !PROBS_EQUAL(this_pin_prob, pin_probs[pin]) ){  //this_pin_prob != pin_probs[pin]){
					cout << (this_pin_prob == pin_probs[pin]) << endl;
					cout << (this_pin_prob - pin_probs[pin]) << endl;
					WTHROW(EX_PATH_ENUM, "Expecting probabilities of pins belonging to the same pin class to be equal. " << 
							"expected: " << this_pin_prob << "  got: " << pin_probs[pin]);
				}
			}

			(*sum_probabilities) += this_pin_prob;
		}
		if (one_pin_prob != NULL){
			(*one_pin_prob) = this_pin_prob;
		}
	} else {
		WTHROW(EX_PATH_ENUM, "Unexpected node type: " << rr_node[source_node_ind].get_rr_type_string());
	}
	//cout << "node: " << source_node_ind << "  sum probabilities: " << *sum_probabilities << endl;
}

/* returns number of sinks corresponding to the specified super-sink node */
int get_num_sinks(int sink_node_ind, t_rr_node &rr_node, Physical_Type_Descriptor &fill_block_type){
	int num_sinks = 0;

	if (rr_node[sink_node_ind].get_rr_type() != SINK){
		WTHROW(EX_PATH_ENUM, "Expected node to be a sink. Got node of type: " << rr_node[sink_node_ind].get_rr_type());
	}

	int node_ptc = rr_node[sink_node_ind].get_ptc_num();			//TODO: call a get_num_subnodes function of some sort?? otherwise shared code with get_num_sources
	Pin_Class &pin_class = fill_block_type.class_inf[node_ptc];

	num_sinks = pin_class.get_num_pins();

	return num_sinks;
}

/* returns number of sources corresponding to the specified super-source node */
int get_num_sources(int source_node_ind, t_rr_node &rr_node, Physical_Type_Descriptor &fill_block_type){
	int num_sources = 0;

	if (rr_node[source_node_ind].get_rr_type() != SOURCE){
		WTHROW(EX_PATH_ENUM, "Expected node to be a source. Got node of type: " << rr_node[source_node_ind].get_rr_type_string());
	}

	int node_ptc = rr_node[source_node_ind].get_ptc_num();
	Pin_Class &pin_class = fill_block_type.class_inf[node_ptc];

	num_sources = pin_class.get_num_pins();

	return num_sources;
}

/* function for a thread to increment the probability metric */
void increment_probability_metric(float probability_increment, int connection_length, int source_node_ind, int sink_node_ind,
				int num_subsources, int num_subsinks, e_pin_type source_pin_type){

	double *total_prob;
	vector<t_lowest_probs_pq> *lowest_probs_pqs;

	/* the routability metric may be broken up into more than one component.
	   currently connections are analyzed separately from regular sources and 'virtual' sources which are
	   attached alongside sinks and connect to the channel tracks from which the sink is immediately reachable (to account for fanout-like effects) */
	if (source_pin_type == DRIVER){
		total_prob = &f_analysis_results.total_prob_drivers;
		lowest_probs_pqs = &f_analysis_results.lowest_probs_pqs_drivers;
	} else if (source_pin_type == RECEIVER){
		total_prob = &f_analysis_results.total_prob_fanout;
		lowest_probs_pqs = &f_analysis_results.lowest_probs_pqs_fanout;

		//cout << connection_length << " " << (*lowest_probs_pqs)[connection_length].size() << " " << probability_increment << endl;
	} else {
		WTHROW(EX_PATH_ENUM, "Unexpected pin type: " << source_pin_type);
	}

	pthread_mutex_lock(&f_analysis_results.thread_mutex);
	*total_prob += probability_increment;
	
	/* account for multiple sources/sinks being present in a supersource/supersink */
	int div_factor = num_subsources * num_subsinks;
	float push_value = probability_increment / (float)div_factor;
	for (int i = 0; i < div_factor; i++){
		(*lowest_probs_pqs)[connection_length].push( push_value );
	}

	pthread_mutex_unlock(&f_analysis_results.thread_mutex);
}


/* at each length, sums the probabilities of the x% worst possible connections */
static float analyze_lowest_probs_pqs(vector<t_lowest_probs_pq> &lowest_probs_pqs){
	float result = 0;

	int num_lengths = (int)lowest_probs_pqs.size();
	for (int ilen = 0; ilen < num_lengths; ilen++){
		int num_entries = lowest_probs_pqs[ilen].size();

		float result_at_len = 0;

		//cout << "at length " << ilen << "  there are " << num_entries << " entries" << endl;

		for (int ient = 0; ient < num_entries; ient++){
			float entry = lowest_probs_pqs[ilen].top();
			//if (ilen == 1)
			//	cout << " length " << ilen << "  entry " << entry << endl;
			result += entry;
			result_at_len += entry;
			lowest_probs_pqs[ilen].pop();
		}

		cout << "len" << ilen << " result: " << result_at_len << endl;
	}

	return result;
}


/* returns a node's demand, less the demand of the specified source/sink connection. if node didn't keep
   history of path counts due to this source/sink connection, or if 'fill_type' is specified as NULL, then node demand is unmodified */
float get_node_demand_adjusted_for_path_history(int node_ind, t_rr_node &rr_node, int source_ind, int sink_ind, Physical_Type_Descriptor *fill_type,
                                                       User_Options *user_opts){

	float adjusted_node_demand = rr_node[node_ind].get_demand(user_opts);

	if (fill_type != NULL){
		RR_Node &source_node = rr_node[ source_ind ];
		int source_ptc = source_node.get_ptc_num();
		int num_source_pins = fill_type->class_inf[ source_ptc ].get_num_pins();
		RR_Node &sink_node = rr_node[ sink_ind ];
		int sink_ptc = sink_node.get_ptc_num();
		int num_sink_pins = fill_type->class_inf[ sink_ptc ].get_num_pins();

		float source_contribution = rr_node[node_ind].get_path_count_history( source_node ) / (float)num_source_pins;
		float sink_contribution = rr_node[node_ind].get_path_count_history( sink_node ) / (float)num_sink_pins;
		float modifier = max(0.0F, max(source_contribution, sink_contribution));

		if (modifier > adjusted_node_demand + 0.00001){
			WTHROW(EX_PATH_ENUM, "modifier " << modifier << " larger than node demand " << adjusted_node_demand);
		}
		adjusted_node_demand -= modifier;

		adjusted_node_demand = max(0.0F, adjusted_node_demand); //because floating point..
	}

	return adjusted_node_demand;
}

