#ifndef ANALYSIS_RELIABILITY_POLY_H
#define ANALYSIS_RELIABILITY_POLY_H

#include "wotan_types.h"


/* computes the reliability polynomial based on the sink node buckets (which contain the number of paths of each length from source to sink) */
double analyze_reliability_polynomial(int source_sink_hops, float *path_counts, int num_path_lengths, int num_routing_nodes, float routing_node_probability);





#endif
