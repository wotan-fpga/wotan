import os
import time
import numpy as np
import wotan_tester as wt

############ Paths ############
base_path = "/autofs/fs1.ece/fs1.eecg.vaughn/opetelin"
vtr_path = base_path + "/vtr-verilog-to-routing"
wotan_path = base_path + "/wotan"

#arch_dir = vtr_path + '/vtr_flow/arch/timing'
arch_dir = wotan_path + '/arch'


############ Architectures ############
#wotan_archs = {'6LUT-iequiv' : arch_dir + '/k6_frac_N10_mem32K_40nm_only_clb.xml',
#               '4LUT-noequiv' : arch_dir + '/k6_frac_N10_mem32K_40nm_only_clb_lattice.xml'}
#
#vpr_archs = {'6LUT-iequiv' : arch_dir + '/k6_frac_N10_mem32K_40nm_test.xml',
#             '4LUT-noequiv' : arch_dir + '/k6_frac_N10_mem32K_40nm_test_lattice.xml'}


wotan_archs = {'6LUT-iequiv' : arch_dir + '/oleg_k6_N10_gate_boost_0.2V_22nm_only_clb.xml',
               '4LUT-noequiv' : arch_dir + '/oleg_k4_N8_I32_gate_boost_0.2V_22nm_noxbar_lut_equiv_only_clb.xml'}

vpr_archs = {'6LUT-iequiv' : arch_dir + '/oleg_k6_N10_gate_boost_0.2V_22nm.xml',
             '4LUT-noequiv' : arch_dir + '/oleg_k4_N8_I32_gate_boost_0.2V_22nm_noxbar_lut_equiv.xml'}

arch_dictionaries = wt.Archs(wotan_archs, vpr_archs)




############ Wotan Labels and Regex Expressions ############
#labels for regular Wotan output
labels_Rel = (
	'Fraction Enumerated',		#0
	'Total Demand',			#1
	'Normalized Demand',		#2
	'Normalized Square Demand',	#3
	'Demand multiplier',		#4
	'Total Prob',			#5
	'Pessimistic Prob'		#6
)

#regex for regular Wotan output
regex_Rel = (
	'.*fraction enumerated: (\d*\.*\d+).*',
	'.*Total demand: (\d+\.*\d+).*',
	'.*Normalized demand: (\d+\.\d+).*',
	'.*Normalized squared demand: (\d+\.\d+).*',
	'.*Opin demand: (\d*\.*\d+).*',
	'.*Total prob: (\d+\.*\d+).*',
	'.*Pessimistic prob: (\d+\.*\d*).*'
)



############ Wotan/VPR command line arguments ############
#VPR options are derived from test suite or arch point
wotan_opts_normal = '-rr_structs_file ' + vtr_path + '/vpr' + '/dumped_rr_structs.txt -nodisp -threads 7 -max_connection_length 2 -keep_path_count_history y'
wotan_opts_rel_poly = '-rr_structs_file ' + vtr_path + '/vpr' + '/dumped_rr_structs.txt -nodisp -threads 7 -max_connection_length 2 -keep_path_count_history n -use_routing_node_demand 0.85'


wotan_opts = wotan_opts_normal

#Test type variable applies when 'run_all_tests_sequentially' is used. It is not used for 'run_architecture_comparisons' (which are hard-coded for a specific test type already)
test_type = 'binary_search_pessimistic_prob'

#index into labels_Rel & regex_Rel that determines which regex'd value will be plotted on a graph
plot_index = 4



########### ALTERA-LIKE ARCHITECTURES ############
test_suites_6lut = []

#altera 6LUT -- fcout sweep with wilton and input equivalence
test_suites_6lut += [wt.make_test_group(num_suites=3,
                                  wirelengths=[1,2,4],
				  switchblocks=['wilton'],
				  arch_names=['6LUT-iequiv'],
				  arch_dictionaries=arch_dictionaries,
				  sweep_type='fcout',
				  sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
				  output_regex_list=regex_Rel,
				  output_label_list=labels_Rel,
				  plot_index=plot_index,
				  wotan_opts=wotan_opts
				 )]
#altera 6LUT -- fcin sweep with wilton and input equivalence
test_suites_6lut += [wt.make_test_group(num_suites=3,
                                  wirelengths=[1,2,4],
				  switchblocks=['wilton'],
				  arch_names=['6LUT-iequiv'],
				  arch_dictionaries=arch_dictionaries,
				  sweep_type='fcin',
				  sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
				  output_regex_list=regex_Rel,
				  output_label_list=labels_Rel,
				  plot_index=plot_index,
				  wotan_opts=wotan_opts
				 )]
#altera 6LUT -- fcout sweep with universal and input equivalence
test_suites_6lut += [wt.make_test_group(num_suites=3,
                                  wirelengths=[1,2,4],
				  switchblocks=['universal'],
				  arch_names=['6LUT-iequiv'],
				  arch_dictionaries=arch_dictionaries,
				  sweep_type='fcout',
				  sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
				  output_regex_list=regex_Rel,
				  output_label_list=labels_Rel,
				  plot_index=plot_index,
				  wotan_opts=wotan_opts
				 )]
#altera 6LUT -- fcin sweep with universal and input equivalence
test_suites_6lut += [wt.make_test_group(num_suites=3,
                                  wirelengths=[1,2,4],
				  switchblocks=['universal'],
				  arch_names=['6LUT-iequiv'],
				  arch_dictionaries=arch_dictionaries,
				  sweep_type='fcin',
				  sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
				  output_regex_list=regex_Rel,
				  output_label_list=labels_Rel,
				  plot_index=plot_index,
				  wotan_opts=wotan_opts
				 )]
#altera 6LUT -- fcout sweep with planar and input equivalence
test_suites_6lut += [wt.make_test_group(num_suites=3,
                                  wirelengths=[1,2,4],
				  switchblocks=['subset'],
				  arch_names=['6LUT-iequiv'],
				  arch_dictionaries=arch_dictionaries,
				  sweep_type='fcout',
				  sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
				  output_regex_list=regex_Rel,
				  output_label_list=labels_Rel,
				  plot_index=plot_index,
				  wotan_opts=wotan_opts
				 )]
#altera 6LUT -- fcin sweep with planar and input equivalence
test_suites_6lut += [wt.make_test_group(num_suites=3,
                                  wirelengths=[1,2,4],
				  switchblocks=['subset'],
				  arch_names=['6LUT-iequiv'],
				  arch_dictionaries=arch_dictionaries,
				  sweep_type='fcin',
				  sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
				  output_regex_list=regex_Rel,
				  output_label_list=labels_Rel,
				  plot_index=plot_index,
				  wotan_opts=wotan_opts
				 )]


########### LATTICE-LIKE ARCHITECTURES ############
test_suites_4lut = []

#lattice 4lut -- fcout sweep with wilton and input equivalence
test_suites_4lut += [wt.make_test_group(num_suites=3,
                                  wirelengths=[1,2,4],
				  switchblocks=['wilton'],
				  arch_names=['4LUT-noequiv'],
				  arch_dictionaries=arch_dictionaries,
				  sweep_type='fcout',
				  sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
				  output_regex_list=regex_Rel,
				  output_label_list=labels_Rel,
				  plot_index=plot_index,
				  wotan_opts=wotan_opts
				 )]
#lattice 4lut -- fcin sweep with wilton and input equivalence
test_suites_4lut += [wt.make_test_group(num_suites=3,
                                  wirelengths=[1,2,4],
				  switchblocks=['wilton'],
				  arch_names=['4LUT-noequiv'],
				  arch_dictionaries=arch_dictionaries,
				  sweep_type='fcin',
				  sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
				  output_regex_list=regex_Rel,
				  output_label_list=labels_Rel,
				  plot_index=plot_index,
				  wotan_opts=wotan_opts
				 )]
#lattice 4lut -- fcout sweep with universal and input equivalence
test_suites_4lut += [wt.make_test_group(num_suites=3,
                                  wirelengths=[1,2,4],
				  switchblocks=['universal'],
				  arch_names=['4LUT-noequiv'],
				  arch_dictionaries=arch_dictionaries,
				  sweep_type='fcout',
				  sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
				  output_regex_list=regex_Rel,
				  output_label_list=labels_Rel,
				  plot_index=plot_index,
				  wotan_opts=wotan_opts
				 )]
#lattice 4lut -- fcin sweep with universal and input equivalence
test_suites_4lut += [wt.make_test_group(num_suites=3,
                                  wirelengths=[1,2,4],
				  switchblocks=['universal'],
				  arch_names=['4LUT-noequiv'],
				  arch_dictionaries=arch_dictionaries,
				  sweep_type='fcin',
				  sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
				  output_regex_list=regex_Rel,
				  output_label_list=labels_Rel,
				  plot_index=plot_index,
				  wotan_opts=wotan_opts
				 )]
#lattice 4lut -- fcout sweep with planar and input equivalence
test_suites_4lut += [wt.make_test_group(num_suites=3,
                                  wirelengths=[1,2,4],
				  switchblocks=['subset'],
				  arch_names=['4LUT-noequiv'],
				  arch_dictionaries=arch_dictionaries,
				  sweep_type='fcout',
				  sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
				  output_regex_list=regex_Rel,
				  output_label_list=labels_Rel,
				  plot_index=plot_index,
				  wotan_opts=wotan_opts
				 )]
#lattice 4lut -- fcin sweep with planar and input equivalence
test_suites_4lut += [wt.make_test_group(num_suites=3,
                                  wirelengths=[1,2,4],
				  switchblocks=['subset'],
				  arch_names=['4LUT-noequiv'],
				  arch_dictionaries=arch_dictionaries,
				  sweep_type='fcin',
				  sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
				  output_regex_list=regex_Rel,
				  output_label_list=labels_Rel,
				  plot_index=plot_index,
				  wotan_opts=wotan_opts
				 )]

test_suites = test_suites_6lut #+ test_suites_4lut


############ Run Tests ############
start_time = time.time()

tester = wt.Wotan_Tester(
		vtr_path = vtr_path,
		wotan_path = wotan_path,
		test_suite_2dlist = test_suites,
		test_type = test_type
		)

### Run test suites and plot groups of tests on the same graph
#tester.run_all_tests_sequentially()


### Run pairwise architecture comparisons
results_file = wotan_path + '/python/pair_test.txt'
#arch_pairs_list = tester.make_random_arch_pairs_list(40)
#arch_pairs_list = wt.my_custom_arch_pair_list(arch_dictionaries)
#tester.run_architecture_comparisons(arch_pairs_list, results_file, wotan_opts,
#                                    compare_against_VPR=True)

arch_list = tester.make_random_arch_list(4)
print(arch_list)
tester.evaluate_architecture_list(arch_list, wotan_path + '/python/absolute_ordering.txt', 
                                  wotan_opts,
                                  compare_against_VPR=True)


### Sweep on architecture over a range of channel widths
#arch_point = wt.Arch_Point_Info.from_str('len1_wilton_fcin0.75_fcout0.25_arch:4LUT-noequiv', arch_dictionaries)
##arch_point = wt.Arch_Point_Info.from_str('len2_universal_fcin0.15_fcout0.35_arch:4LUT-noequiv', arch_dictionaries)
#chan_sweep_results = tester.sweep_architecture_over_W(arch_point, np.arange(60, 110, 2).tolist())
#print(chan_sweep_results)

end_time = time.time()
print('Done. Took ' + str(round(end_time - start_time, 3)) + ' seconds')

