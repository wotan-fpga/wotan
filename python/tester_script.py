import os
import time
import numpy as np
import new_wotan_tester as wt

############ Paths ############
base_path = '/home/opetelin'
vtr_path = base_path + "/vtr"
wotan_path = base_path + "/wotan"

#arch_dir = vtr_path + '/vtr_flow/arch/timing'
arch_dir = wotan_path + '/arch'


############ "Golden" architecture ordering ############
vpr_ordering_6LUT = wotan_path + '/python/6LUT_vpr_ordering.txt'
vpr_ordering_4LUT = wotan_path + '/python/4LUT_vpr_ordering.txt'


############ Wotan Labels and Regex Expressions ############
#labels for regular Wotan output
labels_Rel = (
	'Fraction Enumerated',		#0
	'Total Demand',			#1
	'Normalized Demand',		#2
	'Normalized Square Demand',	#3
	'Demand Multiplier',		#4
	'Driver Metric',		#5
	'Fanout Metric',		#6
	'Routability Metric'		#7
)

#regex for regular Wotan output
regex_Rel = (
	'.*fraction enumerated: (\d*\.*\d+).*',
	'.*Total demand: (\d+\.*\d+).*',
	'.*Normalized demand: (\d+\.\d+).*',
	'.*Normalized squared demand: (\d+\.\d+).*',
	'.*Demand multiplier: (\d*\.*\d+).*',
	'.*Driver metric: (\d+\.*\d+).*',
	'.*Fanout metric: (\d+\.*\d+).*',
	'.*Routability metric: (\d+\.*\d*).*'
)



############ Wotan/VPR command line arguments ############
#VPR options are derived from test suite or arch point
wotan_opts_normal = '-rr_structs_file ' + vtr_path + '/vpr' + '/dumped_rr_structs.txt -nodisp -threads 10 -max_connection_length 8 -keep_path_count_history n'
#wotan_opts_normal = '-rr_structs_file ' + vtr_path + '/vpr' + '/dumped_rr_structs.txt -nodisp -threads 4 -max_connection_length 8 -keep_path_count_history y'
wotan_opts_rel_poly = '-rr_structs_file ' + vtr_path + '/vpr' + '/dumped_rr_structs.txt -nodisp -threads 4 -max_connection_length 2 -keep_path_count_history n -use_routing_node_demand 0.85'


wotan_opts = wotan_opts_normal

#Test type variable applies when 'run_all_tests_sequentially' is used. It is not used for 'run_architecture_comparisons' (which are hard-coded for a specific test type already)
test_type = 'binary_search_routability_metric'

#index into labels_Rel & regex_Rel that determines which regex'd value will be plotted on a graph
plot_index = 4



############ Run Tests ############
start_time = time.time()

tester = wt.Wotan_Tester(
		vtr_path = vtr_path,
		wotan_path = wotan_path,
		test_suite_2dlist = [],
		test_type = test_type
		)



### Get absolute metric for a list of architecture points ###
arch_list = wt.my_custom_archs_list()


vpr_arch_ordering = []
#vpr_arch_ordering = wt.read_file_into_split_string_list('./6LUT_new_ordering.txt')
tester.evaluate_architecture_list(arch_list, wotan_path + '/python/wotan_final_4LUT.txt', 
                                  wotan_opts,
                                  vpr_arch_ordering = vpr_arch_ordering)	#change to [] if you want to run VPR comparisons.



end_time = time.time()
print('Done. Took ' + str(round(end_time - start_time, 3)) + ' seconds')

