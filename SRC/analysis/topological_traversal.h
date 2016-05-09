#ifndef TOPOLOGICAL_TRAVERSAL_H
#define TOPOLOGICAL_TRAVERSAL_H


#include "wotan_types.h"

/**** Typedefs ****/
/* function type that is executed when node is popped from expansion queue during topological traversal */
typedef void(*t_usr_node_popped_func)(int popped_node, int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data);
/* function type that is executed when topological traversal is complete */
typedef void(*t_usr_traversal_done_func)(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf, 
                          e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data);
/* function type that is executed while iterating over a node's children (execution not guaranteed) */
typedef bool(*t_usr_child_iterated_func)(int parent_ind, int parent_edge_ind, int node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
                          e_traversal_dir traversal_dir, int max_path_weight, int from_node_ind, int to_node_ind, User_Options *user_opts, void *user_data);


/**** Function Declarations ****/

/* 
   Function for doing topological traversal.

   usr_exec_node_popped: function to be executed when a new node is popped off the expansion queue during topological traversal
   usr_exec_child_iterated: to be executed while iterating over a node's children (execution not guaranteed -- based on child legality)
                            can return 'false' to specify that corresponding child should be ignored
   usr_exec_traversal_done: executed after entire topological traversal is complete

   All passed-in function pointers can be NULL
*/
void do_topological_traversal(int from_node_ind, int to_node_ind, t_rr_node &rr_node, t_ss_distances &ss_distances, t_node_topo_inf &node_topo_inf,
			e_traversal_dir traversal_dir, int max_path_weight, User_Options *user_opts, void *user_data,
			t_usr_node_popped_func usr_exec_node_popped,
			t_usr_child_iterated_func usr_exec_child_iterated,
			t_usr_traversal_done_func usr_exec_traversal_done);




#endif
