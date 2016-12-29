
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "exception.h"
#include "io.h"
#include "wotan_types.h"
#include "wotan_util.h"
#include "parse_rr_structs_file.h"

using namespace std;


/**** Enums ****/
/* enums for the different sections of the dumped VPR structs file */
enum e_file_section{
	NODE_SECTION = 0,
	SWITCH_SECTION,
	BLOCK_TYPE_SECTION,
	GRID_SECTION,
	NODE_INDICES_SECTION,
	IN_BETWEEN_SECTIONS,	/* not in any section (i.e. there's >1 carriage returns between sections */
	NUM_SECTIONS	
};


/**** Function Declarations ****/
/* returns the file section which is just about to begin based on that section's 'header' line */
static e_file_section get_line_section(string header_line);
/* parses lines of the referenced file, expecting that they belong to the specified section.
   continues parsing until the .end directive is hit. 
   creates the structure(s) into which the section is parsed */
static void make_struct_and_parse_section(e_file_section section, string header_line, fstream &file, 
		Arch_Structs *arch_structs, Routing_Structs *routing_structs);
/* parses rr node section of file into created rr_node structure */
static void parse_rr_node_section(int num_rr_nodes, t_rr_node &rr_node, fstream &file);
/* parses rr switch section of file into created rr_switch_inf structure */
static void parse_rr_switch_inf_section(int num_rr_switches, t_rr_switch_inf &rr_switch_inf, fstream &file);
/* parses block types section of file into created block_type structure */
static void parse_block_type_section(int num_block_types, t_block_type &block_type, fstream &file);
/* parses grid section of file into the created grid structure */
static void parse_grid_section(int x_size, int y_size, t_grid &grid, fstream &file);
/* parses rr node indices section of file into the created rr_node_index structure */
static void parse_rr_node_index_section(int num_rr_types, int x_size, int y_size, t_rr_node_index &rr_node_index, fstream &file);
/* checks whether an sscanf function read as many arguments as were expected and throws an exception if not. 'line' is the line that was scanned */
static void check_expected_vs_read(int num_expected, int num_read, string line);

/**** Function Definitions ****/
/* Parses the specified rr structs file according the specified rr structs mode */
void parse_rr_structs_file( std::string rr_structs_file, Arch_Structs *arch_structs, Routing_Structs *routing_structs, e_rr_structs_mode rr_structs_mode ){

	cout << "Parsing structs file (" << rr_structs_file << ") in mode " << g_rr_structs_mode_string[rr_structs_mode] << endl;

	/* open the file for reading */
	fstream file;
	open_file(&file, rr_structs_file, ios::in);

	/* the rr structs file contains data to fill Wotan's routing and architecture structures. based on the rr_structs_mode, Wotan expects
	   the rr structs file to contain different subsets of structures (see comment of the e_rr_structs_mode enum) */

	/* step through each line of the rr_structs_file and parse it */
	string section_line;
	while ( getline(file, section_line) ){
		e_file_section section = get_line_section(section_line);

		if (rr_structs_mode == RR_STRUCTS_SIMPLE){
			if (section != NODE_SECTION && section != IN_BETWEEN_SECTIONS){
				WTHROW(EX_INIT, "If the rr_structs_mode is specified as 'simple', the rr_structs_file should only contain " <<
				                 "a list of nodes");
			}
		}

		make_struct_and_parse_section(section, section_line, file, arch_structs, routing_structs);
	}
}

/* returns the file section which is just about to begin based on that section's 'header' line */
static e_file_section get_line_section(string header_line){
	e_file_section section;

	/* The specified line must either be a section header line, or an empty line (in between sections) */ 
	if (header_line.empty()){
		section = IN_BETWEEN_SECTIONS;
	} else if ( contains_substring(header_line, ".rr_node(") ){
		section = NODE_SECTION;
	} else if ( contains_substring(header_line, ".rr_switch(") ){
		section = SWITCH_SECTION;
	} else if ( contains_substring(header_line, ".block_type(") ){
		section = BLOCK_TYPE_SECTION;
	} else if ( contains_substring(header_line, ".grid(") ){
		section = GRID_SECTION;
	} else if ( contains_substring(header_line, ".rr_node_indices(") ){
		section = NODE_INDICES_SECTION;
	} else {
		WTHROW(EX_INIT, "The specified line is not a section header line in a VPR dumped structs file:\n" << header_line);
	}

	return section;
}


/* parses lines of the referenced file, expecting that they belong to the specified section.
   continues parsing until the .end directive is hit. 
   creates the structure(s) into which the section is parsed */
static void make_struct_and_parse_section(e_file_section section, string header_line, fstream &file, 
		Arch_Structs *arch_structs, Routing_Structs *routing_structs){

	/* check which section of the dumped VPR structs file the header line represents, create
	   the corresponding data structure, and then parse that section */
	if (section == NODE_SECTION){
		/* rr nodes */
		int num_rr_nodes;
		t_rr_node &rr_node = routing_structs->rr_node;
		sscanf(header_line.c_str(), ".rr_node(%d)", (&num_rr_nodes));

		routing_structs->alloc_and_create_rr_node(num_rr_nodes);

		parse_rr_node_section(num_rr_nodes, rr_node, file);

	} else if (section == SWITCH_SECTION) {
		/* rr switch inf */
		int num_rr_switches;
		t_rr_switch_inf &rr_switch_inf = routing_structs->rr_switch_inf;
		sscanf(header_line.c_str(), ".rr_switch(%d)", (&num_rr_switches));

		routing_structs->alloc_and_create_rr_switch_inf(num_rr_switches);

		parse_rr_switch_inf_section(num_rr_switches, rr_switch_inf, file);

	} else if (section == BLOCK_TYPE_SECTION) {
		/* physical block types */
		int num_block_types;
		t_block_type &block_type = arch_structs->block_type;
		sscanf(header_line.c_str(), ".block_type(%d)", (&num_block_types));
	
		arch_structs->alloc_and_create_block_type(num_block_types);		

		parse_block_type_section(num_block_types, block_type, file);

	} else if (section == GRID_SECTION) {
		/* FPGA grid (which block type is at what location */
		int x_size, y_size;
		t_grid &grid = arch_structs->grid;
		sscanf(header_line.c_str(), ".grid(%d, %d)", (&x_size), (&y_size));

		arch_structs->alloc_and_create_grid(x_size, y_size);

		parse_grid_section(x_size, y_size, grid, file);

		/* determine what the block 'fill' type is for the grid (i.e. which block type index corresponds to the logic block) */
		arch_structs->set_fill_type();
		//int fill_type = arch_structs->get_fill_type_index();
		//cout << "Determined logic block to be of type named '" << arch_structs->block_type[fill_type].get_name() << "'" << endl;

	} else if (section == NODE_INDICES_SECTION) {
		/* rr node indices */
		int num_rr_types, x_size, y_size;
		t_rr_node_index &rr_node_index = routing_structs->rr_node_index;
		sscanf(header_line.c_str(), ".rr_node_indices(%d, %d, %d)", (&num_rr_types), (&x_size), (&y_size));
		if (num_rr_types != NUM_RR_TYPES){
			WTHROW(EX_INIT, "The specified number of rr types in the dumped VPR structs file (" << num_rr_types << 
			       ") does not equal to the number of rr types in Wotan (" << NUM_RR_TYPES << ")");
		}

		routing_structs->alloc_and_create_rr_node_index(num_rr_types, x_size, y_size);

		parse_rr_node_index_section(num_rr_types, x_size, y_size, rr_node_index, file);

	} else if (section == IN_BETWEEN_SECTIONS) {
		/* header line empty -- not in any section. can skip */

	} else {
		WTHROW(EX_INIT, "Unexpected file section: " << section);
	}
}

/* parses rr node section of file into created rr_node structure */
static void parse_rr_node_section(int num_rr_nodes, t_rr_node &rr_node, fstream &file){

	int inode = 0;
	string line;
	getline(file, line);
	while (line != ".end rr_node"){

		int num_read;
		int num_expected;

		int node_num;
		int xlow, ylow, xhigh, yhigh, ptc_num, fan_in;
		int direction;
		e_rr_type rr_type;
		float R, C;
		char rr_type_buf[20];

		/* scan-in values from the line.  */
		num_expected = 11;
		num_read = sscanf(line.c_str(), " node_%d: rr_type(%[^()]) xlow(%d) xhigh(%d) ylow(%d) yhigh(%d) ptc_num(%d) fan_in(%d) direction(%d) R(%f) C(%f)",
				&node_num, rr_type_buf, &xlow, &xhigh, &ylow, &yhigh, &ptc_num, &fan_in, 
				&direction, &R, &C);

		check_expected_vs_read(num_expected, num_read, line);			

		/* check rr type */
		if (0 == strcmp(rr_type_buf, "SINK")){
			rr_type = SINK;
		} else if (0 == strcmp(rr_type_buf, "SOURCE")){
			rr_type = SOURCE;
		} else if (0 == strcmp(rr_type_buf, "IPIN")){
			rr_type = IPIN;
		} else if (0 == strcmp(rr_type_buf, "OPIN")){
			rr_type = OPIN;
		} else if (0 == strcmp(rr_type_buf, "CHANX")){
			rr_type = CHANX;
		} else if (0 == strcmp(rr_type_buf, "CHANY")){
			rr_type = CHANY;
		} else {
			WTHROW(EX_INIT, "Unexpected rr_type: " << rr_type_buf);
		}

		if (inode != node_num){
			WTHROW(EX_INIT, "Expected the dumped rr nodes to be in ascending order by index");
		}

		/* assign values to rr node */
		rr_node[inode].set_rr_type(rr_type);
		rr_node[inode].set_coordinates(xlow, ylow, xhigh, yhigh);
		rr_node[inode].set_R(R);
		rr_node[inode].set_C(C);
		rr_node[inode].set_ptc_num(ptc_num);
		rr_node[inode].set_fan_in(fan_in);

		rr_node[inode].set_direction((e_direction)direction);


		/* the next line tells us how many edges there are */
		getline(file, line);
		int num_edges;
		sscanf(line.c_str(), "  .edges(%d)", &num_edges);

		/* allocate the edge and switch arrays */
		rr_node[inode].alloc_out_edges_and_switches(num_edges);

		/* the subsequent lines list all the edges, and which switch an edge uses */
		int iedge = 0;
		getline(file, line);
		while (line != "  .end edges"){

			if (iedge >= num_edges){
				WTHROW(EX_INIT, "Expected number of edges to not exceed " << num_edges);
			}

			int edge_num, edge, sw;

			num_expected = 3;
			num_read = sscanf(line.c_str(), "   %d: edge(%d) switch(%d)", &edge_num, &edge, &sw);

			check_expected_vs_read(num_expected, num_read, line);			

			if (iedge != edge_num){
				WTHROW(EX_INIT, "Expected edges of dumped rr nodes to be printed in ascending order by index");
			}

			rr_node[inode].out_edges[iedge] = edge;
			rr_node[inode].out_switches[iedge] = sw;

			iedge++;
			getline(file, line);
		}

		inode++;
		getline(file, line);
	}
}

/* parses rr switch section of file into created rr_switch_inf structure */
static void parse_rr_switch_inf_section(int num_rr_switches, t_rr_switch_inf &rr_switch_inf, fstream &file){

	int iswitch = 0;
	string line;
	getline(file, line);		
	/* parse the rr switch inf section */
	while (line != ".end rr_switch"){

		int num_read;
		int num_expected;

		int switch_num, buffered;
		float R, Cin, Cout, Tdel, mux_trans_size, buf_size;

		num_expected = 8;
		num_read = sscanf(line.c_str(), " switch_%d: buffered(%d) R(%f) Cin(%f) Cout(%f) Tdel(%f) mux_trans_size(%f) buf_size(%f)",
				&switch_num, &buffered, &R, &Cin, &Cout, &Tdel, &mux_trans_size, &buf_size);

		check_expected_vs_read(num_expected, num_read, line);			

		if (iswitch != switch_num){
			WTHROW(EX_INIT, "Expected rr switch infs to be printed in ascending order by index.");
		}

		/* now assign variables to the switch inf entry */
		rr_switch_inf[iswitch].set_R(R);
		rr_switch_inf[iswitch].set_Cin(Cin);
		rr_switch_inf[iswitch].set_Cout(Cout);
		rr_switch_inf[iswitch].set_Tdel(Tdel);
		rr_switch_inf[iswitch].set_mux_trans_size(mux_trans_size);
		rr_switch_inf[iswitch].set_buf_size(buf_size);
		rr_switch_inf[iswitch].set_buffered((bool)buffered);

		iswitch++;
		getline(file, line);		
	}
}

/* parses block types section of file into created block_type structure */
static void parse_block_type_section(int num_block_types, t_block_type &block_type, fstream &file){

	int itype = 0;
	string line;
	getline(file, line);
	while(line != ".end block_type"){

		int num_read, num_expected;

		int type_num, num_pins, width, height, num_class, num_drivers, num_receivers, index;
		char name_buf[40];

		num_expected = 9;
		num_read = sscanf(line.c_str(), " type_%d: name(%[^()]) num_pins(%d) width(%d) height (%d) num_class(%d) num_drivers(%d) num_receivers(%d) index(%d)",
						&type_num, name_buf, &num_pins, &width, &height, &num_class, &num_drivers, &num_receivers, &index);

		check_expected_vs_read(num_expected, num_read, line);			

		if (itype != type_num){
			WTHROW(EX_INIT, "Expected block types to be printed in ascending order by index");
		}

		/* assign values to block type */
		block_type[itype].set_name( string(name_buf) );
		block_type[itype].set_index(index);
		block_type[itype].set_num_pins(num_pins);
		block_type[itype].set_width(width);
		block_type[itype].set_height(height);
		block_type[itype].set_num_drivers(num_drivers);
		block_type[itype].set_num_receivers(num_receivers);

		/* next read-in the class info arrays */
		block_type[itype].class_inf.assign(num_class, Pin_Class());
		int iclass = 0;
		getline(file, line);	//header line for class inf section
		getline(file, line);	//first line in class inf section
		while(line != "  .end classes"){
			int class_num, pin_type, num_class_pins;

			num_expected = 3;
			num_read = sscanf(line.c_str(), "   %d: pin_type(%d) num_pins(%d)", &class_num, &pin_type, &num_class_pins);

			check_expected_vs_read(num_expected, num_read, line);			

			if (iclass != class_num){
				WTHROW(EX_INIT, "Expected class infs to be printed in ascending order by index");
			}
			
			/* assign pin type */
			block_type[itype].class_inf[iclass].set_pin_type((e_pin_type)pin_type);

			/* create the class pins */
			block_type[itype].class_inf[iclass].pinlist.assign(num_class_pins, UNDEFINED);
	
			/* now read the pin list */
			int ipin = 0;
			getline(file, line);	//header line for pinlist section
			getline(file, line);	//first line in pinlist section
			while(line != "    .end pinlist"){

				int pin_num, pin;

				num_expected = 2;
				num_read = sscanf(line.c_str(), "     %d: %d", &pin_num, &pin);

				check_expected_vs_read(num_expected, num_read, line);

				if (ipin != pin_num){
					WTHROW(EX_INIT, "Expected class pins to be printed in ascending order by index");
				}

				/* assign pin */
				block_type[itype].class_inf[iclass].pinlist[ipin] = pin;

				ipin++;
				getline(file, line);
			}

			iclass++;
			getline(file, line);
		}	

		/* next read in which pin belongs to which class */
		block_type[itype].pin_class.assign(num_pins, UNDEFINED);
		int ipin = 0;
		getline(file, line);	//header line for pin_class section
		getline(file, line);	//first line of pin_class section
		while(line != "  .end pin_class"){
			int pin_num, pin_class;

			num_expected = 2;
			num_read = sscanf(line.c_str(), "   %d: %d", &pin_num, &pin_class);

			check_expected_vs_read(num_expected, num_read, line);

			if (pin_num != ipin){
				WTHROW(EX_INIT, "Expected pin_class pins to be printed in ascending order by index");
			}

			block_type[itype].pin_class[ipin] = pin_class;

			ipin++;
			getline(file, line);
		}
		
		/* finally, read in which pins are global */
		block_type[itype].is_global_pin.assign(num_pins, true);
		ipin = 0;
		getline(file, line);	//header line for is_global_pin section
		getline(file, line);	//first line of is_global_pin section
		while(line != "  .end is_global_pin"){
			int pin_num, is_global;

			num_expected = 2;
			num_read = sscanf(line.c_str(), "   %d: %d", &pin_num, &is_global);

			check_expected_vs_read(num_expected, num_read, line);

			if (pin_num != ipin){
				WTHROW(EX_INIT, "Expected is_global_pin pins to be printed in ascending order by index");
			}

			block_type[itype].is_global_pin[ipin] = (bool) is_global;

			ipin++;
			getline(file, line);
		}

		itype++;
		getline(file, line);
	}
}

/* parses grid section of file into the created grid structure */
static void parse_grid_section(int x_size, int y_size, t_grid &grid, fstream &file){
	
	int expected_grid_elements = x_size*y_size;
	int read_grid_elements = 0;
	
	string line;
	getline(file, line);	//first line of the grid section
	while (line != ".end grid"){
		int num_expected, num_read;
		int x, y, block_type_index, width_offset, height_offset;

		num_expected = 5;
		num_read = sscanf(line.c_str(), " grid_x%d_y%d: block_type_index(%d) width_offset(%d) height_offset(%d)",
				&x, &y, &block_type_index, &width_offset, &height_offset);

		check_expected_vs_read(num_expected, num_read, line);

		/* grid variables */
		grid.at(x).at(y).set_type_index(block_type_index);
		//grid[x][y].set_type_index(block_type_index);
		grid[x][y].set_width_offset(width_offset);
		grid[x][y].set_height_offset(height_offset);

		getline(file, line);
		read_grid_elements++;
	}
	
	if (read_grid_elements != expected_grid_elements){
		WTHROW(EX_INIT, "Expected to find " << expected_grid_elements << " grid elements, but found " << read_grid_elements);
	}
}


/* parses rr node indices section of file into the created rr_node_index structure */
static void parse_rr_node_index_section(int num_rr_types, int x_size, int y_size, t_rr_node_index &rr_node_index, fstream &file){

	string line;
	getline(file, line);	//first line of rr node index section
	while(line != ".end rr_node_indices"){
		
		int num_expected, num_read;
		int rr_type, x, y;

		/* get the rr_type/x/y coordinate */
		num_expected = 3;
		num_read = sscanf(line.c_str(), " rr_node_index_type%d_x%d_y%d", 
				&rr_type, &x, &y);

		check_expected_vs_read(num_expected, num_read, line);

		/* what is the number of nodes this rr_type/x/y location? */
		getline(file, line);
		int num_nodes = 0;
		num_expected = 1;
		num_read = sscanf(line.c_str(), "  .nodes(%d)", &num_nodes);

		check_expected_vs_read(num_expected, num_read, line);

		/* create the node vector */
		rr_node_index[rr_type][x][y].assign(num_nodes, UNDEFINED);
		
		/* read in the list of nodes at this rr_type/x/y location */
		getline(file, line);
		while(line != "  .end nodes"){
			int node_num, node;

			num_expected = 2;
			num_read = sscanf(line.c_str(), "   %d: %d", &node_num, &node);

			check_expected_vs_read(num_expected, num_read, line);

			rr_node_index[rr_type][x][y][node_num] = node;

			getline(file, line);
		}

		getline(file, line);
	}
}


/* checks whether an sscanf function read as many arguments as were expected and throws an exception if not. 'line' is the line that was scanned */
static void check_expected_vs_read(int num_expected, int num_read, string line){
	if (num_expected != num_read){
		WTHROW(EX_INIT, "Expected to scan " << num_expected << " values but found " << num_read << endl << "Line: " << line);
	}
}


/* If Wotan is being initialized based on an rr structs file then backwards edges/switches need to be determined 
   for each node as a post-processing step. Do this for the pins specified by 'node_type'. if node_type == UNDEFINED,
   then do this for all nodes  */
void initialize_reverse_node_edges_and_switches( Routing_Structs *routing_structs, int node_type ){
	/* setting the incoming edges/switches for each node can be done in two passes over the graph. once to determine the list of switches/edges
	   for each node, and once to set this information for each node */

	int num_nodes = routing_structs->get_num_rr_nodes();

	/* vector of vectors to hold incoming edges and switches for each node */
	vector< vector<int> > inc_switches, inc_edges;
	inc_switches.assign(num_nodes, vector<int>());
	inc_edges.assign(num_nodes, vector<int>());

	/* pass 1 - determine what the incoming edges/switches are for each node */
	for (int inode = 0; inode < num_nodes; inode++){
		int from_node_ind = inode;
		RR_Node &rr_node = routing_structs->rr_node[from_node_ind];
		
		/* for each destination node mark which node the connection is coming from and which switch it uses */
		for (int i_out_edge = 0; i_out_edge < rr_node.get_num_out_edges(); i_out_edge++){
			int to_node_ind = rr_node.out_edges[i_out_edge];
			int switch_ind = rr_node.out_switches[i_out_edge];

			inc_switches[to_node_ind].push_back(switch_ind);
			inc_edges[to_node_ind].push_back(from_node_ind);
		}
	}

	/* pass 2 - allocate and set incoming switch/edge structures for each node */
	for (int inode = 0; inode < num_nodes; inode++){
		RR_Node *rr_node = &routing_structs->rr_node[inode];

		if (node_type != UNDEFINED){
			/* skip nodes that aren't of 'node_type' */
			if (rr_node->get_rr_type() != (e_rr_type)node_type){
				continue;
			}
		}

		int num_inc_edges = (int)inc_edges[inode].size();

		rr_node->free_in_edges_and_switches();

		if (num_inc_edges > 0 && rr_node->get_num_in_edges() == UNDEFINED){
			/* allocate the incoming switches/edges for this node */
			rr_node->alloc_in_edges_and_switches( num_inc_edges );

			/* set incoming switches/edges */
			for (int iedge = 0; iedge < num_inc_edges; iedge++){
				rr_node->in_edges[iedge] = inc_edges[inode][iedge];
				rr_node->in_switches[iedge] = inc_switches[inode][iedge];
			}
		}
	}
}

