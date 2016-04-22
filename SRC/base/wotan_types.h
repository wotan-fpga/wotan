#ifndef WOTAN_TYPES_H
#define WOTAN_TYPES_H

#include <string>
#include <vector>
#include <cmath>
#include "wotan_util.h"
#include <pthread.h>

#define UNDEFINED -1

/* used for comparing floating point probabilities. when I compare probabilities I worry about coarse differences. i.e. 0.3 vs 0.31. 
   since floats/doubles are not exact, I compare for equality by checking that the difference between the two nums is small enough.
   the below value defines what "small enough" means to me */
#define FLOAT_PROB_TOL 0.000001

/* a define for comparing whether two probabilities are equal */
#define PROBS_EQUAL(f1, f2) (std::fabs(f1 - f2) <= FLOAT_PROB_TOL ? true : false)



/**** Enums ****/
/* used to specify the direction of a routing node. INC/DEC represents
   in which way signals on the node are headed relative to the coordinate system */
enum e_direction{
	INC_DIRECTION = 0,	/* going in the positive direction relative to coordinate system */
	DEC_DIRECTION,		/* going in the negative direction relative to coordinate system */
	BI_DIRECTION		/* signals can go in either direction on this kind of node */
};

/* graph nodes can represent different kinds of routing resources.
   Names for printing are set in the g_rr_type_string variable */
enum e_rr_type{
	SOURCE = 0,	/* a signal source */
	//VIRTUAL_SOURCE,	/* a virtual signal source -- to be placed on ipins to account for fanout */
	SINK,		/* a signal sink */
	IPIN,		/* input pin on a block */
	OPIN,		/* an output pin on a block */
	CHANX,		/* a wire segment in an x-directed channel */
	CHANY,		/* a wire segment in a y-directed channel */
	NUM_RR_TYPES
};
extern const std::string g_rr_type_string[NUM_RR_TYPES];

/* a pin on a physical block can be unconnected (OPEN), be a driver, or a receiver */
enum e_pin_type{
	OPEN = -1,
	DRIVER,
	RECEIVER
};

/* specifies direction of graph traversal */
enum e_traversal_dir{
	FORWARD_TRAVERSAL = 0,
	BACKWARD_TRAVERSAL
};

/* specifies what form the contents of the rr structs file (parsed in during initialization) are expected to take 
	RR_STRUCTS_VPR -- dumped routing resource structs from VPR. Should include lists of:
			- rr nodes
			- rr switches 
			- physical block types
			- grid entries
			- rr node indices

	RR_STRUCTS_SIMPLE -- indicates a simple one-source/one-sink graph. Should include lists of:
			- rr nodes
*/
enum e_rr_structs_mode{
	RR_STRUCTS_UNDEFINED = 0,
	RR_STRUCTS_VPR,
	RR_STRUCTS_SIMPLE,
	NUM_RR_STRUCTS_MODES
};
extern const std::string g_rr_structs_mode_string[NUM_RR_STRUCTS_MODES];

/* how do node buckets store path counts? does each
   bucket index correspond to a certain path weight?
   or does each bucket index correspond to #hops from source/sink? */
enum e_bucket_mode{
	BY_PATH_WEIGHT = 0,
	BY_PATH_HOPS
};



/**** Forward Declarations ****/
class RR_Node;
class Physical_Type_Descriptor;
class RR_Switch_Inf;
class Grid_Tile;
class Arch_Structs;
class Routing_Structs;
class SS_Distances;
class Node_Buckets;
class Node_Topological_Info;


/**** Typedefs ****/
/* a list of pin / length probabilities */
typedef std::vector< double > t_prob_list;

/* a vector of rr nodes */
//typedef RR_Node* t_rr_node;
typedef std::vector< RR_Node > t_rr_node;

/* a vector of physical block type descriptors */
typedef std::vector< Physical_Type_Descriptor > t_block_type;

/* a vector of rr switch types. each entry contains information about a different switch type */
typedef std::vector< RR_Switch_Inf > t_rr_switch_inf;

/* helps to quickly find which block types are at which coordinates of the FPGA grid */
typedef std::vector< std::vector< Grid_Tile > > t_grid;	//[x_coord][y_coord]

/* [0..NUM_RR_TYPES-1][0..xsize-1][0..ysize-1][0..number of nodes at this location (indexable by ptc)]
   Allows for an easy way of finding which rr node index is at a specific rr_type/x/y/ptc coordinate */
typedef std::vector< std::vector< std::vector< std::vector<int> > > > t_rr_node_index;

/* a chanwidth value for each x/y coordinate */
typedef std::vector< std::vector< int > > t_chanwidth;

/* for keeping track of the distance from a given node to a source/sink for which path enumeration is being performed */
typedef std::vector< SS_Distances > t_ss_distances;

/* topological traversal info structures for each node */
typedef std::vector< Node_Topological_Info > t_node_topo_inf;


/**** Classes ****/
/* Contains user-defined options */
class User_Options{
public:
	bool nodisp;				/* specifies whether to do graphics or not */
	e_rr_structs_mode rr_structs_mode;	/* Wotan's routing structures are read-in according to this mode */
	std::string rr_structs_file;		/* path to file from which rr structures are to be read */
	int max_connection_length;		/* maximum connection length to be considered during path enumeration */
	bool keep_path_count_history;		/* routing nodes will keep track of path counts from adjacent sources/sinks. adds accuracy to probability analysis */
	bool analyze_core;			/* reachability analysis will only be performed for a core region of the FPGA */ //TODO: defined as what?

	float use_routing_node_demand;		/* if not UNDEFINED, then demand for routing nodes (CHANX/CHANY) will be considered to be whatever is specified here.
						   demand for all non-routing nodes will be considered to be 0 */

	int num_threads;			/* number of threads to use for path enumeration & probability analysis */

	double ipin_probability;
	double opin_probability;
	double demand_multiplier;
	t_prob_list length_probabilities;

	User_Options();
};


/* A class used to pass around some settings specific to path enumeration & probability analysis.
   The contents of this class are either derived from the contents of the User_Options class, or hard-coded 
   in the appropriate alloc/get functions */
class Analysis_Settings{
private:

public:
	Analysis_Settings();

	/* A list of tile coordinates representing FPGA tiles from which path enumeration is to be performed */
	std::vector< Coordinate > test_tile_coords;

	/* Contains the probabilities of using each given pin belonging to the 'fill' block type (the block type can be
	   determined from Arch_Structs::get_fill_type_index()). If a pin probability is 0, no paths will be enumerated from it */
	t_prob_list pin_probabilities;

	/* 	Contains the probabilities of encountering connections of a given length. If a particular source/dest connection has a length
	   with an occurance probability of 0 then paths for that connection will not be enumerated. 
	   	Note that these probabilities must add up to 1, and each source pin from which paths are enumerated must have connections 
	   of each constituent length within the test area. This list of probabilities is therefore derived based on the list of 
	   length probabilities contained in User_Options and is affected by the size of the test area -- since not all lenghts specified
	   in the probability list of User_Options may be possible within the test area, probabilities of allowed lengths must be scaled
	   before being stored in this particular list (such that they add up to 1) */
	t_prob_list length_probabilities;


	/* set methods */
	void alloc_and_set_pin_probabilities(double driver_prob,		/* set probabilities of driver/receiver pins (belonging to fill block type) */
	                                     double receiver_prob,
	                                     Arch_Structs *arch_structs);
	void alloc_and_set_length_probabilities(User_Options*);			/* set length probabilities (based on length probabilities from User_Options) */
	void alloc_and_set_test_tile_coords(Arch_Structs*, Routing_Structs*);	/* allocates the test_tile_coords list and sets it based on routing architecture */

	/* get methods */
	int get_max_path_weight(int conn_length);				/* returns maximum allowable path weight according to passed in connection length */
};


/* A routing resource node on the graph. This structure is based from the corresponding VPR structure, but with slight modifications */
class RR_Node_Base{
private:
	e_rr_type type;					/* the routing resource type of this node (pin, wire, etc) */

	/* allows getting start/end coordinates of nodes */
	short xlow;					/* x coordinate of the low end of this routing resource */
	short ylow;					/* y coordinate of the low end of this routing resource */
	short span;					/* how many CLBs this node spans */

	float R;					/* resistance (ohms) to go through this node (doesn't include switch resistances) */
	float C;					/* total capacitance (farads) of this node (including switches that hang off from it) */
	
	short ptc_num;					/* pin-track-class number. allows lookups of which pin/track/etc an rr node represents */
	short fan_in;					/* the fan-in to this node */
	short num_out_edges;				/* number of edges emanating from this node */

	enum e_direction direction;			/* direction along which signals would travel on this node (if applicable) */

public:

	RR_Node_Base();

	int *out_edges;					/* a list of rr nodes *to* which this node connects [0..get_num_out_edges()-1] */
	short *out_switches;				/* a list of switches which are used by the edges emanating from this node */
	
	/* allocator functions */
	void alloc_out_edges_and_switches(short);

	/* freeing function */
	void free_allocated_members();

	/* get methods */
	e_rr_type get_rr_type() const;			/* get the rr type of this node */
	std::string get_rr_type_string() const; 	/* retrieve rr_type as a string */
	short get_xlow() const;				/* get low x coordinate of this node */
	short get_ylow() const;				/* get low y coordinate of this node */
	short get_xhigh() const;			/* get high x coordinate of this node */
	short get_yhigh() const;			/* get high y coordinate of this node */
	short get_span() const;				/* how many logic blocks does this node span? */
	float get_R() const;				/* get node resistance */
	float get_C() const;				/* get node capacitance */
	short get_ptc_num() const;			/* get the pin-track-class number of this node */
	short get_fan_in() const;			/* get the fan-in of this node */
	short get_num_out_edges() const;		/* get the number of edges emanating from this node */
	e_direction get_direction() const;		/* get the directionality of this node in relation to the coordinate system (increasing/decreasing/bidir) */

	/* set methods */
	void set_rr_type(e_rr_type);			/* set rr type of this node */
	void set_coordinates(short x1, short y1, 	/* set node coordinates. x1/x2 don't have to be ordered (same for y1/y2) */
	                     short x2, short y2);	
	void set_R(float);				/* set resistance of this node */
	void set_C(float);				/* set capacitance of this node */
	void set_ptc_num(short);			/* set pin-track-class number of this node */
	void set_fan_in(short);				/* set fan-in of this node*/
	void set_direction(e_direction);		/* set directionality of this node */
};


/* Derived from the RR_Node_Base class (based on the VPR rr node structure), this class adds functionality specifically required by Wotan */
class RR_Node : public RR_Node_Base {
private:
	short num_in_edges;				/* number of edges linking into this node */
	float weight;					/* weight of this node */
	double demand;					/* fractional demand for this node. used for routability analysis */

	int num_lb_sources_and_sinks;			/* total number of sources and sinks on a logic block */
	

	/* each node keeps track of the number of paths from/to all nearby sources/sinks that are within the 
	   (manhattan distance w.r.t. logic blocks) radius 'path_count_history_radius'. The center of the 
	   manhattan circle is at the xlow/ylow coordinates of this node

	In general, a manhattan circle of radius r has 4*r CLBs in the circumference. Adding up successive
	circumferences gives the #elements in a manhattan circle of radius r as 1 + 4*[r(r+1)/2].

	It is convenient to allocate path history elements in terms of polar coordinates as opposed to cartesian.
	The indexing variables for the path count histories structure are then:
	   - radius: the manhattan distance between this node and the target node
	   - arc: distance along circumference to target node (count starts at (0,r) cartesian coordinate)
	   - source/sink class index: identifies the source/sink */
	float ***source_sink_path_history;	//[0..radius][0..circumference-1][0..num_source/sinks -1]
	int path_count_history_radius;

	/* a hack that allows paths to be enumerated out of non-source nodes -- a virtual source node can be created to connect to some
	   subset of predecessors of this node which can be useful for things like accounting for fanout (by enumerating paths backward
	   through ipins essentially).
	   this variable marks the index of the corresponding virtual source. */
	int virtual_source_node_ind;

	pthread_mutex_t my_mutex;

protected:
	/* Increments + returns path count history, or simply returns path count history
	   of this node based on the 'increment' bool variable */
	float access_path_count_history(float increment_val, RR_Node &target_node, bool increment);

public:
	RR_Node();

	bool highlight;

	/* these structures are used to do backwards traversals of the graph. */
	int *in_edges;					/* a list of rr nodes *from* which this node receives connections [0..get_num_in_edges()-1] */
	short *in_switches;				/* a list of switches which are used by the edges linking into this node */


	/* allocator functions */
	void alloc_in_edges_and_switches(short);

	void alloc_source_sink_path_history(int num_lb_sources_and_sinks);

	/* freeing functions */
	void free_in_edges_and_switches();
	void free_allocated_members();

	/* set methods */
	void clear_demand();
	void increment_demand(double);
	void set_virtual_source_node_ind(int);
	void set_weight();

	/* get methods */
	short get_num_in_edges() const;
	double get_demand(User_Options*) const;
	float get_weight() const;
	int get_virtual_source_node_ind() const;

	/* increments path count history at this node due to the specified target node.
	   the specified target node is either the source or sink of a connection that
	   carries paths through *this* node */
	void increment_path_count_history(float increment_val, RR_Node &target_node);

	/* returns path count history at this node due to the specified target node.
	   the specified target node is either the source or sink of a connection that
	   carries paths through *this* node.
	   returns UNDEFINED if this node doesn't carry relevant path count info */
	float get_path_count_history(RR_Node &target_node);
};


/* represents a switch used in the rr graph. Basically a copy of VPR's analogous structure */
class RR_Switch_Inf{
private:
	bool buffered;					/* is this switch buffered? */
	float R;					/* resistance to go through the switch/buffer */
	float Cin;					/* switch input capacitance */
	float Cout;					/* switch output capacitance */
	float Tdel;					/* switch intrinsic delay. total delay through switch is Tdel + R*Cout */
	float mux_trans_size;				/* area of switch's mux transistors in minimum width transistor units */
	float buf_size;					/* area of buffer in minimum width transistor units */
	
public:

	RR_Switch_Inf();

	/* get and set methods */
	bool get_buffered() const;
	float get_R() const;
	float get_Cin() const;
	float get_Cout() const;
	float get_Tdel() const;
	float get_mux_trans_size() const;
	float get_buf_size() const;

	void set_buffered(bool);
	void set_R(float);
	void set_Cin(float);
	void set_Cout(float);
	void set_Tdel(float);
	void set_mux_trans_size(float);
	void set_buf_size(float);
};


/* Represents a set of equivalent pins within some physical block type */
class Pin_Class{
private:
	e_pin_type type;				/* pin can be driver/receiver/open */
public:
	
	std::vector<int> pinlist;			/* a list of pins belonging to this class */

	Pin_Class();

	/* get methods */
	e_pin_type get_pin_type() const;
	int get_num_pins() const;

	/* set methods */
	void set_pin_type(e_pin_type);
};


/* represents a physical block type (i.e. CLB, multiplier, mem, etc). This structure is basically a copy of VPR's s_type_descriptor */
class Physical_Type_Descriptor{
private:
	std::string name;				/* name of this block type */
	int index;					/* the index of this block type in the t_block_type array */
	int num_pins;					/* number of pins in this block type */
	int width;					/* width (in CLB spans) of this block type */
	int height;					/* height (in CLB spans) of this block type */
	
	int num_drivers;				/* number of pins in this block that are drivers */
	int num_receivers;				/* number of pins in this block that are receivers */
public:

	Physical_Type_Descriptor();

	std::vector<Pin_Class> class_inf;		/* contains info on each pin class of this block */
	std::vector<int> pin_class;			/* Specifies which class each pin belongs to. Used to index into class_inf */
	std::vector<bool> is_global_pin;		/* True if the corresponding pin is a global pin */
	
	/* get methods */
	std::string get_name() const;
	int get_index() const;
	int get_num_pins() const;
	int get_width() const;
	int get_height() const;
	int get_num_drivers() const;
	int get_num_receivers() const;

	/* set methods */
	void set_name(std::string);
	void set_index(int);
	void set_num_pins(int);
	void set_width(int);
	void set_height(int);
	void set_num_drivers(int);
	void set_num_receivers(int);
};


/* as part of the t_grid structure, this class helps index into the proper block type according to the coordinate on the FPGA grid */
class Grid_Tile{
private:
	int type_index;			/* the index (to t_block_type) of the block type that is at this grid location */
	int type_width_offset;		/* the width offset from the block's origin (0,0) tile at this grid coordinate */
	int type_height_offset;		/* the height offset from the block's origin (0,0) tile at this grid coordinate */
public:

	Grid_Tile();

	/* get methods */
	int get_type_index() const;
	int get_width_offset() const;
	int get_height_offset() const;

	/* set methods */
	void set_type_index(int);
	void set_width_offset(int);
	void set_height_offset(int);
};


/* contains architecture structures */
class Arch_Structs{
private:
	int fill_type_index;			/* the index, in t_block_type array, of the most common type of block (assumed to be the logic block) */
public:
	Arch_Structs();

	t_block_type block_type;		/* a 1-D array of physical block types (i.e. CLB, empty, etc) */
	t_grid grid;				/* a 2-D array where each entry gives info about the block type at that physical location */
	t_chanwidth chanwidth_x;		/* a 2-D array where each entry gives the x-directed channel width at that coordinate */
	t_chanwidth chanwidth_y;		/* a 2-D array where each entry gives the y-directed channel width at that coordiante */

	/* allocator functions. if we want to move from vectors to C-style arrays, can change this, and deallocate in destructor or freeing function */
	void alloc_and_create_block_type(int);
	void alloc_and_create_grid(int x_size, int y_size);

	/* set methods */
	void set_fill_type();					/* sets 'fill_type_index' according to the most common block in 'grid' */
	void set_chanwidth(Routing_Structs *routing_structs);	/* sets x-directed and y-directed channel widths */ 

	/* get methods */
	int get_fill_type_index() const;			/* returns 'fill_type_index' */
	void get_grid_size(int *x_size, int *y_size) const;	/* returns x and y sizes of the grid */
	int get_num_block_types() const;			/* returns number of physical block types */
};


/* contains routing structures */
class Routing_Structs{
private:
	int num_rr_nodes;
public:

	t_rr_node rr_node;				/* a 1-D array of rr nodes */
	t_rr_switch_inf rr_switch_inf;			/* a 1-D array of rr switch types */
	t_rr_node_index rr_node_index;			/* a matrix for lookups of rr nodes at some physical location */

	/* allocator functions. if we want to move from vectors to C-style arrays, can change this, and deallocate in destructor */
	void alloc_and_create_rr_node(int);
	void alloc_rr_node_path_histories(int num_lb_sources_and_sinks);

	void alloc_and_create_rr_switch_inf(int);

	void alloc_and_create_rr_node_index(int num_rr_types, int x_size, int y_size);

	void init_rr_node_weights();

	/* get methods */
	int get_num_rr_nodes() const;
};


/* Objects of this class are used to store the distance of a graph node to some specific source
   and some specific pair. */
class SS_Distances{
private:
	int source_distance;			/* distance to source */
	int sink_distance;			/* distance to sink */
	bool visited_from_source;		/* the corresponding node has had its source distance set by a graph traversal from source */
	bool visited_from_sink;			/* the corresponding node has had its sink distance set by a graph traversal from sink */

	int source_hops;			/* shortest # of node hops from source */
	int sink_hops;				/* shortest # of node hops to sink */
	bool visited_from_source_hops;		/* true when corresponding node has already been visited while calculating source_hops */
	bool visited_from_sink_hops;		/* true when corresponding node has already been visited while calculating sink_hops */


public:
	
	SS_Distances();

	/* resets all variables */
	void clear();

	/* set methods */
	void set_source_distance(int);
	void set_sink_distance(int);
	void set_visited_from_source(bool);
	void set_visited_from_sink(bool);
	void set_source_hops(int);
	void set_sink_hops(int);
	void set_visited_from_source_hops(bool);
	void set_visited_from_sink_hops(bool);

	/* get methods */
	int get_source_distance() const;
	int get_sink_distance() const;
	bool get_visited_from_source() const;
	bool get_visited_from_sink() const;
	int get_source_hops() const;
	int get_sink_hops() const;
	bool get_visited_from_source_hops() const;
	bool get_visited_from_sink_hops() const;

	/* returns true if the specified node has paths running through it from source to sink that are below
	   the maximum allowable weight (i.e. node is legal) */
	bool is_legal(int my_node_weight, int max_path_weight) const;
};


/* Represents a node that has been visited during topological graph traversal, but who's dependencies
   aren't fully satisfied (i.e. it still has parents which have not been visited).
   This structure is used to deal with graph cycles -- objects of this class are put on a sorted
   list, and if the traditional expansion queue becomes empty during topological traversal,
   said sorted structure is used to get the next node on which to expand */
class Node_Waiting{
private:
	int path_weight;	/* the 'path_weight' attributed to the waiting node -- primary sort for cycle breaking */
	int source_dist;	/* the distance from corresponding node to the source node -- secondary sort for cycle breaking */
	int node_ind;		/* the index of the corresponding node -- tertiary sort for cycle breaking */
public:
	Node_Waiting();

	/* set methods */
	void set( int set_ind, int set_path_weight, int set_source_dist );
	void clear();

	/* get methods */
	int get_node_ind() const;
	int get_path_weight() const;
	int get_source_distance() const;

	/* overload < for purposes of storing class objects in maps/sets */
	bool operator < (const Node_Waiting &obj) const;
};


/* nodes have associated with them two bucket structures. one bucket structure is associated with a source and
   one with a sink (during path enumeration between a specific source/sink). the index of the bucket structure
   corresponds to a particular path weight. the buckets can then be used to keep track of how many paths there
   are of each weight going to a source/sink; or during the probability analysis step, what is the probability
   of a path of some weight not existing to the source/sink.

   This structure is used in the topological traversal of the graph to count paths */
class Node_Buckets{
private:
	int num_source_buckets;
	int num_sink_buckets;
	e_bucket_mode bucket_mode;

public:

	Node_Buckets();
	Node_Buckets(int max_path_weight_bound);	/* allocates source/sink buckets based on the maximum path weight bound specified */

	float *source_buckets;
	float *sink_buckets;

	/* allocator methods */
	void alloc_source_sink_buckets(int set_num_source_buckets, int set_num_sink_buckets);

	/* free methods */
	void free_source_sink_buckets();

	/* set methods */
	void clear();
	void clear_up_to(int);
	void set_bucket_mode(e_bucket_mode);
	
	/* get methods */
	int get_num_source_buckets() const;
	int get_num_sink_buckets() const;
	e_bucket_mode get_bucket_mode() const;

	/* returns number of legal paths which go through the node associated with this structure */
	float get_num_paths(int my_node_weight, int my_dist_to_source, int max_path_weight) const;

	/* returns the probability of this node being unreachable from source (the node structures must contain probabilities instead of path counts) */
	float get_probability_not_reachable(int my_node_weight, float my_node_probability) const;
};


/* a structure that contains topological traversal info for the associated node */
class Node_Topological_Info{
private:
	bool done_from_source;			/* this node has already been placed onto expansion queue for a traversal from source */	
	bool done_from_sink;			/* this node has already been placed onto expansion queue for a traversal from sink */		

	short times_visited_from_source;	/* the # times the node has been visited in a topological traversal from source */		
	short times_visited_from_sink;		/* the # times the node has been visited in a topological traversal from sink */		

	short num_legal_in_nodes;		/* number of legal nodes which connect to this node */						
	short num_legal_out_nodes;		/* number of legal nodes to which this node connects */						

	int node_level;				/* The topological traversal level of this node	*/

	bool node_smoothed;			/* special variable used during recursive topological cutline traversal */
	float adjusted_demand;			/* special variable used during recursive topological cutline traversal */
	float was_visited;			/* special variable for recursive topological cutline traversal -- true if node was visited
						   in any level of recursion */
protected:

	/* returns number of legal nodes on specified edge list */
	short get_num_legal_nodes(int *edge_list, int num_edges, t_rr_node &rr_node, t_ss_distances &ss_distances, int max_path_weight);
public:
	
	Node_Topological_Info();

	/* used to limit which paths are considered during topological path enumeration, based on path weight */
	Node_Buckets buckets;

	/* keeps info that is essential for accessing the corresponding node on a set structure used for breaking cycles */
	Node_Waiting node_waiting_info;

	/* resets variables. does not deallocate node buckets structure (only clears contents) */
	void clear();

	/* set methods */
	void set_done_from_source(bool);
	void set_done_from_sink(bool);
	void increment_times_visited_from_source();	//increment by 1
	void increment_times_visited_from_sink();
	void set_times_visited_from_source(short);
	void set_times_visited_from_sink(short);
	void set_level(int);
	void set_num_legal_in_nodes(short);
	void set_num_legal_out_nodes(short);
	void set_node_smoothed(bool);
	void set_adjusted_demand(float);
	void set_was_visited(bool);


	/* get methods */
	short get_times_visited_from_source() const;
	short get_times_visited_from_sink() const;
	bool get_done_from_source() const;
	bool get_done_from_sink() const;
	int get_level() const;
	short get_num_legal_in_nodes() const;
	short get_num_legal_out_nodes() const;
	bool get_node_smoothed() const;
	float get_adjusted_demand() const;
	bool get_was_visited() const;
	

	/* returns number of legal nodes that have edges into this node. if this value is
	   not yet set, then it gets calculated and set as well */
	short set_and_or_get_num_legal_in_nodes(int my_node_index, t_rr_node &rr_node, t_ss_distances &ss_distances, int max_path_weight);
	/* returns number of legal nodes to which this node has edges. if this value is
	not yet set, then it gets calculated and set as well */
	short set_and_or_get_num_legal_out_nodes(int my_node_index, t_rr_node &rr_node, t_ss_distances &ss_distances, int max_path_weight);
};

#endif
