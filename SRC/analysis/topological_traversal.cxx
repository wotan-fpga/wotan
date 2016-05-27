
#include <queue>
#include <set>
#include "topological_traversal.h"
#include "exception.h"
#include "wotan_types.h"

using namespace std;


/**** Typedefs ****/
/* A structure that is used to break cycles during topological traversal. Objects of the
   Node_Waiting class are put on this sorted structure, and if the traditional expansion queue 
   becomes empty during topological traversal, this structure is used to get the next node on which
   to expand */
typedef set< Node_Waiting > t_nodes_waiting;


/**** Function Declarations ****/
/* Used during topological traversal. Selectively puts the nodes specified in edge_list onto queue.
   Manages the sorted nodes_waiting structure which is used to deal with cycles during topological traversal.
   usr_exec_child_iterated -- executed after it is verified that a given child is legal (can be NULL) */
static void put_children_on_queue_and_update_structs(int *edge_list, int num_nodes, int parent_ind, t_rr_node &rr_node, t_ss_distances &ss_distances,
					t_node_topo_inf &node_topo_inf, queue<int> &Q, t_nodes_waiting &nodes_waiting, e_traversal_dir traversal_dir,
					int max_path_weight, int from_node_ind, int to_node_ind, User_Options *user_opts, void *user_data,
					t_usr_child_iterated_func usr_exec_child_iterated);
/* puts specified child node onto the sorted 'nodes_waiting' structure. this structure is sorted by a path weight 
   (which will be determined in this function), and the child's node index serving as a tie breaker */
static void put_child_onto_nodes_waiting_structure(int child_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, 
			t_node_topo_inf &node_topo_inf, e_traversal_dir traversal_dir, t_nodes_waiting &nodes_waiting);


/**** Function Definitions ****/
/* 
   Function for doing topological traversal.

   usr_exec_node_popped: function to be executed when a new node is popped off the expansion queue during topological traversal
   usr_exec_child_iterated: to be executed while iterating over a node's children (execution not guaranteed -- based on child legality)
   usr_exec_traversal_done: executed after entire topological traversal is complete

   All passed-in function pointers can be NULL
*/
void do_topological_traversal(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
			e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data,
			t_usr_node_popped_func usr_exec_node_popped,
			t_usr_child_iterated_func usr_exec_child_iterated,
			t_usr_traversal_done_func usr_exec_traversal_done){

	/* a queue for traversing the graph */
	queue<int> Q;
	
	/* a sorted list of node indices corresponding to nodes which have unmet dependencies;
	   used to break cycles */
	t_nodes_waiting nodes_waiting;

	//Commenting because this check doesn't fly when we do recursive traversals
	///* check that starting node is a source node */
	//e_rr_type from_type = rr_node[from_node_ind].get_rr_type();
	//if ( from_type != SOURCE && from_type != SINK ){
	//	WTHROW(EX_PATH_ENUM, "Expected starting node to be of SOURCE or SINK type");
	//}

	/* put starting node onto queue */
	Q.push( from_node_ind );

	/* mark 'from' node as visited */
	node_topo_inf[from_node_ind].set_was_visited(true);
	if (traversal_dir == FORWARD_TRAVERSAL){
		node_topo_inf[from_node_ind].increment_times_visited_from_source();
		node_topo_inf[from_node_ind].set_done_from_source(true);
	} else {
		node_topo_inf[from_node_ind].increment_times_visited_from_sink();
		node_topo_inf[from_node_ind].set_done_from_sink(true);
	}

	/* now use queue to traverse the graph */
	while ( !Q.empty() ){
		int node_ind;
		int *edge_list;
		int num_edges;

		node_ind = Q.front();
		Q.pop();

		/* get edges along which to expand */
		if (traversal_dir == FORWARD_TRAVERSAL){
			edge_list = rr_node[node_ind].out_edges;
			num_edges = rr_node[node_ind].get_num_out_edges();

		} else {
			edge_list = rr_node[node_ind].in_edges;
			num_edges = rr_node[node_ind].get_num_in_edges();
		}

		/* EXECUTE USER-DEFINED FUNCTION */
		if (usr_exec_node_popped != NULL){
			usr_exec_node_popped(node_ind, from_node_ind, to_node_ind, rr_node, ss_distances, node_topo_inf, traversal_dir, max_path_weight, user_opts, user_data);
		}

		/* put children onto queue or nodes_waiting structure */
		put_children_on_queue_and_update_structs(edge_list, num_edges, node_ind, rr_node, ss_distances, node_topo_inf,
						Q, nodes_waiting, traversal_dir, max_path_weight, from_node_ind, to_node_ind,
						user_opts, user_data, usr_exec_child_iterated);


		if (Q.empty() && !nodes_waiting.empty()){
			/* encountered a cycle somewhere. get first node from the sorted nodes_waiting structure and continue expanding
			   on that */
			if (nodes_waiting.empty()){
				WTHROW(EX_PATH_ENUM, "Nodes waiting queue empty!");
			}

			Node_Waiting node_waiting = (*nodes_waiting.begin());
			nodes_waiting.erase(node_waiting);

			int next_node_ind = node_waiting.get_node_ind();
			Q.push(next_node_ind);

			if (traversal_dir == FORWARD_TRAVERSAL){
				node_topo_inf[next_node_ind].set_done_from_source(true);
			} else {
				node_topo_inf[next_node_ind].set_done_from_sink(true);
			}
		}
	}

	/* EXECUTE USER-DEFINED FUNCTION */
	if (usr_exec_traversal_done != NULL){
		usr_exec_traversal_done(from_node_ind, to_node_ind, rr_node, ss_distances, node_topo_inf, traversal_dir, max_path_weight, user_opts, user_data);
	}
}


/* Used during topological traversal. Selectively puts the nodes specified in edge_list onto queue.
   Manages the sorted nodes_waiting structure which is used to deal with cycles during topological traversal.

   usr_exec_child_iterated -- executed after it is verified that a given child is legal (can be NULL) */
static void put_children_on_queue_and_update_structs(int *edge_list, int num_nodes, int parent_ind, t_rr_node &rr_node, t_ss_distances &ss_distances,
					t_node_topo_inf &node_topo_inf, queue<int> &Q, t_nodes_waiting &nodes_waiting, e_traversal_dir traversal_dir,
					int max_path_weight, int from_node_ind, int to_node_ind, User_Options *user_opts, void *user_data,
					t_usr_child_iterated_func usr_exec_child_iterated){


	for (int inode = 0; inode < num_nodes; inode++){
		int node_ind = edge_list[inode];
		
		/* skip nodes which have already been inserted onto the queue */
		if (traversal_dir == FORWARD_TRAVERSAL){
			if (node_topo_inf[node_ind].get_done_from_source()){
				continue;
			}
		} else {
			if (node_topo_inf[node_ind].get_done_from_sink()){
				continue;
			}
		}

		/* skip nodes which cannot carry a legal path from source to sink */
		if ( !ss_distances[node_ind].is_legal(rr_node[node_ind].get_weight(), max_path_weight) ){
			continue;	
		}


		/* EXECUTE USER-DEFINED FUNCTION */
		bool ignore_node = false;
		if (usr_exec_child_iterated != NULL){
			ignore_node = usr_exec_child_iterated(parent_ind, inode, node_ind, rr_node, ss_distances, node_topo_inf, traversal_dir, 
			                                     max_path_weight, from_node_ind, to_node_ind, user_opts, user_data);
		}
		if (ignore_node){
			continue;
		}

		/* mark that this node was visited. this indicates that some state variables related to the node have been changed, 
		   and should be reset after the current connection traversal is complete */
		node_topo_inf[node_ind].set_was_visited(true);

		/* increment number of times node has been visited and get the number of legal parents of this node (for purposes of knowing
		   whether this node has all its parent depenencies met) */
		int num_times_visited;
		int num_node_legal_parents;
		if (traversal_dir == FORWARD_TRAVERSAL){
			node_topo_inf[node_ind].increment_times_visited_from_source();
			num_times_visited = node_topo_inf[node_ind].get_times_visited_from_source();
			num_node_legal_parents = node_topo_inf[node_ind].set_and_or_get_num_legal_in_nodes(node_ind, rr_node, ss_distances, max_path_weight);
		} else {
			node_topo_inf[node_ind].increment_times_visited_from_sink();
			num_times_visited = node_topo_inf[node_ind].get_times_visited_from_sink();
			num_node_legal_parents = node_topo_inf[node_ind].set_and_or_get_num_legal_out_nodes(node_ind, rr_node, ss_distances, max_path_weight);
		}

		/* if this node is the destination node */
		if (node_ind == to_node_ind){
			continue;
		}

		/* push node to nodes_waiting structure, remove it from there, or do nothing depending on how many times
		   the node has been visited */
		int remaining_dependencies = num_node_legal_parents - num_times_visited;

		if (num_times_visited == 1 && remaining_dependencies > 0){
			/* visiting this node for the first time and it still has unmet dependencies -- push onto nodes_waiting structure */
			put_child_onto_nodes_waiting_structure(node_ind, rr_node, ss_distances, node_topo_inf, traversal_dir, 
			                                       nodes_waiting);

		} else if (num_times_visited == 1 && remaining_dependencies == 0){
			/* visiting this node for the first time, but all its dependencies are already met -- push onto queue */
			Q.push(node_ind);

			if (traversal_dir == FORWARD_TRAVERSAL){
				node_topo_inf[node_ind].set_done_from_source(true);
			} else {
				node_topo_inf[node_ind].set_done_from_sink(true);
			}
		} else if (remaining_dependencies > 0){
			/* this node has been visited before and it's already on the nodes_waiting structure -- do nothing */
		} else if (remaining_dependencies == 0){
			/* this node has been visited before, and all of its dependencies are now met -- remove from nodes_waiting structure
			   and push onto queue */
			Node_Waiting my_node_waiting = node_topo_inf[node_ind].node_waiting_info;
			node_topo_inf[node_ind].node_waiting_info.clear();
	
			nodes_waiting.erase( my_node_waiting );
			Q.push(node_ind);

			if (traversal_dir == FORWARD_TRAVERSAL){
				node_topo_inf[node_ind].set_done_from_source(true);
			} else {
				node_topo_inf[node_ind].set_done_from_sink(true);
			}
		}
	}
}


/* puts specified child node onto the sorted 'nodes_waiting' structure. this structure is sorted by a path weight 
   (which will be determined in this function), and the child's node index serving as a tie breaker */
static void put_child_onto_nodes_waiting_structure(int child_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, 
			t_node_topo_inf &node_topo_inf, e_traversal_dir traversal_dir, t_nodes_waiting &nodes_waiting){

	/* Currently the path weight attributed to the child node is the shortest path from the 
	   starting node (be that the source or the sink, depending on direction of traversal) to
	   the child node, minus the weight of the child node. 

	   The nodes_waiting structure is then sorted by path weight, with the index of nodes 
	   serving as a tie breaker. This heuristic is used to break graph cycles/dependencies; it 
	   seems intuitive that the nodes closest to the starting node have a cycle, whereas the
	   nodes farther from the starting node are more likely to have a dependency on a closer
	   node with a cycle.

	   This is a provisional heuristic for breaking cycles. Its effectiveness has yet to be
	   evaluated (TODO) */
	   //TODO: now actually I use a 3-level sort
	   //	1st level: min path weight through node (by descending order)	//should 1 & 2 be swapped? why this way?
	   //	2nd level: min path weight to source (by ascending order)
	   //	3rd level: pointer of node (by ascending order)

	int child_weight = rr_node[child_ind].get_weight();

	int source_dist = ss_distances[child_ind].get_source_distance();
	int sink_dist = ss_distances[child_ind].get_sink_distance();

	int path_weight = ss_distances[child_ind].get_source_distance() + ss_distances[child_ind].get_sink_distance() - 1*child_weight;//2*child_weight;
	int dist_to_start;

	if (traversal_dir == FORWARD_TRAVERSAL){
		dist_to_start = source_dist - child_weight;
	} else {
		dist_to_start = sink_dist - child_weight;
	}

	Node_Waiting node_waiting;
	node_waiting.set( child_ind, path_weight, dist_to_start );

	nodes_waiting.insert(node_waiting);

	node_topo_inf[child_ind].node_waiting_info = node_waiting;
}

