#include <cstdlib>
#include <cstring>
#include <sstream>
#include <set>
#include "wotan_init.h"
#include "wotan_types.h"
#include "globals.h"
#include "exception.h"
#include "io.h"
#include "draw.h"
#include "parse_rr_structs_file.h"

using namespace std;


/**** Defines ****/
/* the smallest allowable FPGA size (int terms of logic block spans) */
#define MIN_GRID_SIZE_X 5
#define MIN_GRID_SIZE_Y 5


/**** Function Declarations ****/ 
/* Parses the command line options. Options are parsed into the user_opts variable */
static void wotan_parse_command_args(int argc, char **argv, User_Options *user_opts);
/* checks the initialized state of the tool */
static void check_setup( User_Options *user_opts, Arch_Structs *arch_structs, Routing_Structs *routing_structs );
/* Prints instructions for tool usage */
static void wotan_print_usage();
/* Prints intro title for the tool */
static void wotan_print_title();
/* creates a virtual source node for every sink node and links it to the nodes which connect into its ipins.
   these new sources allow for (in effect) enumerating paths from ipins while accounting for input pin equivalence */
void create_virtual_sources(Routing_Structs *routing_structs);
void create_virtual_sources_2(Routing_Structs *routing_structs); // NATHAN
 


/**** Function Definitions ****/

/* Reads in architecture file, as well as user options */
void wotan_init(int argc, char **argv, User_Options *user_opts, Arch_Structs *arch_structs, Routing_Structs *routing_structs,
                Analysis_Settings *analysis_settings){

	wotan_print_title();

	//may be changed when command-line arguments are read-in
	srand(3);

	/* check that we have the minimum number of arguments */
	if (argc < 2){
		wotan_print_usage();
		exit(0);
	}

	/* parse user-specified options into user_opts variable */
	wotan_parse_command_args(argc, argv, user_opts);

	/* parse user-specified rr structs file into Wotan's architecture and routing structures */
	parse_rr_structs_file(user_opts->rr_structs_file, arch_structs, routing_structs, user_opts->rr_structs_mode);

	/* if Wotan structures are initialized from a structures file dumped by VPR, then Wotan 
	   structures aren't complete just yet. need to allocate and set incoming edges for each node.
	   Do this for sinks first, and then for the rest of the nodes later
	   	- Virtual sources are created for sinks, 2nd step necessary to account for those newly-created virtual sources */
	initialize_reverse_node_edges_and_switches(routing_structs, UNDEFINED); 

	/* create virtual sources for all sinks -- this allows (in effect) enumerating of paths from ipins */
	//create_virtual_sources(routing_structs);
	create_virtual_sources_2(routing_structs);

	/* all nodes */
	initialize_reverse_node_edges_and_switches(routing_structs, UNDEFINED); 

	if (user_opts->rr_structs_mode == RR_STRUCTS_VPR){
		/* initialize analysis settings */
		analysis_settings->alloc_and_set_pin_probabilities(user_opts->opin_probability, user_opts->ipin_probability, arch_structs);
		analysis_settings->alloc_and_set_length_probabilities(user_opts);
		analysis_settings->alloc_and_set_test_tile_coords(arch_structs, routing_structs);

		/* initialize path count history structures of rr nodes */
		int fill_type_ind = arch_structs->get_fill_type_index();
		if (user_opts->self_congestion_mode == MODE_RADIUS){
			//TODO: allocate this in analysis_main::alloc_self_congestion_structs
			routing_structs->alloc_rr_node_path_histories( (int)arch_structs->block_type[fill_type_ind].class_inf.size() );
		}

		/* initialize channel widths */
		arch_structs->set_chanwidth( routing_structs );
	}

	/* initialize rr node weights */
	routing_structs->init_rr_node_weights();

	/* check initialized state */
	check_setup(user_opts, arch_structs, routing_structs);

	/* initialize graphics */
	if (user_opts->nodisp == false){
		int max_block_pins = 0;
		int num_block_types = arch_structs->get_num_block_types();
		for (int itype = 0; itype < num_block_types; itype++){
			int num_type_pins = arch_structs->block_type[itype].get_num_pins();
			if (num_type_pins > max_block_pins){
				max_block_pins = num_type_pins;
			}
		}
		init_draw_coords((float)max_block_pins, routing_structs, arch_structs);
		init_graphics("Wotan v0.1", WHITE);

		update_screen(routing_structs, arch_structs, user_opts);
	}

	return;
}


/* Parses the command line options. Options are parsed into the user_opts variable */
static void wotan_parse_command_args(int argc, char **argv, User_Options *user_opts){

	int iopt = 0;
	/* first parameter is the name of the program -- skip */
	iopt++;	

	/* next we read in the options */
	for ( ; iopt < argc; iopt++ ){
		if ( strcmp(argv[iopt], "-rr_structs_file") == 0 ){
			/* Wotan structures to be initialized based on a dumped VPR structures file */
			iopt++;
			
			if (iopt >= argc){
				WTHROW(EX_INIT, "Expected an argument for the -rr_structs_file option");
			}

			if (user_opts->rr_structs_mode == RR_STRUCTS_UNDEFINED){
				user_opts->rr_structs_mode = RR_STRUCTS_VPR;	//if hasn't been set yet, then set the default. may be changed by a later cmd-line argument
			}
			user_opts->rr_structs_file = argv[iopt];
		} else if ( strcmp(argv[iopt], "-rr_structs_mode") == 0 ){
			/* specifies mode in which the rr structs file is expected to be */
			iopt++;

			if (iopt >= argc){
				WTHROW(EX_INIT, "Expected an argument for the -rr_structs_mode option");
			}

			if ( strcmp(argv[iopt], "VPR") == 0 ){
				cout << "Analyzing VPR structs." << endl;
				user_opts->rr_structs_mode = RR_STRUCTS_VPR;
			} else if ( strcmp(argv[iopt], "simple") == 0 ){
				cout << "Analyzing basic structs." << endl;
				user_opts->rr_structs_mode = RR_STRUCTS_SIMPLE;
			} else {
				WTHROW(EX_INIT, "Unrecognized rr structs mode: " << argv[iopt]);
			}
		} else if ( strcmp(argv[iopt], "-threads") == 0 ){
			/* number of threads to use during path enumeration */
			iopt++;

			if (iopt >= argc){
				WTHROW(EX_INIT, "Expected an argument for the -threads option");
			}

			user_opts->num_threads = atoi(argv[iopt]);
		} else if ( strcmp(argv[iopt], "-max_connection_length") == 0 ){
			/* maximum connection length to consider during path enumeration */
			iopt++;

			if (iopt >= argc){
				WTHROW(EX_INIT, "Expected an argument for the -max_connection_length option");
			}
			
			user_opts->max_connection_length = atoi(argv[iopt]);
		} else if ( strcmp(argv[iopt], "-analyze_core") == 0 ){
			/* reachability analysis will only be performed for a core region of the FPGA (though paths are still enumerated everywhere) */
			iopt++;

			if (iopt >= argc){
				WTHROW(EX_INIT, "Expected a y/n argument for the -analyze_core option");
			}

			if ( strcmp(argv[iopt], "y") == 0 ){
				user_opts->analyze_core = true;
			} else if ( strcmp(argv[iopt], "n") == 0 ){
				user_opts->analyze_core = false;
			} else {
				WTHROW(EX_INIT, "-analyze_core option needs y/n argument");
			}
		} else if ( strcmp(argv[iopt], "-use_routing_node_demand") == 0 ){
			/* The demand for routing nodes (CHANX, CHANY) will be considered to be whatever is specified. Demands for all
			   other node types will be considered to be 0. */
			iopt++;

			if (iopt >= argc){
				WTHROW(EX_INIT, "Expected an argument for the -use_routing_node_demand option");
			}

			stringstream ss;
			ss << argv[iopt];
			float routing_node_demand;
			ss >> routing_node_demand;

			if (routing_node_demand <= 0){
				WTHROW(EX_INIT, "Expected specified routing node demand (for '-use_routing_node_demand' argument) to be > 0. Got " << 
						routing_node_demand);
			}

			user_opts->use_routing_node_demand = routing_node_demand;
		} else if ( strcmp(argv[iopt], "-opin_demand") == 0 ){
			/* sets opin demand to the specified value */
			iopt++;

			if (iopt >= argc){
				WTHROW(EX_INIT, "Expected an argument for the -opin_demand option");
			}

			stringstream ss;
			ss << argv[iopt];
			float opin_demand;
			ss >> opin_demand;

			if (opin_demand < 0){
				WTHROW(EX_INIT, "Expected opin demand to be >= 0. Got " << 
						opin_demand);
			}

			user_opts->opin_probability = opin_demand;
		} else if ( strcmp(argv[iopt], "-demand_multiplier") == 0 ){
			/* sets demand multiplier according to the specified value */
			iopt++;

			if (iopt >= argc){
				WTHROW(EX_INIT, "Expected an argument for the -demand_multiplier option");
			}
	
			stringstream ss;
			ss << argv[iopt];
			float demand_multiplier;
			ss >> demand_multiplier;

			if (demand_multiplier <= 0){
				WTHROW(EX_INIT, "Expected demand multiplier to be > 0. Got " << demand_multiplier);
			}

			user_opts->demand_multiplier = demand_multiplier;
		} else if ( strcmp(argv[iopt], "-search_for_reliability") == 0 ){
			/* adjusts demand multiplier until the target value of reliability is found */
			iopt++;

			if (iopt >= argc){
				WTHROW(EX_INIT, "Expected an argument for the -search_for_reliability option");
			}

			stringstream ss;
			ss << argv[iopt];
			float target_reliability;
			ss >> target_reliability;

			if (target_reliability <= 0 || target_reliability > 1.0){
				WTHROW(EX_INIT, "Expected an argument between 0 and 1. Got " << target_reliability);
			}

			user_opts->target_reliability = target_reliability;
		} else if ( strcmp(argv[iopt], "-self_congestion") == 0 ){
			/* method to deal with self congestion */
			iopt++;

			if (iopt >= argc){
				WTHROW(EX_INIT, "Expected an argument for the -self_congestion option");
			}

			if ( strcmp(argv[iopt], "none") == 0 ){
				user_opts->self_congestion_mode = MODE_NONE;
			} else if ( strcmp(argv[iopt], "radius") == 0 ){
				user_opts->self_congestion_mode = MODE_RADIUS;
			} else if ( strcmp(argv[iopt], "path_dependence") == 0 ){
				user_opts->self_congestion_mode = MODE_PATH_DEPENDENCE;
			} else {
				WTHROW(EX_INIT, "Unrecognized self_congestion mode: " << argv[iopt]);
			}
		} else if ( strcmp(argv[iopt], "-seed") == 0 ){
			/* seed for random numbers */
			iopt++;

			if (iopt >= argc){
				WTHROW(EX_INIT, "Expected an argument for the -seed option");
			}

			stringstream ss;
			ss << argv[iopt];
			unsigned int seed;
			ss >> seed;

			srand(seed);
		} else if ( strcmp(argv[iopt], "-nodisp") == 0 ){
			/* no graphics */
			user_opts->nodisp = true;
		} else {
			WTHROW(EX_INIT, "unrecognized command-line option: " << argv[iopt]);
		}
	}

	return;
}

/* Prints intro title for the tool */
static void wotan_print_title(){
	cout << "===============================================" << endl;
	cout << "                   Wotan v0.1                  " << endl;
	cout << "   FPGA Routing Architecture Evaluation Tool   " << endl;
	cout << "===============================================" << endl;
	cout << endl;
}

/* Prints instructions for tool usage */
static void wotan_print_usage(){
	cout << "Wotan is a tool for early-stage FPGA routing architecture evaluation without benchmarks." << endl;
	cout << "Path enumeration inside a small FPGA test area is used to get a sense of the congestion patterns" << endl;
	cout << "that the routing architecture is susceptible to, and these congestion patterns are then used to" << endl;
	cout << "perform reachability analysis on different source/sink pairs." << endl << endl;
	
	cout << "Usage:" << endl;
	cout << "\t./wotan -rr_structs_file <file_path> [-rr_structs_mode <VPR/simple>] [-threads <num_threads>] [-max_connection_length <max_length>]" << endl <<
		"\t\t[-analyze_core <y/n>] [-use_routing_node_demand <demand>]" << endl <<
		"\t\t[-demand_multiplier <multiplier>] [-self_congestion_mode <none/radius/path_dependence>] [-seed <value>] [-nodisp]" << endl << endl;

	cout << "Options:" << endl;

	cout << "\t-rr_structs_file: used to specify a path to the structs file based on which Wotan will be initialized" << endl << endl;

	cout << "\t-rr_structs_mode: used to specify what 'mode' Wotan should expect the rr_structs_file to be in. The allowed modes are:" << endl <<
		"\t\tVPR -- expect rr structs file to contain dumped structures from VPR (default)" << endl <<
		"\t\tsimple -- expect rr structs file to contain only the rr_node section (in the same format as for the dumped VPR structures file) with" << endl <<
		"\t\t\tonly one source node and one sink node. This is useful for debugging and analyzing custom graphs" << endl << endl;

	cout << "\t-threads: used to specify the number of threads to be used during the path enumeration and probability analysis steps (default is 1)" << endl << endl;

	cout << "\t-max_connection_length: the maximum allowed connection length for path enumeration (default is 3)" << endl << endl;

	cout << "\t-analyze_core: if set, reachability analysis will only be performed for a core region of the FPGA;" << endl;
	cout << "\t\tpath enumeration is still performed everywhere (enabled by default)" << endl << endl;

	cout << "\t-use_routing_node_demand: if specified, routing nodes (CHANX/CHANY) will be treated as having the specified demand; nodes of other" << endl;
	cout << "\t\ttypes will be treated as having a demand of 0 (disabled by default)" << endl << endl;

	cout << "\t-demand_multiplier: if specified this scaling factor will be applied to node demands (except ipin/opin/source/sink)" << endl << endl;

	cout << "\t-self_congestion: specify the mode used to deal with self-congestion effects. Demands enumerated from a source to a" << endl;
	cout << "\t                  sink can conflict with routing probability analysis from that source to that sink. i.e. if the" << endl;
	cout << "\t                  output pin associated with the source has a demand of 1.0, all connections evaluated from the source" << endl;
	cout << "\t                  will have a routing probability of 0 unless the demand contributed to that pin by the source is discounted" << endl;
	cout << "\t\tnone -- do not deal with self-congestion (default)" << endl;
	cout << "\t\tradius -- Each source (sink) keeps track of demands contributed to nearby nodes within some specified Manhattan distance." << endl;
	cout << "\t\t          These demands can then be discounted when analyzing probability from that source (to that sink)." << endl;
	cout << "\t\tpath_dependence -- More complicated but more accurate. Each node keeps track of demands contributed to it by its children." << endl;
	cout << "\t\t                   Child node demands are then discounted routing probability analysis traverses from the respective child" << endl;
	cout << "\t\t                   node to this one. This mode uses significantly more memory." << endl << endl;

	//Commenting. This doesn't really work.
	//cout << "\t-search_for_reliability: if specified, wotan will search for the demand_multiplier value required to achieve the specified value of reliability." << endl;
	//cout << "\t\tany values specified with the -demand_multiplier option will be ignored." << endl << endl;

	cout << "\t-seed: specified the seed for the random number generator" << endl << endl;

	cout << "\t-nodisp: if specified, graphics will be disabled (graphics are enabled by default)" << endl << endl;
}

/* checks the initialized state of the tool */
static void check_setup( User_Options *user_opts, Arch_Structs *arch_structs, Routing_Structs *routing_structs ){
	if (user_opts->rr_structs_mode == RR_STRUCTS_UNDEFINED){
		WTHROW(EX_INIT, "Currently Wotan can only be initialized by reading a routing-resource structures file");
	}

	/* check that the FPGA architecture consists only of CLB blocks (except for the perimeter which is unavoidably I/O);
	   only homogeneous architectures (in terms of block types) are allowed for now */
	int grid_size_x, grid_size_y;
	arch_structs->get_grid_size(&grid_size_x, &grid_size_y);

	if (user_opts->rr_structs_mode == RR_STRUCTS_VPR){
		if (grid_size_x < MIN_GRID_SIZE_X || grid_size_y < MIN_GRID_SIZE_Y){
			WTHROW(EX_INIT, "Minimum allowed FPGA size is " << MIN_GRID_SIZE_X << " by " << MIN_GRID_SIZE_Y  << " logic block spans. " <<
					"Specified FPGA size is " << grid_size_x << " by " << grid_size_y << endl);
		}
	}

	int fill_type_ind = arch_structs->get_fill_type_index();
	for (int ix = 1; ix < grid_size_x-1; ix++){
		for (int iy = 1; iy < grid_size_y-1; iy++){
			int block_type_ind = arch_structs->grid[ix][iy].get_type_index();
			//cout << "(" << ix << "," << iy << "): " << arch_structs->block_type[block_type_ind].get_name() << 
			//	"  height " << arch_structs->grid[ix][iy].get_height_offset() << "  width " << arch_structs->grid[ix][iy].get_width_offset() << endl;
			if (block_type_ind != fill_type_ind){
				WTHROW(EX_INIT, "Except for I/O blocks on the perimeter of the FPGA, only logic blocks are allowed. " << endl <<
						"Determined logic block type to be '" << arch_structs->block_type[fill_type_ind].get_name() << "'" << endl <<
						"But found a block '" << arch_structs->block_type[block_type_ind].get_name() << "' in the interior of the FPGA" << endl <<
						"Currently only a homogeneous set of blocks (with exception of peripheral I/O) is allowed" << endl);
			}
		}
	}

	/* check that the number of threads to be used during analysis is greater than 0 */
	if (user_opts->num_threads <= 0){
		WTHROW(EX_INIT, "Number of threads to be used during path enumeration has to be greater than 0");
	}

	/* if user wants a specific routing node demand (via -use_routing_node_demand) option, then path count histories should not be kept */
	if (user_opts->use_routing_node_demand > 0){
		if (user_opts->self_congestion_mode != MODE_NONE){
			WTHROW(EX_INIT, "Only the 'none' self-congestion method is allowed if the -use_routing_node_demand option is used.");
		}
	}

	/* if user selected rr_structs_mode to be 'simple', then graphics should be disabled and the -use_routing_node_demand option should have been specified */
	if (user_opts->rr_structs_mode == RR_STRUCTS_SIMPLE){
		if (user_opts->nodisp == false){
			WTHROW(EX_INIT, "No display mode currently supported for use with the '-rr_structs_mode simple' option. Please use the -nodisp option to disable graphics");
		}

		if (user_opts->use_routing_node_demand != UNDEFINED){
			WTHROW(EX_INIT, "The -use_routing_node_demand option does currently does not work.");
		}
		//if (user_opts->use_routing_node_demand < 0){
		//	WTHROW(EX_INIT, "The '-rr_structs_mode simple' option currently requires that the '-use_routing_node_demand' option be used to specify a positive " << 
		//	                 "demand for routing nodes");
		//}
	}
}


/* creates a virtual source node for every sink node and links it to the nodes which connect into its ipins.
   these new sources allow for (in effect) enumerating paths from ipins while accounting for input pin equivalence */
void create_virtual_sources(Routing_Structs *routing_structs){
	int num_nodes = routing_structs->get_num_rr_nodes();

	t_rr_node &rr_node = routing_structs->rr_node;

	/* find and act on sink nodes */
	for (int inode = 0; inode < num_nodes; inode++){
		/* skip nodes that aren't sinks */
		if (rr_node[inode].get_rr_type() != SINK){
			continue;
		}

		RR_Node &sink_node = rr_node[inode];	//note if rr_node struct is changed this might get invalidated...

		/* some properties of the sink to be copied */
		int num_in_edges_sink = sink_node.get_num_in_edges();
		int ptc = sink_node.get_ptc_num();
		short x1, y1, x2, y2;
		x1 = sink_node.get_xlow();
		y1 = sink_node.get_ylow();
		x2 = sink_node.get_xhigh();
		y2 = sink_node.get_yhigh();


		if (num_in_edges_sink <= 0){
			WTHROW(EX_INIT, "Found sink node (" << inode << ") with no incoming edges");
		}

		/* unique channel node indices reachable (backwards) by the sink (through ipins) */
		set<int> channel_nodes;

		/* iterate over each ipin connecting into the sink. want to mark the nodes that connect into the ipins */
		int node_ind;
		for (int iedge_sink = 0; iedge_sink < num_in_edges_sink; iedge_sink++){
			node_ind = sink_node.in_edges[ iedge_sink ];
			RR_Node &ipin_node = rr_node[ node_ind ];

			/* skip non-ipin nodes */
			if (ipin_node.get_rr_type() != IPIN){
				continue;
			}

			int *ipin_edges = ipin_node.in_edges;
			int num_ipin_edges = ipin_node.get_num_in_edges();

			/* mark unique nodes which connect into the ipin */
			for (int iedge_ipin = 0; iedge_ipin < num_ipin_edges; iedge_ipin++){
				node_ind = ipin_edges[iedge_ipin];

				channel_nodes.insert( node_ind );
			}
		}

		/* create a virtual source that will have outgoing edges to those chanx/chany nodes immediately reachable (backwards) by the sink (through ipins) */
		rr_node.push_back(RR_Node());
		RR_Node &new_node = rr_node.back();
		//RR_Node new_node;
		new_node.set_is_virtual_source(true);
		new_node.set_rr_type(SOURCE);
		new_node.set_coordinates(x1, y1, x2, y2);
		new_node.set_ptc_num(ptc);

		/* we have found unique nodes which connect into the ipins (that then connect into the sink). add these nodes as out-edges for
		   our new virtual source */
		int num_channel_nodes = (int)channel_nodes.size();
		new_node.alloc_out_edges_and_switches( num_channel_nodes );
		int iedge = 0;
		for ( int chan_node_ind : channel_nodes ){
			new_node.out_edges[iedge] = chan_node_ind;
			iedge++;
		}

		/* insert new node into the rr_node structure */
		//rr_node.push_back(new_node);
		int new_node_index = (int)rr_node.size()-1;

		/* mark the sink node with the index of this new virtual source */
		rr_node[inode].set_virtual_source_node_ind( new_node_index );		//using rr_node instead of sink_node reference because rr_node vector changed
	}
}

// This function differs from the other one because it only constructs virtual sources for CHANX/Y nodes
// NATHAN
void create_virtual_sources_2(Routing_Structs *routing_structs)
{
	// DEBUG FIX
	cout << "Creating virtual sources" << endl;

	int num_nodes = routing_structs->get_num_rr_nodes();
	t_rr_node &rr_node = routing_structs->rr_node;

	// Find and act on CHANX/CHANY nodes
	for (int inode = 0; inode < num_nodes; inode++)
	{
		// Skip nodes that aren't CHANX/CHANY
		if ((rr_node[inode].get_rr_type() != CHANX) && (rr_node[inode].get_rr_type() != CHANY)) {
			continue;
		}

		// DEBUG FIX
		//cout << "Adding virtual source for node " << inode << endl;

		RR_Node &my_node = rr_node[inode];	// Note if rr_node struct is changed this might get invalidated...

		// Some properties of the node to be copied
		int ptc = my_node.get_ptc_num();
		int num_in_edges_chan = my_node.get_num_in_edges();
		int num_out_edges = (int) my_node.get_num_out_edges();
		short x1, y1, x2, y2;
		x1 = my_node.get_xlow();
		y1 = my_node.get_ylow();
		x2 = my_node.get_xhigh();
		y2 = my_node.get_yhigh();

		// Get the out edges before rr_node gets modified by the new_node
		vector<int> my_node_out_edges;
		vector<int> my_node_out_switches;
		for (int i = 0; i < num_out_edges; i++) {
			my_node_out_edges.push_back(my_node.out_edges[i]);
			my_node_out_switches.push_back(my_node.out_switches[i]);
		}

		if (num_in_edges_chan <= 0) {
			WTHROW(EX_INIT, "Found CHANX/Y node (" << inode << ") with no incoming edges");
		}

		rr_node.push_back(RR_Node());
		RR_Node &new_node = rr_node.back();
		new_node.set_is_virtual_source(true);
		new_node.set_rr_type(SOURCE);
		new_node.set_coordinates(x1, y1, x2, y2);
		new_node.set_ptc_num(ptc);
		new_node.set_regular_source_node_ind(inode);

		new_node.alloc_out_edges_and_switches( num_out_edges );
		for (int iedge = 0; iedge < num_out_edges; iedge++)
		{
			new_node.out_edges[iedge] = my_node_out_edges[iedge];
			new_node.out_switches[iedge] = my_node_out_switches[iedge];
		}

		// Insert new node into the rr_node structure
		int new_node_index = (int) rr_node.size()-1;

		// Mark the sink node with the index of this new virtual source
		// Using rr_node instead of my_node reference because rr_node vector changed
		rr_node[inode].set_virtual_source_node_ind( new_node_index );
	}
}

