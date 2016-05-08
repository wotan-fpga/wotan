
#include <cmath>
#include "io.h"
#include "exception.h"
#include "wotan_types.h"

using namespace std;

/* this has to exactly match e_rr_type */
const string g_rr_type_string[NUM_RR_TYPES]{
	"SOURCE",
	//"VIRTUAL_SOURCE",
	"SINK",
	"IPIN",
	"OPIN",
	"CHANX",
	"CHANY"
};

/* this has to exactly match e_rr_structs_mode */
const string g_rr_structs_mode_string[NUM_RR_STRUCTS_MODES]{
	"RR_STRUCTS_UNDEFINED",
	"RR_STRUCTS_VPR",
	"RR_STRUCTS_SIMPLE"
};

/*==== User Options Class ====*/
User_Options::User_Options(){
	this->nodisp = false;
	this->rr_structs_mode = RR_STRUCTS_UNDEFINED;
	this->num_threads = 1;
	this->max_connection_length = 3;
	this->keep_path_count_history = true;
	this->analyze_core = true;

	this->use_routing_node_demand = UNDEFINED;

	/* pin pbobabilities can be initialized from a file in the future, but for now set them
	   to some default values */
	this->ipin_probability = 0.0;	//was 0.3
	this->opin_probability = 0.6;
	this->demand_multiplier = 1.0;

	/* length probabilities can be initialized from a file in the future, but for now set them
	   to some default value */
	this->length_probabilities.assign(20, 0);
	this->length_probabilities[0] = 0;	//TODO, shouldn't really be 0 because we can have feedback paths internal to a logic block (from sources to sinks)
	this->length_probabilities[1] = 0.40;
	this->length_probabilities[2] = 0.23;
	this->length_probabilities[3] = 0.14;
	this->length_probabilities[4] = 0.08;
	this->length_probabilities[5] = 0.065;
	this->length_probabilities[6] = 0.035;
	this->length_probabilities[7] = 0.025;
	this->length_probabilities[8] = 0.015;
	this->length_probabilities[9] = 0.01;
	this->length_probabilities[10] = 0.008;
	this->length_probabilities[11] = 0.008;
	this->length_probabilities[12] = 0.006;
	this->length_probabilities[13] = 0.005;
	this->length_probabilities[14] = 0.004;
	this->length_probabilities[15] = 0.003;
	this->length_probabilities[16] = 0.003;
}
/*==== END User Options Class ====*/

/*==== Analysis_Settings Class ====*/
Analysis_Settings::Analysis_Settings(){

}

/* sets probabilities of driver/receiver pins of the physical descriptor type that represents the logic block */
void Analysis_Settings::alloc_and_set_pin_probabilities(double driver_prob, double receiver_prob, Arch_Structs *arch_structs){
	/* get fill block type (i.e. the logic block type) */
	int fill_type_index = arch_structs->get_fill_type_index();
	Physical_Type_Descriptor *fill_block_type = &arch_structs->block_type[fill_type_index];

	/* get number of pins at this block type */
	int num_block_pins = fill_block_type->get_num_pins();

	/* allocate pin probabilities structure */
	this->pin_probabilities.assign(num_block_pins, 0.0);

	/* now set pin probabilities based on whether they are a driver or a receiver. global pins and
	   all other pins receiver a probability of 0 */
	for (int ipin = 0; ipin < num_block_pins; ipin++){
		int pin_class = fill_block_type->pin_class[ipin];
		e_pin_type pin_type = fill_block_type->class_inf[pin_class].get_pin_type();

		if (fill_block_type->is_global_pin[ipin]){
			this->pin_probabilities[ipin] = 0;
		} else {
			if (pin_type == DRIVER){
				this->pin_probabilities[ipin] = driver_prob;
			} else if (pin_type == RECEIVER){
				this->pin_probabilities[ipin] = receiver_prob;
			} else {
				this->pin_probabilities[ipin] = 0;
			}
		} 
	}
}

/* sets length probabilities based on the length probabilities from User_Options */
void Analysis_Settings::alloc_and_set_length_probabilities(User_Options *user_opts){
	/* The number of entries in the length probabilities list depends on the maximum allowed connection length. 
	   The values of these entries will be a scaled copy of the values in the User_Options probabilities list. */
	int max_entries = user_opts->max_connection_length + 1; //[0...max_connection_length]
	this->length_probabilities.assign(max_entries, 0);

	int user_opts_entries = (int)user_opts->length_probabilities.size();
	
	/* check that there are enough probability entries in user_opts to satisfy the maximum allowable connection length */
	if (max_entries > user_opts_entries){
		WTHROW(EX_INIT, "Insufficient number of connection length probabilities provided by user." <<
		                "There are " << user_opts_entries << " entries in the user length probability list " <<
				"but a 'max_connection_length' of " << user_opts->max_connection_length << " calls for " << 
				max_entries << " entries."); 
	}

	//TODO: check that the FPGA grid can allow for such lengths

	/* The probabilities entered into the Analysis_Settings list must add up to 1, so a bit of scaling needs to happen
	   when values are copied from the user_opts probabilities list */
	double prob_sum = 0;
	for (int i = 0; i < max_entries; i++){
		prob_sum += user_opts->length_probabilities[i];
	}
	for (int i = 0; i < max_entries; i++){
		this->length_probabilities[i] = user_opts->length_probabilities[i] / prob_sum;
	}
}

/* allocates the test tile coords list and sets it based on the routing architecture */
void Analysis_Settings::alloc_and_set_test_tile_coords(Arch_Structs *arch_structs, Routing_Structs *routing_structs){
	this->test_tile_coords.clear();

	/* TODO: Ideally what we want is to find the set of test tiles from which we can perform enumeration
	   which can then be replicated to all other tiles in the FPGA. For now, however, just pick a 
	   single tile in the middle of the FPGA to test Wotan flow */
	int grid_size_x, grid_size_y;
	arch_structs->get_grid_size(&grid_size_x, &grid_size_y);
	//Coordinate coord(5,5);
	//this->test_tile_coords.push_back(coord);

	for (int ix = 1; ix < grid_size_x-1; ix++){
		for (int iy = 1; iy < grid_size_y-1; iy++){
			Coordinate coord(ix, iy);
			this->test_tile_coords.push_back(coord);
		}
	}
}

/* returns maximum allowable path weight according to passed in connection length */
int Analysis_Settings::get_max_path_weight(int conn_length){
	/* this is a provisional scheme; will probably change later. but for now will set max
	   path weight to give some flexibility in enumerating paths of the connection */
	int max_path_weight = 15 + conn_length*1.3;
	return max_path_weight;
}
/*==== END Analysis_Settings Class ====*/


/*==== RR_Node_Base Class ====*/
/* Constructor initializes everything to UNDEFINED */
RR_Node_Base::RR_Node_Base(){
	this->type = (e_rr_type)UNDEFINED;
	this->xlow = UNDEFINED;
	this->ylow = UNDEFINED;
	this->span = UNDEFINED;
	this->R = UNDEFINED;
	this->C = UNDEFINED;
	this->ptc_num = UNDEFINED;
	this->fan_in = UNDEFINED;
	this->num_out_edges = UNDEFINED;
	this->direction = (e_direction)UNDEFINED;
	this->out_edges = NULL;
	this->out_switches = NULL;
}

/* frees allocated members */
void RR_Node_Base::free_allocated_members(){
	delete [] this->out_edges;
	delete [] this->out_switches;
	this->out_edges = NULL;
	this->out_switches = NULL;
}

/* Allocates edge array and switch array, and sets num_out_edges */
void RR_Node_Base::alloc_out_edges_and_switches(short n_edges){
	this->out_edges = new int[n_edges];
	this->out_switches = new short[n_edges];
	this->num_out_edges = n_edges;
}

/* get the rr type of this node */
e_rr_type RR_Node_Base::get_rr_type() const{
	return this->type;
}

/* Retrieve node's rr_type as a string */
string RR_Node_Base::get_rr_type_string() const{
	string type_str = g_rr_type_string[this->type];
	return type_str;
}

/* get low x coordinate of this node */
short RR_Node_Base::get_xlow() const{
	return this->xlow;
}

/* get low y coordinate of this node */
short RR_Node_Base::get_ylow() const{
	return this->ylow;
}

/* get the high x coordinate of this node */
short RR_Node_Base::get_xhigh() const{
	int xhigh;
	if (this->type == CHANX){
		xhigh = this->xlow + this->span-1;
	} else {
		xhigh = this->xlow;
	}
	return xhigh;
}

/* get the high y coordinate of this node */
short RR_Node_Base::get_yhigh() const{
	int yhigh;
	if (this->type == CHANY){
		yhigh = this->ylow + this->span-1;
	} else {
		yhigh = this->ylow;
	}
	return yhigh;
}


/* how many logic blocks does this node span? */
short RR_Node_Base::get_span() const{
	return this->span;
}

/* get node resistance */
float RR_Node_Base::get_R() const{
	return this->R;
}

/* get node capacitance */
float RR_Node_Base::get_C() const{
	return this->C;
}

/* gets pin-track-class number of this node */
short RR_Node_Base::get_ptc_num() const{
	return this->ptc_num;
}

/* gets the fan-in of this node */
short RR_Node_Base::get_fan_in() const{
	return this->fan_in;
}

/* gets the number of edges emanating from this node */
short RR_Node_Base::get_num_out_edges() const{
	return this->num_out_edges;
}

/* get directionality of this node */
e_direction RR_Node_Base::get_direction() const{
	return this->direction;
}

/* sets the rr type of this node */
void RR_Node_Base::set_rr_type(e_rr_type t){
	this->type = t;
}

/* sets xlow, ylow, and span fields of this node. note that only CHANX
   and CHANY nodes are allowed to have a span greater than 1 */
void RR_Node_Base::set_coordinates(short x1, short y1, short x2, short y2){
	/* set xlow */
	if (x1 < x2){
		this->xlow = x1;
	} else {
		this->xlow = x2;
	}

	/* set ylow */
	if (y1 < y2){
		this->ylow = y1;
	} else {
		this->ylow = y2;
	}

	/* set span */
	int xspan = abs(x1-x2) + 1;
	int yspan = abs(y1-y2) + 1;

	if (xspan > 1 && yspan > 1){
		/* a node that spans multiple CLBs in both the x and y directions?? */
		WTHROW(EX_GRAPH, "Routing node of type " << this->get_rr_type_string() << " has both x and y spans greater than 1.");
	}

	this->span = max(xspan, yspan);
}

/* set node resistance */
void RR_Node_Base::set_R(float res){
	this->R = res;
}

/* set node capacitance */
void RR_Node_Base::set_C(float cap){
	this->C = cap;
}

/* set pin-track-class number of the node */
void RR_Node_Base::set_ptc_num(short ptc){
	this->ptc_num = ptc;
}

/* set node fan-in */
void RR_Node_Base::set_fan_in(short f){
	this->fan_in = f;
}

/* set node direction */
void RR_Node_Base::set_direction(e_direction dir){
	this->direction = direction;
}

/*==== END RR_Node_Base Class ====*/


/*==== RR_Node Class ====*/
/* Constructor initializes everything to UNDEFINED */
RR_Node::RR_Node(){
	this->num_in_edges = UNDEFINED;
	this->weight = UNDEFINED;
	this->in_edges = NULL;
	this->in_switches = NULL;
	this->clear_demand();

	this->num_lb_sources_and_sinks = UNDEFINED;
	this->path_count_history_radius = UNDEFINED;

	this->virtual_source_node_ind = UNDEFINED;

	pthread_mutex_init(&this->my_mutex, NULL);

	this->highlight = false;
}

/* allocate the in_edges and in_switches array and sets num_in_edges */
void RR_Node::alloc_in_edges_and_switches(short n_edges){
	this->in_edges = new int[n_edges];
	this->in_switches = new short[n_edges];
	this->num_in_edges = n_edges;
}

/* allocate source/sink path history structure */
void RR_Node::alloc_source_sink_path_history(int set_num_lb_sources_and_sinks){
	int history_radius = 0;		//TODO: this should really be a command line option that makes its way to this function!!!
	e_rr_type rr_type = this->get_rr_type();

	/* want to only keep path count history for opin/ipin/chanx/chany */
	if (rr_type == OPIN || rr_type == IPIN || rr_type == CHANX || rr_type == CHANY){

		if (rr_type == OPIN || rr_type == IPIN){
			/* want to only keep path history for own CLB */
			history_radius = 0;
		}

		/* allocate */
		this->source_sink_path_history = new float** [history_radius+1];
		for (int iradius = 0; iradius <= history_radius; iradius++){
			int circumference = max(1, 4*iradius);
			this->source_sink_path_history[iradius] = new float* [circumference];

			for (int ic = 0; ic < circumference; ic++){
				this->source_sink_path_history[iradius][ic] = new float [set_num_lb_sources_and_sinks];

				/* initialize elements to UNDEFINED */
				for (int is = 0; is < set_num_lb_sources_and_sinks; is++){
					this->source_sink_path_history[iradius][ic][is] = UNDEFINED;
				}
			}
		}

		this->path_count_history_radius = history_radius;
		this->num_lb_sources_and_sinks = set_num_lb_sources_and_sinks;
	} else {
		this->num_lb_sources_and_sinks = UNDEFINED;
	}
}

/* freen in-edges and switches */
void RR_Node::free_in_edges_and_switches(){
	delete [] this->in_edges;
	delete [] this->in_switches;
	this->in_edges = NULL;
	this->in_switches = NULL;

	this->num_in_edges = UNDEFINED;
}

/* frees allocated members. extends the parent function of the same name */
void RR_Node::free_allocated_members(){
	/* call the parent function */
	RR_Node_Base::free_allocated_members();

	/* free edges and switches structures */
	this->free_in_edges_and_switches();

	/* free path count history structure */
	if (this->path_count_history_radius > 0){
		int radius = this->path_count_history_radius;

		for (int iradius = 0; iradius <= radius; iradius++){
			int circumference = max(1, 4*iradius);

			for (int ic = 0; ic < circumference; ic++){
				delete this->source_sink_path_history[iradius][ic];
			}
			delete [] this->source_sink_path_history[iradius];
		}
		delete [] this->source_sink_path_history;
		this->source_sink_path_history = NULL;

		this->path_count_history_radius = UNDEFINED;
		this->num_lb_sources_and_sinks = UNDEFINED;
	}

	pthread_mutex_destroy(&this->my_mutex);
}

/* sets node demand to 0 */
void RR_Node::clear_demand(){
	this->demand = 0.0;
}

/* increment node demand by specified value */
void RR_Node::increment_demand(double value){
	pthread_mutex_lock(&this->my_mutex);
	this->demand += value;
	pthread_mutex_unlock(&this->my_mutex);
}

/* sets weight of this node */
void RR_Node::set_weight(){
	/* weight of node is its wirelength usage */
	short x_low, y_low, x_high, y_high;
	x_low = this->get_xlow();
	y_low = this->get_ylow();
	x_high = this->get_xhigh();
	y_high = this->get_yhigh();

	//float my_weight = (float)(x_high - x_low) + (y_high - y_low);
	//if (this->get_rr_type() == CHANX || this->get_rr_type() == CHANY){
	//	my_weight += 1.0;
	//}


	float my_weight = 0.0;
	if (this->get_rr_type() == CHANX || this->get_rr_type() == CHANY){
		my_weight = 1.0 + this->demand*((float)this->get_span() + 1.0);
		my_weight = ceil(my_weight);
	}
	
	this->weight = my_weight;
}

/* sets the index of the virtual source node corresponding to this node. can be used for enumerating paths from non-source nodes */
void RR_Node::set_virtual_source_node_ind(int node_ind){
	this->virtual_source_node_ind = node_ind;
}

/* returns node demand */
double RR_Node::get_demand(User_Options *user_opts) const{
	double return_value;

	if (user_opts->use_routing_node_demand <= 0){
		////XXX TEST: DELETE ME
		//e_rr_type my_type = this->get_rr_type();
		//if (my_type == IPIN || my_type == OPIN)
		//	return 0.0;

		/* return demand recorded at this node */
		return_value = this->demand;
	} else {
		/* user has specified that a specific demand be used for routing nodes (those of CHANX/CHANY type) */
		e_rr_type rr_type = this->get_rr_type();
		if (rr_type == CHANX || rr_type == CHANY){
			return_value = user_opts->use_routing_node_demand;
		} else {
			return_value = 0.0;
		}
	}

	return return_value;
}

/* returns number of edges incident to this node */
short RR_Node::get_num_in_edges() const{
	return this->num_in_edges;
}

/* returns weight of this node */
float RR_Node::get_weight() const{
	return this->weight;
}

/* returns index of virtual source node corresponding to this node */
int RR_Node::get_virtual_source_node_ind() const{
	return this->virtual_source_node_ind;
}

/* increments path count history at this node due to the specified target node.
   the specified target node is either the source or sink of a connection that
   carries paths through *this* node */
void RR_Node::increment_path_count_history(float increment_val, RR_Node &target_node){
	this->access_path_count_history(increment_val, target_node, true);
}

/* returns path count history at this node due to the specified target node.
   the specified target node is either the source or sink of a connection that
   carries paths through *this* node.
   returns UNDEFINED if this node doesn't carry relevant path count info */
float RR_Node::get_path_count_history(RR_Node &target_node){
	float result = this->access_path_count_history(UNDEFINED, target_node, false);
	return result; 
}

/* Increments + returns path count history, or simply returns path count history
   of this node based on the 'increment' bool variable */
float RR_Node::access_path_count_history(float increment_val, RR_Node &target_node, bool increment){
	float result = UNDEFINED;

	e_rr_type target_type = target_node.get_rr_type();
	if (target_type != SOURCE && target_type != SINK){
		WTHROW(EX_PATH_ENUM, "Cannot access node's path count history if target node is not SOURCE or SINK");
	}
	if (increment_val < 0 && increment){
		WTHROW(EX_PATH_ENUM, "Cannot increment node's path count history by a negative value");
	}

	int target_x = target_node.get_xlow();
	int target_y = target_node.get_ylow();
	int target_ptc = target_node.get_ptc_num();

	e_rr_type my_type = this->get_rr_type();
	if (my_type == OPIN || my_type == IPIN /*|| my_type == CHANX || my_type == CHANY*/){		//FIXME: why was this commented out? why didn't i comment it? :(
		int radius = this->path_count_history_radius;
		int my_x = this->get_xlow();
		int my_y = this->get_ylow();

		int diff_x = my_x - target_x;
		int diff_y = my_y - target_y;

		int target_dist = abs(diff_x) + abs(diff_y);

		if (target_dist <= radius){
			/* have allocated memory to keep history for specified target coordinate */
			int arc = UNDEFINED;

			/* the radius coordinate of the target is given by 'target_dist'. the arc coordinate
			   of the target needs to be calculated based on x_diff & y_diff */
			if (target_dist == 0){
				arc = 0;
			} else if (diff_x > 0 && diff_y >= 0){
				/* quadrant 1 */
				arc = diff_y;
			} else if (diff_x <= 0 && diff_y > 0){
				/* quadrant 2 */
				arc = -diff_x + target_dist;
			} else if (diff_x < 0 && diff_y <= 0){
				/* quadrant 3 */
				arc = -diff_y + 2*target_dist;
			} else if (diff_x >= 0 && diff_y < 0){
				/* quadrant 4 */
				arc = diff_x + 3*target_dist;
			}

			/* perform access */
			float path_count = this->source_sink_path_history[target_dist][arc][target_ptc];
			if (increment){
				if (path_count == UNDEFINED){
					path_count = increment_val;
				} else {
					path_count += increment_val;
				}

				/* multiple threads may be incrementing path counts -- use mutex to synchronize */
				pthread_mutex_lock(&this->my_mutex);
				this->source_sink_path_history[target_dist][arc][target_ptc] = path_count;
				pthread_mutex_unlock(&this->my_mutex);
			}
			result = path_count;
		} else {
			/* have NOT allocated memory to keep history for specified target coordinate */
			result = UNDEFINED;
		}

	} else {
		result = UNDEFINED;
		//WTHROW(EX_PATH_ENUM, "Cannot access path count history for a node that is not of opin/ipin/chanx/chany type");
	}

	return result;
}
/*==== END RR_Node Class ====*/


/*==== RR_Switch_Inf Class ====*/
/* constructor initializes member variables to UNDEFINED */
RR_Switch_Inf::RR_Switch_Inf(){
	this->buffered = false;
	this->R = UNDEFINED;
	this->Cin = UNDEFINED;
	this->Cout = UNDEFINED;
	this->Tdel = UNDEFINED;
	this->mux_trans_size = UNDEFINED;
	this->buf_size = UNDEFINED;
}

/* return whether this switch is buffered */
bool RR_Switch_Inf::get_buffered() const{
	return this->buffered;
}

/* get resistance to go through the switch */
float RR_Switch_Inf::get_R() const{
	return this->R;
}

/* get switch input capacitance */
float RR_Switch_Inf::get_Cin() const{
	return this->Cin;
}

/* get switch output capacitance */
float RR_Switch_Inf::get_Cout() const{
	return this->Cout;
}

/* get switch intrinsic delay (doesn't account for R/C) */
float RR_Switch_Inf::get_Tdel() const{
	return this->Tdel;
}

/* get size (in MTUs) of transistors used to build the switch mux */
float RR_Switch_Inf::get_mux_trans_size() const{
	return this->mux_trans_size;
}

/* get the switch buffer size */
float RR_Switch_Inf::get_buf_size() const{
	return this->buf_size;
}

/* set if this switch is buffered */
void RR_Switch_Inf::set_buffered(bool buf){
	this->buffered = buf;
}

/* set resistance to go through the switch */
void RR_Switch_Inf::set_R(float res){
	this->R = res;
}

/* set switch input capacitance */
void RR_Switch_Inf::set_Cin(float input_C){
	this->Cin = input_C;
}

/* set switch output capacitance */
void RR_Switch_Inf::set_Cout(float output_C){
	this->Cout = output_C;
}

/* set switch intrinsic delay (doesn't account for R/C) */
void RR_Switch_Inf::set_Tdel(float T){
	this->Tdel = T;
}

/* set size (in MTUs) of transistors used to build the switch mux */
void RR_Switch_Inf::set_mux_trans_size(float mt_size){
	this->mux_trans_size = mt_size;
}

/* set switch buffer size */
void RR_Switch_Inf::set_buf_size(float b_size){
	this->buf_size = b_size;
}
/*==== END RR_Switch_Inf Class ====*/



/*==== Pin_Class Class ====*/
/* Constructor initialized pin type to OPEN */
Pin_Class::Pin_Class(){
	this->type = OPEN;
}

/* returns the pin type represented by this pin class */
e_pin_type Pin_Class::get_pin_type() const{
	return this->type;
}

/* returns number of pins belonging to this pin class */
int Pin_Class::get_num_pins() const{
	return (int)this->pinlist.size();
}

/* sets the pin type of this pin class */
void Pin_Class::set_pin_type(e_pin_type t){
	this->type = t;
}
/*==== END Pin_Class Class ====*/


/*==== Physical_Type_Descriptor Class ====*/
/* Constructor initializes member variables to UNDEFINED */
Physical_Type_Descriptor::Physical_Type_Descriptor(){
	this->name = "UNDEFINED";
	this->index = UNDEFINED;
	this->num_pins = UNDEFINED;
	this->width = UNDEFINED;
	this->height = UNDEFINED;
	this->num_drivers = UNDEFINED;
	this->num_receivers = UNDEFINED;
}

/* returns the name of this block type */
std::string Physical_Type_Descriptor::get_name() const{
	return this->name;
}

/* returns the index of this block type in an array */
int Physical_Type_Descriptor::get_index() const{
	return this->index;
}

/* returns the number of pins that belong to this block type */
int Physical_Type_Descriptor::get_num_pins() const{
	return this->num_pins;
}

/* returns the width (in CLB spans) of this block type */
int Physical_Type_Descriptor::get_width() const{
	return this->width;
}

/* returns the height (in CLB spans) of this block type */
int Physical_Type_Descriptor::get_height() const{
	return this->height;
}

/* returns the number of pins on this block type that are drivers */
int Physical_Type_Descriptor::get_num_drivers() const{
	return this->num_drivers;
}

/* returns the number of pins on this block type that are receivers */
int Physical_Type_Descriptor::get_num_receivers() const{
	return this->num_receivers;
}

/* sets the name of this block type */
void Physical_Type_Descriptor::set_name(std::string n){
	this->name = n;
}

/* sets the index of this block type within an array TODO: which array? */
void Physical_Type_Descriptor::set_index(int ind){
	this->index = ind;
}

/* sets the number of pins that belong to this block type */
void Physical_Type_Descriptor::set_num_pins(int npins){
	this->num_pins = npins;
}

/* sets the width (in CLB spans) of this block type */
void Physical_Type_Descriptor::set_width(int w){
	this->width = w;
}

/* sets the height (in CLB spans) of this block type */
void Physical_Type_Descriptor::set_height(int h){
	this->height = h;
}

/* sets the number of pins on this block type that are drivers */
void Physical_Type_Descriptor::set_num_drivers(int num_d){
	this->num_drivers = num_d;
}

/* sets the number of pins on this block type that are receivers */
void Physical_Type_Descriptor::set_num_receivers(int num_r){
	this->num_receivers = num_r;
}
/*==== END Physical_Type_Descriptor Class ====*/

/*==== Grid_Tile Class ====*/
/* Constructor initializes member variables to UNDEFINED */
Grid_Tile::Grid_Tile(){
	this->type_index = UNDEFINED;
	this->type_width_offset = UNDEFINED;
	this->type_height_offset = UNDEFINED;
}

/* returns the index of the corresponding physical block type */
int Grid_Tile::get_type_index() const{
	return this->type_index;
}
/* returns the width offset from the block's origin tile at this tile */
int Grid_Tile::get_width_offset() const{
	return this->type_width_offset;
}

/* returns the height offset from the block's origin tile at this tile */
int Grid_Tile::get_height_offset() const{
	return this->type_height_offset;
}

/* sets the index of the block type that is located at this tile */
void Grid_Tile::set_type_index(int ind){
	this->type_index = ind;
}

/* sets the width offset from the block's origin tile at this tile */
void Grid_Tile::set_width_offset(int w_offset){
	this->type_width_offset = w_offset;
}

/* sets the height offset from the block's origin tile at this tile */
void Grid_Tile::set_height_offset(int h_offset){
	this->type_height_offset = h_offset;
}
/*==== END Grid_Tile Class ====*/


/*==== Arch_Structs Class ====*/
Arch_Structs::Arch_Structs(){
	this->fill_type_index = UNDEFINED;
}

/* allocate and create the specified number of uninitialized block type entries */
void Arch_Structs::alloc_and_create_block_type(int n_block_types){
	this->block_type.assign(n_block_types, Physical_Type_Descriptor());
}

/* allocate and create the corresponding number of uninitialized grid entries */
void Arch_Structs::alloc_and_create_grid(int x_size, int y_size){
	vector<Grid_Tile> y_vec;
	y_vec.assign(y_size, Grid_Tile());

	this->grid.assign(x_size, y_vec);
}

/* sets 'fill_type_index' according to the most common block in the grid */
void Arch_Structs::set_fill_type(){
	int grid_size_x;
	int grid_size_y;

	int num_block_types = this->get_num_block_types();

	vector<int> type_counts;
	type_counts.assign(num_block_types, 0);

	this->get_grid_size(&grid_size_x, &grid_size_y);

	/* traverse each block of the grid */
	for (int ix = 0; ix < grid_size_x; ix++){
		for (int iy = 0; iy < grid_size_y; iy++){
			Grid_Tile *tile = &this->grid[ix][iy];

			/* want to make sure we count 'large' blocks only once */
			if (tile->get_width_offset() != 0 || tile->get_height_offset() != 0){
				continue;
			}

			int block_type_index = tile->get_type_index();
			type_counts[block_type_index]++;
		}
	}

	/* with all blocks of the grid traversed, figure out which block type is the most common */
	int most_common_type = 0;
	int most_common_type_num = type_counts[0];
	for (int itype = 1; itype < num_block_types; itype++){
		int this_num = type_counts[itype];

		if (this_num > most_common_type_num){
			most_common_type = itype;
			most_common_type_num = this_num;
		}
	}
	this->fill_type_index = most_common_type;
}

/* sets x-directed and y-directed channel widths based on rr node indices */
void Arch_Structs::set_chanwidth(Routing_Structs *routing_structs){
	t_rr_node_index &rr_node_index = routing_structs->rr_node_index;

	int x_entries, y_entries;
	this->get_grid_size(&x_entries, &y_entries);

	/* allocate structures */
	this->chanwidth_x.assign(x_entries, vector<int>(y_entries, 0));
	this->chanwidth_y.assign(x_entries, vector<int>(y_entries, 0));

	/* now count the number of nodes in each x-directed and y-directed channel */
	for (int ix = 0; ix < x_entries; ix++){
		for (int iy = 0; iy < y_entries; iy++){
			this->chanwidth_x[ix][iy] = (int)rr_node_index[CHANX][ix][iy].size();
			this->chanwidth_y[ix][iy] = (int)rr_node_index[CHANY][ix][iy].size();
		}
	}
}

/* returns 'fill_type_index' -- the index of the most common block in the grid */
int Arch_Structs::get_fill_type_index() const{
	return this->fill_type_index;
}

/* returns the x and y sizes of the grid */
void Arch_Structs::get_grid_size(int *x_size, int *y_size) const{
	int grid_size_x;
	int grid_size_y;

	grid_size_x = (int)this->grid.size();
	if (grid_size_x > 0){
		grid_size_y = (int)this->grid[0].size();
	} else {
		grid_size_y = 0;
	}

	(*x_size) = grid_size_x;
	(*y_size) = grid_size_y;
}

/* returns number of physical block types */
int Arch_Structs::get_num_block_types() const{
	return (int)this->block_type.size();
}

/*==== END Arch_Structs Class ====*/

/*==== Routing_Structs Class ====*/
/* allocate and create the specified number of uninitialized rr nodes */
void Routing_Structs::alloc_and_create_rr_node(int n_rr_nodes){
	this->rr_node.assign(n_rr_nodes, RR_Node());
	//this->rr_node = new RR_Node[n_rr_nodes];
	//this->num_rr_nodes = n_rr_nodes;
}
/* allocates path count history structures for each node */
void Routing_Structs::alloc_rr_node_path_histories(int num_lb_sources_and_sinks){
	for (int inode = 0; inode < this->get_num_rr_nodes(); inode++){
		RR_Node *node = &this->rr_node[inode];
		node->alloc_source_sink_path_history(num_lb_sources_and_sinks);
	}
}

/* allocate and create the specified number of uninitialized rr switch inf entries */
void Routing_Structs::alloc_and_create_rr_switch_inf(int n_rr_switches){
	this->rr_switch_inf.assign(n_rr_switches, RR_Switch_Inf());
}

/* allocate and create the corresponding number of rr node index entries */
void Routing_Structs::alloc_and_create_rr_node_index(int num_rr_types, int x_size, int y_size){
	vector<int> ptc_vec;
	vector< vector<int> > y_vec;
	vector< vector< vector<int> > > x_vec;

	y_vec.assign(y_size, ptc_vec);
	x_vec.assign(x_size, y_vec);
	this->rr_node_index.assign(num_rr_types, x_vec);
}

/* initializes node weights */
void Routing_Structs::init_rr_node_weights(){
	int num_nodes = this->get_num_rr_nodes();
	
	for (int inode = 0; inode < num_nodes; inode++){
		this->rr_node[inode].set_weight();
	}
}

/* returns number of rr nodes */
int Routing_Structs::get_num_rr_nodes() const{
	return (int)this->rr_node.size();
	//return this->num_rr_nodes;
}
/*==== END Routing_Structs Class ====*/



/*==== SS_Distances Class ====*/
SS_Distances::SS_Distances(){
	this->clear();
}

/* sets distance to source */
void SS_Distances::set_source_distance(int set_source_dist){
	this->source_distance = set_source_dist;
}

/* sets distance to sink */
void SS_Distances::set_sink_distance(int set_sink_dist){
	this->sink_distance = set_sink_dist;
}

/* sets whether this node has been visited from source */
void SS_Distances::set_visited_from_source(bool set_visited){
	this->visited_from_source = set_visited;
}

/* sets whether this node has been visited from sink */
void SS_Distances::set_visited_from_sink(bool set_visited){
	this->visited_from_sink = set_visited;
}

/* sets shortest # hops from source to specified value */
void SS_Distances::set_source_hops(int set_src_hops){
	this->source_hops = set_src_hops;
}

/* sets shortest # hops to sink to specified value */
void SS_Distances::set_sink_hops(int set_snk_hops){
	this->sink_hops = set_snk_hops;
}

/* sets whether corresponding node has already been visited during a traversal to calculate source hops */
void SS_Distances::set_visited_from_source_hops(bool visited){
	this->visited_from_source_hops = visited;
}

/* sets whether corresponding node has already been visited during a traversal to calculate sink hops */
void SS_Distances::set_visited_from_sink_hops(bool visited){
	this->visited_from_sink_hops = visited;
}

/* resets source/sink distances to UNDEFINED */
void SS_Distances::clear(){
	this->source_distance = UNDEFINED;
	this->sink_distance = UNDEFINED;
	this->visited_from_source = false;
	this->visited_from_sink = false;

	this->source_hops = UNDEFINED;
	this->sink_hops = UNDEFINED;
	this->visited_from_source_hops = false;
	this->visited_from_sink_hops = false;
}

/* returns distance to source */
int SS_Distances::get_source_distance() const{
	return this->source_distance;
}

/* returns distance to sink */
int SS_Distances::get_sink_distance() const{
	return this->sink_distance;
}

/* returns whether corresponding node has been visited from source */
bool SS_Distances::get_visited_from_source() const{
	return this->visited_from_source;
}

/* returns whether corresponding node has been visited from sink */
bool SS_Distances::get_visited_from_sink() const{
	return this->visited_from_sink;
}

/* gets shortest # hops from source to specified value */
int SS_Distances::get_source_hops() const{
	return this->source_hops;
}

/* gets shortest # hops to sink to specified value */
int SS_Distances::get_sink_hops() const{
	return this->sink_hops;
}

/* gets whether corresponding node has already been visited during a traversal to calculate source hops */
bool SS_Distances::get_visited_from_source_hops() const{
	return this->visited_from_source_hops;
}

/* gets whether corresponding node has already been visited during a traversal to calculate sink hops */
bool SS_Distances::get_visited_from_sink_hops() const{
	return this->visited_from_sink_hops;
}

/* returns true if the specified node has paths running through it from source to sink that are below
   the maximum allowable weight (i.e. node is legal) */
bool SS_Distances::is_legal(int my_node_weight, int max_path_weight) const{
	bool result;

	int source_dist = this->get_source_distance();
	int sink_dist = this->get_sink_distance();

	if (source_dist == UNDEFINED || sink_dist == UNDEFINED){
		result = false;
	} else {
		if (source_dist + sink_dist - my_node_weight <= max_path_weight){
			result = true;
		} else {
			result = false;
		}
	}

	return result;
}
/*==== END SS_Distances Class ====*/


/*==== Node_Waiting Class ====*/

Node_Waiting::Node_Waiting(){
	this->node_ind = UNDEFINED;
	this->path_weight = UNDEFINED;
	this->source_dist = UNDEFINED;
}

/* set methods */
void Node_Waiting::set( int set_ind, int set_path_weight, int set_source_dist ){
	this->node_ind = set_ind;
	this->path_weight = set_path_weight;
	this->source_dist = set_source_dist;
}

/* clears variables */
void Node_Waiting::clear(){
	this->node_ind = UNDEFINED;
	this->path_weight = UNDEFINED;
	this->source_dist = UNDEFINED;
}

/* returns index of node that this structure is associated with -- used as a tertiary sort to the cycle-breaking structure */
int Node_Waiting::get_node_ind() const{
	return this->node_ind;
}

/* returns path weight associated with the node this structure represents -- used as a primary sort to the cycle-breaking structure */
int Node_Waiting::get_path_weight() const{
	return this->path_weight;
}

/* returns distance to source associated with the corresponding node -- used as a secondary sort to the cycle-breaking structure */
int Node_Waiting::get_source_distance() const{
	return this->source_dist;
}

/* overload < for purposes of storing class objects in maps/sets */
bool Node_Waiting::operator < (const Node_Waiting &obj) const{
	bool result;
	
	/* primary sorting based on path_weight, secondary on source distance, tertiary on node index */
	if (this->path_weight > obj.get_path_weight()){
		result = true;
	} else {
		if (this->path_weight == obj.get_path_weight()){
			if (this->source_dist < obj.get_source_distance()){
				result = true;
			} else {
				if (this->source_dist == obj.get_source_distance()){
					if (this->node_ind < obj.get_node_ind()){
						result = true;
					} else {
						result = false;
					}
				} else {
					result = false;
				}
			}

		} else {
			result = false;
		}
	}

	return result;
}
/*==== END Node_Waiting Class ====*/


/*==== Node_Buckets Class ====*/
Node_Buckets::Node_Buckets(){
	this->num_source_buckets = UNDEFINED;
	this->num_sink_buckets = UNDEFINED;
}

Node_Buckets::Node_Buckets(int max_path_weight_bound){
	this->num_source_buckets = UNDEFINED;
	this->num_sink_buckets = UNDEFINED;

	this->alloc_source_sink_buckets(max_path_weight_bound+1, max_path_weight_bound+1);	//[0..max_path_weight+bound]
}

/* allocates the specified number of source and sink buckets */
void Node_Buckets::alloc_source_sink_buckets(int set_num_source_buckets, int set_num_sink_buckets){
	if (set_num_source_buckets != set_num_sink_buckets){
		WTHROW(EX_INIT, "number of source and sink buckets is expected to be equal");
	}

	this->source_buckets = new float[set_num_source_buckets];
	this->sink_buckets = new float[set_num_sink_buckets];

	/* initialize to 0 */
	for (int ibucket = 0; ibucket < set_num_source_buckets; ibucket++){
		this->source_buckets[ibucket] = UNDEFINED;
		this->sink_buckets[ibucket] = UNDEFINED;
	}

	this->num_source_buckets = set_num_source_buckets;
	this->num_sink_buckets = set_num_sink_buckets;
}

/* deallocate memory for bucket structures */
void Node_Buckets::free_source_sink_buckets(){
	//TODO: does this never get called?
	delete [] this->source_buckets;
	delete [] this->sink_buckets;
	this->source_buckets = NULL;
	this->sink_buckets = NULL;

	this->num_source_buckets = 0;
	this->num_sink_buckets = 0;
	this->bucket_mode = BY_PATH_WEIGHT;
}

/* resets all bucket entries to 0 */
void Node_Buckets::clear(){
	for (int i = 0; i < this->num_source_buckets; i++){
		this->source_buckets[i] = UNDEFINED;
	}

	for (int i = 0; i < this->num_sink_buckets; i++){
		this->sink_buckets[i] = UNDEFINED;
	}
}

/* resets all bucket entries up to and including the index specified to 0 */
void Node_Buckets::clear_up_to(int max_ind){
	for (int i = 0; i <= max_ind; i++){
		this->source_buckets[i] = UNDEFINED;
	}

	for (int i = 0; i <= max_ind; i++){
		this->sink_buckets[i] = UNDEFINED;
	}
}

/* sets the mode of the node buckets */
void Node_Buckets::set_bucket_mode(e_bucket_mode mode){
	this->bucket_mode = mode;
}


/* returns number of buckets associated with connections to source */
int Node_Buckets::get_num_source_buckets() const{
	return this->num_source_buckets;
}

/* returns number of buckets associated with connections to sink */
int Node_Buckets::get_num_sink_buckets() const{
	return this->num_sink_buckets;
}

/* returns the mode of the node buckets */
e_bucket_mode Node_Buckets::get_bucket_mode() const{
	return this->bucket_mode;
}


/* returns number of legal paths which go through the node associated with this structure */
float Node_Buckets::get_num_paths(int my_node_weight, int my_dist_to_source, int max_path_weight ) const{

	float paths_through_node = 0;

	float incremental_sink_paths = 0;
	int next_j = my_node_weight + 1;

	for (int j = 0; j < next_j; j++){
		if (this->sink_buckets[j] != UNDEFINED){
			incremental_sink_paths += this->sink_buckets[j];
		}
	}

	for (int i = max_path_weight; i >= my_dist_to_source; i--){
		if (this->source_buckets[i] != UNDEFINED){
			paths_through_node += this->source_buckets[i] * incremental_sink_paths;
		}
		if (this->sink_buckets[next_j] != UNDEFINED){
			incremental_sink_paths += this->sink_buckets[next_j];
		}
		next_j++;
	}

	return paths_through_node;
}


/* returns the probability of this node being unreachable from source (the node structures must contain probabilities instead of path counts) */
float Node_Buckets::get_probability_not_reachable(int my_node_weight, float my_node_demand) const{
	float probability_unreachable = 1;

	/* the probability of the node being unreachable from source is the probability that none of its incoming paths are available
	   OR this node is unavailable. The incoming paths are assumed to all be independent of eachother; this is really not the case
	   because many paths share the same nodes upstream -- remains to be seen if this assumption is OK */

	/* AND bucket probabilities */
	for (int ibucket = my_node_weight; ibucket < this->num_source_buckets; ibucket++){
		if (this->source_buckets[ibucket] != UNDEFINED){
			probability_unreachable *= this->source_buckets[ibucket];
		}
	}

	my_node_demand = min(1.0F, my_node_demand);

	/* OR with the probability of this node being unavailable */
	probability_unreachable = probability_unreachable + my_node_demand - (probability_unreachable * my_node_demand);

	return probability_unreachable;
}
/*==== END Node_Buckets Class ====*/
	

/*==== Node_Topological_Info Class ====*/
Node_Topological_Info::Node_Topological_Info(){
	this->clear();
}

/* resets variables. does not deallocate node buckets structure (only clears contents) */
void Node_Topological_Info::clear(){
	this->times_visited_from_source = 0;
	this->times_visited_from_sink = 0;
	this->num_legal_in_nodes = UNDEFINED;
	this->num_legal_out_nodes = UNDEFINED;
	this->done_from_source = false;
	this->done_from_sink = false;
	this->node_level = UNDEFINED;
	this->node_smoothed = false;
	this->adjusted_demand = UNDEFINED;
	this->was_visited = false;

	this->node_waiting_info.clear();
	this->buckets.clear();
}

/* sets whether this node has has already been placed onto expansion queue for a traversal from source */
void Node_Topological_Info::set_done_from_source(bool set_val){
	this->done_from_source = set_val;
}

/* sets whether this node has has already been placed onto expansion queue for a traversal from source */
void Node_Topological_Info::set_done_from_sink(bool set_val){
	this->done_from_sink = set_val;
}


/* sets whether this node has been visited from source */
void Node_Topological_Info::increment_times_visited_from_source(){
	this->times_visited_from_source++;
}

/* sets whether this node has been visited from sink */
void Node_Topological_Info::increment_times_visited_from_sink(){
	this->times_visited_from_sink++;
}

/* sets topological traversal level of corresponding node */
void Node_Topological_Info::set_level(int value){
	this->node_level = value;
}

void Node_Topological_Info::set_times_visited_from_source(short val){
	this->times_visited_from_source = val;
}
void Node_Topological_Info::set_times_visited_from_sink(short val){
	this->times_visited_from_sink = val;
}
void Node_Topological_Info::set_num_legal_in_nodes(short val){
	this->num_legal_in_nodes = val;
}
void Node_Topological_Info::set_num_legal_out_nodes(short val){
	this->num_legal_out_nodes = val;
}
void Node_Topological_Info::set_node_smoothed(bool smoothed){
	this->node_smoothed = smoothed;
}
void Node_Topological_Info::set_adjusted_demand(float val){
	this->adjusted_demand = val;
}
void Node_Topological_Info::set_was_visited(bool val){
	this->was_visited = val;
}

/* returns whether corresponding node has been visited from source */
short Node_Topological_Info::get_times_visited_from_source() const{
	return this->times_visited_from_source;
}

/* returns whether corresponding node has been visited from sink */
short Node_Topological_Info::get_times_visited_from_sink() const{
	return this->times_visited_from_sink;
}

/* sets whether this node has has already been placed onto expansion queue for a traversal from source */
bool Node_Topological_Info::get_done_from_source() const{
	return this->done_from_source;
}

/* sets whether this node has has already been placed onto expansion queue for a traversal from source */
bool Node_Topological_Info::get_done_from_sink() const{
	return this->done_from_sink;
}

/* returns topological traversal level of the corresponding node */
int Node_Topological_Info::get_level() const{
	return this->node_level;
}

short Node_Topological_Info::get_num_legal_in_nodes() const{
	return this->num_legal_in_nodes;
}
short Node_Topological_Info::get_num_legal_out_nodes() const{
	return this->num_legal_out_nodes;
}
bool Node_Topological_Info::get_node_smoothed() const{
	return this->node_smoothed;
}
float Node_Topological_Info::get_adjusted_demand() const{
	return this->adjusted_demand;
}
bool Node_Topological_Info::get_was_visited() const{
	return this->was_visited;
}

/* returns number of legal nodes that have edges into this node. if this value is
   not yet set, then it gets set as well */
short Node_Topological_Info::set_and_or_get_num_legal_in_nodes(int my_node_index, t_rr_node &rr_node, t_ss_distances &ss_distances, int max_path_weight){

	if (this->num_legal_in_nodes == UNDEFINED){
		/* if not yet set, then calculate and set */
		int *edge_list;
		int num_edges;

		edge_list = rr_node[my_node_index].in_edges;
		num_edges = rr_node[my_node_index].get_num_in_edges();

		this->num_legal_in_nodes = this->get_num_legal_nodes(edge_list, num_edges, rr_node, ss_distances, max_path_weight);
	}

	return this->num_legal_in_nodes;
}

/* returns number of legal nodes to which this node has edges. if this value is
   not yet set, then it gets set as well */
short Node_Topological_Info::set_and_or_get_num_legal_out_nodes(int my_node_index, t_rr_node &rr_node, t_ss_distances &ss_distances, int max_path_weight){

	if (this->num_legal_out_nodes == UNDEFINED){
		/* if not yet set, then calculate and set */
		int *edge_list;
		int num_edges;

		edge_list = rr_node[my_node_index].out_edges;
		num_edges = rr_node[my_node_index].get_num_out_edges();

		this->num_legal_out_nodes = this->get_num_legal_nodes(edge_list, num_edges, rr_node, ss_distances, max_path_weight);
	}

	return this->num_legal_out_nodes;
}

/* returns number of legal nodes on specified edge list */
short Node_Topological_Info::get_num_legal_nodes(int *edge_list, int num_edges, t_rr_node &rr_node, t_ss_distances &ss_distances, int max_path_weight){
	int num_legal_nodes = 0;

	/* check how many nodes belonging to this edge list are legal */
	for (int iedge = 0; iedge < num_edges; iedge++){
		int node_ind = edge_list[iedge];
		int node_weight = rr_node[node_ind].get_weight();

		//TODO: should check whether node can have legal path through *me* as opposed to a legal path through itself
		if ( ss_distances[node_ind].is_legal(node_weight, max_path_weight) ){
			num_legal_nodes++;
		}
	}

	return num_legal_nodes;	
}
/*==== END Node_Topological_Info Class ====*/
