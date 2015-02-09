#ifndef ANALYSIS_MAIN_H
#define ANALYSIS_MAIN_H

#include "wotan_types.h"

/**** Function Declarations ****/
/* the entry function to performing routability analysis */
void run_analysis(User_Options *user_opts, Analysis_Settings *analysis_settings, Arch_Structs *arch_structs, 
			Routing_Structs *routing_structs);

/* returns a node's demand, less the demand of the specified source/sink connection. if node didn't keep
   history of path counts due to this source/sink connection, then node demand is unmodified */
float get_node_demand_adjusted_for_path_history(int node_ind, t_rr_node &rr_node, int source_ind, int sink_ind, Physical_Type_Descriptor *fill_type,
                                                       User_Options *user_opts);

#endif
