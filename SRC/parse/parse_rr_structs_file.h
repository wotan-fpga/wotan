#ifndef PARSE_RR_STRUCTS_FILE_H
#define PARSE_RR_STRUCTS_FILE_H

#include <string>

/**** Function Declarations ****/
/* Parses the specified rr structs file according the specified rr structs mode */
void parse_rr_structs_file( std::string rr_structs_file, Arch_Structs *arch_structs, Routing_Structs *routing_structs, e_rr_structs_mode rr_structs_mode );

/* If Wotan is being initialized based on an rr structs file then backwards edges/switches need to be determined 
   for each node as a post-processing step. Do this for the pins specified by 'node_type'. if node_type == UNDEFINED,
   then do this for all nodes  */
void initialize_reverse_node_edges_and_switches( Routing_Structs *routing_structs, int node_type );


#endif
