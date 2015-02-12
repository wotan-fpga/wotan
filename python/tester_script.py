import os
import time
import numpy as np
import wotan_tester as wt

############ Paths ############
base_path = "/home/oleg/Documents/work/UofT/Grad"
vtr_path = base_path + "/vtr-verilog-to-routing"
wotan_path = base_path + "/my_vtr/wotan"
wotan_arch = "k6_frac_N10_mem32K_40nm_only_clb.xml"
vpr_arch = "k6_frac_N10_mem32K_40nm_test.xml"



############ Wotan Labels and Regex Expressions ############
#labels for regular Wotan output
labels_Rel = (
	'Fraction Enumerated',
	'Total Demand',
	'Normalized Demand',
	'Normalized Square Demand',
	'Total Prob',
	'Pessimistic Prob'
)

#regex for regular Wotan output
regex_Rel = (
	'.*fraction enumerated: (\d*\.*\d+).*',
	'.*Total demand: (\d+\.*\d+).*',
	'.*Normalized demand: (\d+\.\d+).*',
	'.*Normalized squared demand: (\d+\.\d+).*',
	'.*Total prob: (\d+\.*\d+).*',
	'.*Pessimistic prob: (\d+\.*\d*).*'
)



############ Wotan/VPR command line arguments ############
vpr_opts = vtr_path + '/vtr_flow/arch/timing/' + wotan_arch + ' ../vtr_flow/benchmarks/blif/wiremap6/alu4.pre-vpr.blif -route_chan_width 70 -nodisp'

wotan_opts_normal = '-rr_structs_file ' + vtr_path + '/vpr' + '/dumped_rr_structs.txt -nodisp -threads 7 -max_connection_length 2 -keep_path_count_history y'
wotan_opts_rel_poly = '-rr_structs_file ' + vtr_path + '/vpr' + '/dumped_rr_structs.txt -nodisp -threads 7 -max_connection_length 2 -keep_path_count_history n -use_routing_node_demand 0.85'


wotan_opts = wotan_opts_normal

#test_type = wt.Test_Type.normal
test_type = 'binary_search_pessimistic_prob'
plot_index = 5			#into into labels_Rel/regex_Rel -- this value will be plotted on a graph

############ Test Suites ############
#fcout sweep with wilton and input equivalence
test_fcout_inequiv_1 = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=True,
			output_equiv=False,
			switchblock='wilton',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcout_inequiv_2 = wt.Wotan_Test_Suite(
			wirelength=2,
			input_equiv=True,
			output_equiv=False,
			switchblock='wilton',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcout_inequiv_3 = wt.Wotan_Test_Suite(
			wirelength=4,
			input_equiv=True,
			output_equiv=False,
			switchblock='wilton',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)

fcout_inequiv_suites = [test_fcout_inequiv_1, test_fcout_inequiv_2, test_fcout_inequiv_3]


#fcin sweep with wilton and input equivalence
test_fcin_inequiv_1 = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=True,
			output_equiv=False,
			switchblock='wilton',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcin_inequiv_2 = wt.Wotan_Test_Suite(
			wirelength=2,
			input_equiv=True,
			output_equiv=False,
			switchblock='wilton',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcin_inequiv_3 = wt.Wotan_Test_Suite(
			wirelength=4,
			input_equiv=True,
			output_equiv=False,
			switchblock='wilton',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
fcin_inequiv_suites = [test_fcin_inequiv_1, test_fcin_inequiv_2, test_fcin_inequiv_3]



#fcin sweep with different switchblocks at len1
test_fcin_inequiv_wilton = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=True,
			output_equiv=False,
			switchblock='wilton',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string='sb'
			)
test_fcin_inequiv_universal = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=True,
			output_equiv=False,
			switchblock='universal',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string='sb'
			)
test_fcin_inequiv_subset = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=True,
			output_equiv=False,
			switchblock='subset',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string='sb'
			)
fcin_switchblocks_suites = [test_fcin_inequiv_wilton, test_fcin_inequiv_universal, test_fcin_inequiv_subset]


#fcin sweep with wilton and no pin equivalence
test_fcin_noequiv_1 = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=True,
			output_equiv=False,
			switchblock='wilton',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcin_noequiv_2 = wt.Wotan_Test_Suite(
			wirelength=2,
			input_equiv=True,
			output_equiv=False,
			switchblock='wilton',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcin_noequiv_3 = wt.Wotan_Test_Suite(
			wirelength=4,
			input_equiv=False,
			output_equiv=False,
			switchblock='wilton',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
fcin_noequiv_suites = [test_fcin_noequiv_1, test_fcin_noequiv_2, test_fcin_noequiv_3]


##################################################


#fcout sweep with universal and input equivalence
test_fcout_universal_inequiv_1 = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=True,
			output_equiv=False,
			switchblock='universal',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcout_universal_inequiv_2 = wt.Wotan_Test_Suite(
			wirelength=2,
			input_equiv=True,
			output_equiv=False,
			switchblock='universal',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcout_universal_inequiv_3 = wt.Wotan_Test_Suite(
			wirelength=4,
			input_equiv=True,
			output_equiv=False,
			switchblock='universal',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)

fcout_inequiv_universal_suites = [test_fcout_universal_inequiv_1, test_fcout_universal_inequiv_2, test_fcout_universal_inequiv_3]


#fcin sweep with universal and input equivalence
test_fcin_universal_inequiv_1 = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=True,
			output_equiv=False,
			switchblock='universal',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcin_universal_inequiv_2 = wt.Wotan_Test_Suite(
			wirelength=2,
			input_equiv=True,
			output_equiv=False,
			switchblock='universal',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcin_universal_inequiv_3 = wt.Wotan_Test_Suite(
			wirelength=4,
			input_equiv=True,
			output_equiv=False,
			switchblock='universal',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
fcin_inequiv_universal_suites = [test_fcin_universal_inequiv_1, test_fcin_universal_inequiv_2, test_fcin_universal_inequiv_3]


#fcout sweep with subset and input equivalence
test_fcout_subset_inequiv_1 = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=True,
			output_equiv=False,
			switchblock='subset',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcout_subset_inequiv_2 = wt.Wotan_Test_Suite(
			wirelength=2,
			input_equiv=True,
			output_equiv=False,
			switchblock='subset',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcout_subset_inequiv_3 = wt.Wotan_Test_Suite(
			wirelength=4,
			input_equiv=True,
			output_equiv=False,
			switchblock='subset',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)

fcout_inequiv_subset_suites = [test_fcout_subset_inequiv_1, test_fcout_subset_inequiv_2, test_fcout_subset_inequiv_3]


#fcin sweep with subset and input equivalence
test_fcin_subset_inequiv_1 = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=True,
			output_equiv=False,
			switchblock='subset',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcin_subset_inequiv_2 = wt.Wotan_Test_Suite(
			wirelength=2,
			input_equiv=True,
			output_equiv=False,
			switchblock='subset',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcin_subset_inequiv_3 = wt.Wotan_Test_Suite(
			wirelength=4,
			input_equiv=True,
			output_equiv=False,
			switchblock='subset',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
fcin_inequiv_subset_suites = [test_fcin_subset_inequiv_1, test_fcin_subset_inequiv_2, test_fcin_subset_inequiv_3]



####################################################################################


#fcout sweep with wilton and input equivalence
test_fcout_outequiv_1 = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=False,
			output_equiv=True,
			switchblock='wilton',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcout_outequiv_2 = wt.Wotan_Test_Suite(
			wirelength=2,
			input_equiv=False,
			output_equiv=True,
			switchblock='wilton',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcout_outequiv_3 = wt.Wotan_Test_Suite(
			wirelength=4,
			input_equiv=False,
			output_equiv=True,
			switchblock='wilton',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)

fcout_outequiv_suites = [test_fcout_outequiv_1, test_fcout_outequiv_2, test_fcout_outequiv_3]


#fcin sweep with wilton and input equivalence
test_fcin_outequiv_1 = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=False,
			output_equiv=True,
			switchblock='wilton',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcin_outequiv_2 = wt.Wotan_Test_Suite(
			wirelength=2,
			input_equiv=False,
			output_equiv=True,
			switchblock='wilton',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcin_outequiv_3 = wt.Wotan_Test_Suite(
			wirelength=4,
			input_equiv=False,
			output_equiv=True,
			switchblock='wilton',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
fcin_outequiv_suites = [test_fcin_outequiv_1, test_fcin_outequiv_2, test_fcin_outequiv_3]


#fcout sweep with universal and input equivalence
test_fcout_universal_outequiv_1 = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=False,
			output_equiv=True,
			switchblock='universal',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcout_universal_outequiv_2 = wt.Wotan_Test_Suite(
			wirelength=2,
			input_equiv=False,
			output_equiv=True,
			switchblock='universal',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcout_universal_outequiv_3 = wt.Wotan_Test_Suite(
			wirelength=4,
			input_equiv=False,
			output_equiv=True,
			switchblock='universal',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)

fcout_outequiv_universal_suites = [test_fcout_universal_outequiv_1, test_fcout_universal_outequiv_2, test_fcout_universal_outequiv_3]


#fcin sweep with universal and input equivalence
test_fcin_universal_outequiv_1 = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=False,
			output_equiv=True,
			switchblock='universal',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcin_universal_outequiv_2 = wt.Wotan_Test_Suite(
			wirelength=2,
			input_equiv=False,
			output_equiv=True,
			switchblock='universal',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcin_universal_outequiv_3 = wt.Wotan_Test_Suite(
			wirelength=4,
			input_equiv=False,
			output_equiv=True,
			switchblock='universal',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
fcin_outequiv_universal_suites = [test_fcin_universal_outequiv_1, test_fcin_universal_outequiv_2, test_fcin_universal_outequiv_3]


#fcout sweep with subset and input equivalence
test_fcout_subset_outequiv_1 = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=False,
			output_equiv=True,
			switchblock='subset',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcout_subset_outequiv_2 = wt.Wotan_Test_Suite(
			wirelength=2,
			input_equiv=False,
			output_equiv=True,
			switchblock='subset',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcout_subset_outequiv_3 = wt.Wotan_Test_Suite(
			wirelength=4,
			input_equiv=False,
			output_equiv=True,
			switchblock='subset',
			sweep_type='fcout',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)

fcout_outequiv_subset_suites = [test_fcout_subset_outequiv_1, test_fcout_subset_outequiv_2, test_fcout_subset_outequiv_3]


#fcin sweep with subset and input equivalence
test_fcin_subset_outequiv_1 = wt.Wotan_Test_Suite(
			wirelength=1,
			input_equiv=False,
			output_equiv=True,
			switchblock='subset',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcin_subset_outequiv_2 = wt.Wotan_Test_Suite(
			wirelength=2,
			input_equiv=False,
			output_equiv=True,
			switchblock='subset',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
test_fcin_subset_outequiv_3 = wt.Wotan_Test_Suite(
			wirelength=4,
			input_equiv=False,
			output_equiv=True,
			switchblock='subset',
			sweep_type='fcin',
			sweep_range=np.arange(0.05, 0.95, 0.1).tolist(),
			output_regex_list=regex_Rel,
			output_label_list=labels_Rel,
			plot_index=plot_index,
			wotan_opts=wotan_opts,
			vpr_opts=vpr_opts,
			extra_string=''
			)
fcin_outequiv_subset_suites = [test_fcin_subset_outequiv_1, test_fcin_subset_outequiv_2, test_fcin_subset_outequiv_3]


############ Compile The Test Suites ############
test_suites = [fcin_inequiv_suites, fcout_inequiv_suites, fcout_inequiv_universal_suites, fcin_inequiv_universal_suites, fcout_inequiv_subset_suites, fcin_inequiv_subset_suites, 
               fcin_outequiv_suites, fcout_outequiv_suites, fcout_outequiv_universal_suites, fcin_outequiv_universal_suites, fcout_outequiv_subset_suites, fcin_outequiv_subset_suites]    #fcin_switchblocks_suites] #, fcin_noequiv_suites]




############ Run Tests ############
start_time = time.time()

tester = wt.Wotan_Tester(
		vtr_path = vtr_path,
		wotan_path = wotan_path,
		wotan_arch = wotan_arch,
		vpr_arch = vpr_arch,
		test_suite_2dlist = test_suites,
		test_type = test_type
		)

#tester.run_all_tests_sequentially()

results_file = '/home/oleg/Documents/work/UofT/Grad/my_vtr/wotan/python/pair_test.txt'
arch_pairs_list = tester.make_random_arch_pairs_list(60)
#arch_pairs_list = wt.my_custom_arch_pair_list()
tester.run_architecture_comparisons(arch_pairs_list, results_file, wotan_opts, vpr_opts,
                                    compare_against_VPR=True)

end_time = time.time()
print('Done. Took ' + str(round(end_time - start_time, 3)) + ' seconds')

