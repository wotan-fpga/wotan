
#include <cmath>
#include "exception.h"
#include "analysis_reliability_poly.h"

using namespace std;

/* minimum possible hops (edges) from source to sink. source->opin->routing->ipin->sink --> 4 hops */
#define MIN_POSSIBLE_HOPS 4


class Poly_Coeff{
public:
	double coeff;
	int ind;

	Poly_Coeff(double c, int i){
		this->coeff = c;
		this->ind = i;
	}
};


	/*
	Reliability polynomial counts pathsets. N_l --> pathsets of cardinality l --> have l nodes.
	So I'm looking to find out how many paths there are with 'l' nodes, l+1 nodes, etc

	Furthermore, I'm looking to disregard all but the routing nodes

	When I talk about #hops, that refers to the number of edges between two nodes.
	If a path consists of n edges, then it consists on n+1 nodes (including source/sink/opin/ipin)
	So it's fine to keep paths according to the hop count -- it just becomes necessary to convert from
	"# hops in path" to "# routing nodes in path" during the computation of the reliability polynomial

	Another issue: can I really assume that I can get the N_{l+1} term exactly based on the N_l term??
	- Is it possible that two pathsets in N_l become the same pathset in N_{l+1} with the addition on 
	  a new node to each? Note that I'm only looking at routing nodes here.
		- @ Fs=3, a track cannot connect to two different tracks in the same channel.
		- above requires that two paths differ by only one track
		- @ Fs=3 I don't think this is possible -- should be able to compute N_{l+1} coefficient exactly

	Dilemma: should only routing nodes be included in the subgraph?
		- is that fair/unfair to architectures with pin equivalence?
			- the question is what probabilities should be assigned opins/ipins if they are to be included?? nothing fair, certainly...
		- but if ipins/opins are not considered, then only routing nodes remain, whereas the pathcount numbers take into account
		  the effects of ipins/opins as well
		- so I think I *have to* consider ipins/opins
	*/


/**** Function Declarations ****/

/* converts the # of hops (edges) from source to sink to the # of routing nodes between source and sink */
static int convert_hops_to_routing_nodes(int hops_count);

/* Returns a Sperner upper bound on the coefficient with one-less cardinality than 'higher_cardinality_coeff' */
static double sperner_bound_on_lower_coefficient(Poly_Coeff higher_cardinality_coeff, int num_routing_nodes);



/**** Function Definitions ****/

/* computes the reliability polynomial based on the sink node buckets (which contain the number of paths of each length from source to sink) */
double analyze_reliability_polynomial(int source_sink_hops, 		// # hops (edges) from source to sink
				     double *path_counts, 		// # paths at each cardinality of hops (edges)
				     int num_path_count_entries, 	// # entries in above array
				     int num_routing_nodes, 		// # routing nodes in subgraph
				     float routing_node_probability){	// probability (of operation) to be used for each routing node
	double probability = 0;

	if (routing_node_probability < 0){
		WTHROW(EX_PATH_ENUM, "Computing the reliability polynomial requires node probabilities to be >= 0. Got: " << routing_node_probability);
	}
	if (source_sink_hops < MIN_POSSIBLE_HOPS){
		WTHROW(EX_PATH_ENUM, "There should always be at least four hops from source to sink. Got a connection with " << source_sink_hops);
	}

	/* sanity check -- make sure there are no paths of length < source_sink_hops. TODO: get rid of this later */
	for (int ilength = 0; ilength < num_path_count_entries; ilength++){
		if (path_counts[ilength] > 0 && ilength < source_sink_hops){
			WTHROW(EX_PATH_ENUM, "number of hops (edges) from source to sink is " << source_sink_hops << ", but got path count of " << path_counts[ilength] <<
						" corresponding to " << ilength << " hops");
		}
	}

	/* a vector to hold the reliability polynomial coefficients */
	vector< Poly_Coeff > rel_poly;

	
	/************ Get coefficients based on N_l ************/
	
	/* get the first coefficient of the reliability polynomial */
	double first_term_value = path_counts[source_sink_hops];
	int first_term_subscript = convert_hops_to_routing_nodes(source_sink_hops);
	rel_poly.push_back( Poly_Coeff(first_term_value, first_term_subscript) );
	//cout << "" << first_term_value << " paths of cardinality " << first_term_subscript << " in graph of " << num_routing_nodes << " nodes" << endl;

	/* get the second coefficient based on the first */
	int second_term_hops = source_sink_hops + 1;
	int second_term_subscript = convert_hops_to_routing_nodes(second_term_hops);
	double second_term_value = 0;
	if (second_term_hops < num_path_count_entries){
		/* skip this cardinality if there are no path counts here */
		if (path_counts[second_term_hops] >= 0){
			second_term_value += path_counts[second_term_hops];
		}
	}
	int unused_nodes = num_routing_nodes - convert_hops_to_routing_nodes(source_sink_hops);

	if (unused_nodes >= 0){
		/* we are looking at routing nodes. a new pathset can be formed by looking at 
		   any minimUM pathset and adding any unused node to it. in total, 
		   			N_l * unused_nodes 
		   new pathsets of cardinality (l+1) formed based on N_l are formed without forming any new minimAL pathsets
		   and without overlap with eachother (at Fs=3 anyway) */
		second_term_value += rel_poly[0].coeff * unused_nodes;
	} else {
		//shouldn't get here
		WTHROW(EX_PATH_ENUM, "number of unused nodes is less than zero?? Got: " << unused_nodes);
	}
	rel_poly.push_back( Poly_Coeff(second_term_value, second_term_subscript) );
	//cout << "second coeff: " << second_term_value << " " << second_term_subscript << endl;

	/* get the value of the last coeff */
	int last_term_subscript = num_routing_nodes;
	if (last_term_subscript > second_term_subscript){
		double last_term_value = 1;
		rel_poly.push_back( Poly_Coeff(last_term_value, last_term_subscript) );
	}

	/* now get an upper bound on all the coefficients between the second one and the last one.
	   The bounding is based on the very last coefficient, N_{m} */
	int ind = 3;
	for (int isub = last_term_subscript-1; isub > second_term_subscript; isub--){
		Poly_Coeff previous_poly = rel_poly[ind-1];

		double new_coeff = sperner_bound_on_lower_coefficient(previous_poly, num_routing_nodes);

		rel_poly.push_back( Poly_Coeff(new_coeff, isub) );

		ind++;
	}


	//The code below was used to get a LOWER bound based on N_{l+1} coefficient. The lower bound was reaaaaaally low
	///* now get a lower bound on the rest of the polynomial coefficients */
	//for (int i = 2; i < num_routing_nodes; i++){
	//	double minpaths = 0;

	//	Poly_Coeff previous_coeff = rel_poly[i-1];

	//	int new_ind = previous_coeff.ind + 1;

	//	if (new_ind > num_routing_nodes){
	//		break;
	//	}

	//	if (new_ind < num_path_count_entries){
	//		minpaths = path_counts[new_ind];
	//	}

	//	double new_coeff = lower_bound_coeff_BBST(previous_coeff, new_ind, minpaths, num_routing_nodes);

	//	rel_poly.push_back( Poly_Coeff(new_coeff, new_ind) );
	//}


	/************ Calculate Rel Poly ************/
	vector< Poly_Coeff >::const_iterator it;
	probability = 0;
	for (it = rel_poly.begin(); it != rel_poly.end(); it++){
		double coeff = (double)it->coeff;
		double num_operational_nodes = (double)it->ind;
		double num_failed_nodes = (double)(num_routing_nodes - num_operational_nodes);
		probability += coeff * pow(routing_node_probability, num_operational_nodes) * pow((1-routing_node_probability), num_failed_nodes);
		//cout << "\tind: " << num_operational_nodes << "  coeff: " << coeff << endl;
	}

	//cout << "probability: " << probability << endl;

	//WTHROW(EX_OTHER, "GAR");
	return probability;
}

/* Returns a Sperner upper bound on the coefficient with one-less cardinality than 'higher_cardinality_coeff' */
static double sperner_bound_on_lower_coefficient(Poly_Coeff higher_cardinality_coeff, int num_routing_nodes){
	/*

	Sperner bound:

	N_{i} = (i+1)/(m-1) * N_{i+1}

	If you think about it, this is an INCREDIBLY loose bound :(

	*/

	double m = (double)num_routing_nodes;
	double i = (double)higher_cardinality_coeff.ind - 1;
	double N_iplus = higher_cardinality_coeff.coeff;

	double upper_bound = (i+1)/(m-i) * N_iplus;
	return upper_bound;
}


/* converts the # of hops (edges) from source to sink to the # of routing nodes between source and sink */
static int convert_hops_to_routing_nodes(int hops_count){
	/* -2 due to source->opin and ipin->sink hops. +1 because in a path of n edges there is n+1 nodes */
	int nodes_count = hops_count - 2 + 1;
	return nodes_count;
}







/* Returns a lower Sperner bound on the value of the coefficient that represents pathsets with cardinality 'new_ind'. 
   Uses the previous coefficient to get this value */
//static double lower_bound_coeff_BBST(Poly_Coeff previous_coeff, int new_ind, double minpaths_at_new_ind, int num_routing_nodes);	//TODO: not using this; also, this isn't BBST

///* Returns a lower Sperner bound on the value of the coefficient that represents pathsets with cardinality 'new_ind'. 
//   Uses the previous coefficient to get this value */
//static double lower_bound_coeff_BBST(Poly_Coeff previous_coeff, int new_ind, double minpaths_at_new_ind, int num_routing_nodes){		//TODO: not using this
//	double lower_bound = 0;
//
//	//actually, using sperner bound to make a lower bound on a coefficient with one-higher cardinality compared to 'Previous_Coeff'
//
//
//	double m = (double)num_routing_nodes;
//	double ip = (double)previous_coeff.ind;
//	double N_i = previous_coeff.coeff;
//	lower_bound = N_i * (m-ip)/(ip+1);
//	//lower_bound = floor(lower_bound);
//
//	//cout << "\tm: " << m << "  ip: " << ip << "  new_ind: " << new_ind << "  coeff: " << lower_bound << endl;
//	return lower_bound;
//}
