import subprocess
import os
import sys
import time
import datetime
import re
import random
import multiprocessing
from enum import Enum
import numpy as np
import matplotlib.pyplot as plt
from plotly.graph_objs import *



###### Enums ######
#TODO: need to replace Enum with something python-compatible (a tuple of strings?). Otherwise burst.eecg can't run it.
#Represents how the test should be run for each architecture point.

e_Test_Type = ('normal',
		'binary_search_norm_demand',
		'binary_search_total_prob',
		'binary_search_pessimistic_prob'
		)

#class e_Test_Type(Enum):
#	normal = 0				#just a regular run
#	binary_search_norm_demand = 1		#adjust pin demand until normalized node demands hits some target value
#	binary_search_total_prob = 2		#adjust pin demand until total probability hits some target value
#	binary_search_pessimistic_prob = 3	#adjust pin demand until pessimistic probability hits some target value




###### Classes ######

#represents a suite of Wotan tests to be run
class Wotan_Test_Suite:
	def __init__(self, wirelength,		#wirelength to test (number)
	             input_equiv, 		#should CLB have input equivalence
		     output_equiv, 		#should CLB have output equivalence
		     switchblock,		#which switchblock to test (wilton/universal/subset)
		     sweep_type,		#what value will be swept for this test (fcin/fcout(
		     sweep_range,		#what range should the value be swept over? (this should be a list/tuple)
		     output_regex_list,		#list of regex lines that will be used to parse output (one output per regex)
		     output_label_list,		#list of readable labels to associate with each regex above (for annotating outputs)
		     plot_index,		#index into above two lists. which output should be plotted agains the variable swept in the arch file?
		     wotan_opts,		#a string of wotan options to be used for this test suite
		     vpr_opts,			#a string of vpr options to be used for this test suite (i.e. to set channel width, architecture, etc)
		     extra_string=''):		#a short string that will be prefixed to the string descriptor of this suite. used to help name results file
		

		self.wirelength = wirelength
		self.input_equiv = input_equiv
		self.output_equiv = output_equiv
		
		if switchblock != 'wilton' and switchblock != 'universal' and switchblock != 'subset':
			print('unrecognized switchblock: ' + switchblock)
			sys.exit()
		self.switchblock = switchblock

		if sweep_type != 'fcin' and sweep_type != 'fcout':
			print('unrecognized sweep type: ' + sweep_type)
			sys.exit()
		self.sweep_type = sweep_type

		self.sweep_range = sweep_range

		if len(output_regex_list) == 0 or len(output_regex_list) != len(output_label_list):
			print('regex list should be of same length as the label list, and neither list should have a length of 0')
			sys.exit()
		self.output_regex_list = output_regex_list
		self.output_label_list = output_label_list

		if plot_index >= len(output_regex_list):
			print('plot_index is outside the range of the regex list')
			sys.exit()
		self.plot_index = plot_index

		self.wotan_opts = wotan_opts
		self.vpr_opts = vpr_opts

		self.extra_descriptor_str = extra_string


	#returns a brief string specifying wirelength -- useful for annotating graphs and naming files
	def wirelength_str(self):
		result = 'len' + str(self.wirelength)
		return result

	#returns a brief string specifying input/output equivalence -- useful for annotating graphs and naming files
	def equiv_str(self):
		result = ''
		if self.input_equiv and self.output_equiv:
			result = 'inout-equiv'
		elif self.input_equiv:
			result = 'in-equiv'
		elif self.output_equiv:
			result = 'out-equiv'
		else:
			result = 'no-equiv'
		return result

	#returns a brief string specifying switchblock -- useful for annotating graphs and naming files
	def switchblock_str(self):
		result = ''
		if self.switchblock == 'subset':
			result = 'planar'
		else:
			result = self.switchblock
		return result

	#returns a brief string specifying the sweep type -- useful for annotating graphs and naming files
	def sweep_type_str(self):
		return self.sweep_type

	#returns a string describing the entire test suite -- useful for naming files and stuff
	def as_str(self, separator='_'):
		test_str = ''
		if self.extra_descriptor_str:
			test_str += self.extra_descriptor_str + separator
		test_str += self.wirelength_str() + separator + self.switchblock_str() + separator + self.equiv_str() + separator + self.sweep_type_str()
		return test_str

	#return string to describe a specific attribute
	def get_attr_as_string(self, attr):
		return_str = ''
		if attr == 'wirelength':
			return_str = self.wirelength_str()
		elif attr == 'input_equiv' or attr == 'output_equiv':
			return_str = self.equiv_str()
		elif attr == 'switchblock':
			return_str = self.switchblock_str()
		elif attr == 'sweep_type':
			return_str = self.sweep_type_str()
		else:
			print('unrecognized attribute: ' + attr)
			sys.exit()
		return return_str


	#returns a string describing the specified list of attributes
	def attributes_as_string(self, attribute_list, separator='_'):
		return_str = ''
		#return_str += self.wirelength_str()

		return_str += self.get_attr_as_string(attribute_list[0])
		for attr in attribute_list[1:]:
			return_str += separator + self.get_attr_as_string(attr)
		
		return return_str


	def __str__(self):
		return self.as_str()
	def __repr__(self):
		return self.as_str()


#a class used to run wotan tests.
#I have also modified it to be able to run VPR tests (hence why two different architectures are passed in to the constructor)
class Wotan_Tester:

	#constructor
	def __init__(self, vtr_path,		#path to the base vtr folder
	             wotan_path, 		#path to the base wotan folder
		     wotan_arch,		#name of XML architecture over which WOTAN tests will be run. it is assumed this architecture will be in vtr's timing archs folder
		     vpr_arch,			#name of XML architecture over which VPR tests will be run
		     test_type,			#string specifying test type (holds one of the values in e_Test_Type
		     test_suite_2dlist):	#a list of lists. each sublist contains a set of test suites which should be plotted on the same graph

		#initialize wotan-related stuff
		self.wotan_path = wotan_path

		#initialize vtr-related stuff
		self.vtr_path = vtr_path
		self.vpr_path = vtr_path + "/vpr"
		self.wotan_arch = wotan_arch
		self.wotan_arch_path = vtr_path + "/vtr_flow/arch/timing/" + wotan_arch
		self.vpr_arch = vpr_arch
		self.vpr_arch_path = vtr_path + "/vtr_flow/arch/timing/" + vpr_arch

		print('\n')
		print('Wotan Path: ' + self.wotan_path)
		print('VPR Path: ' + self.vpr_path)
		print('Wotan Architecture: ' + self.wotan_arch_path)
		print('VPR Architecture: ' + self.vpr_arch_path + '\n\n')

		#test suite stuff
		if len(test_suite_2dlist) == 0:
			print('expected test suite to have one or more tests')
			sys.exit()
		self.test_suite_2dlist = test_suite_2dlist

		if test_type not in e_Test_Type:
			print('unrecognized test type: ' + test_type)
			sys.exit()
		self.test_type = test_type

		#get a list of every single architecture point in the test suites.
		#each entry in the list contains indices into appropriate test_suite_2dlist entries
		self.arch_point_list = self.make_arch_point_list_from_test_suites()	#from self.test_suite_2d_list


	############ Command-Line Related ############
	#parses the specified string and returns a list of arguments where each
	#space-delimited value receives its own entry in the list
	def get_argument_list(self, string):
		result = string.split()
		return result

	#runs command with specified arguments and returns the result
	#arguments is a list where each individual argument is in it's own entry
	#i.e. the -l and -a in "ls -l -a" would each have their own entry in the list
	def run_command(self, command, arguments):
		result = subprocess.check_output([command] + arguments)
		return result


	############ VPR Related ############
	#compiles VPR
	def make_vpr(self):
		os.chdir( self.vpr_path )
		result = self.run_command("make", [])
		return result

	#runs VPR with specified arguments
	def run_vpr(self, arguments):
		arg_list = self.get_argument_list(arguments)

		#switch to vpr directory
		os.chdir( self.vpr_path )
		output = str(self.run_command("./vpr", arg_list))
		return output

	#replaces CLB Fc_in and Fc_out in the architecture file according to specified parameters
	#count specifies how many instances of the fcin/fcout line to replace. If 0 or omitted, replace all.
	def replace_arch_fc(self, arch_path, fc_in, fc_out, count=0):
		clb_regex = '.*pb_type name="clb".*'
		fc_regex = 'fc default_in_type="frac" default_in_val="\d+\.\d*" default_out_type="frac" default_out_val="\d+\.\d*"'
		new_line = 'fc default_in_type="frac" default_in_val="' + str(fc_in) + '" default_out_type="frac" default_out_val="' + str(fc_out) + '"'

		if count == 1:
			#use negative lookahead to replace the first 'fc def...' line that occurs after the clb pb type specification
			lookahead_regex = '(?!' + clb_regex + ')'
			line_regex = fc_regex + lookahead_regex
		else:
			#if we want to replace multiple instances of the pattern then it doesn't make sense to use lookahead
			line_regex = fc_regex

		replace_pattern_in_file(line_regex, new_line, arch_path, count=count)


	#replaces wirelength in the architecture with the specified length. switch population is full
	def replace_arch_wirelength(self, arch_path, new_wirelength):
		#regex for old lines
		segment_regex = '<segment freq="\d\.\d*" length="\d+" type="unidir" Rmetal="101" Cmetal="22.5e-15">'
		sb_type_regex = '<sb type="pattern">[1,\s]+</sb>'
		cb_type_regex = '<cb type="pattern">[1,\s]+</cb>'

		#create regex for new lines
		new_sb_pattern = ('1 ' * (new_wirelength+1)).strip()
		new_cb_pattern = ('1 ' * new_wirelength).strip()

		new_segment = '<segment freq="1.0" length="' + str(new_wirelength) + '" type="unidir" Rmetal="101" Cmetal="22.5e-15">'
		new_sb_type = '<sb type="pattern">' + new_sb_pattern + '</sb>'
		new_cb_type = '<cb type="pattern">' + new_cb_pattern + '</cb>'

		#do regex replacements
		replace_pattern_in_file(segment_regex, new_segment, arch_path, count=1)
		replace_pattern_in_file(sb_type_regex, new_sb_type, arch_path, count=1)
		replace_pattern_in_file(cb_type_regex, new_cb_type, arch_path, count=1)


	#replaces switch block in the architecture
	def replace_arch_switchblock(self, arch_path, new_switchblock):
		if new_switchblock != 'subset' and new_switchblock != 'universal' and new_switchblock != 'wilton':
			print('unrecognized switch block: ' + str(new_switchblock))
			sys.exit()

		#regex for old switch block
		switchblock_regex = '<switch_block type="\w+" fs="3"/>'

		#new switch block line
		new_switchblock_line = '<switch_block type="' + new_switchblock + '" fs="3"/>'

		replace_pattern_in_file(switchblock_regex, new_switchblock_line, arch_path, count=1)

	#replaces pin equivalence in architecture
	def replace_arch_pin_equiv(self, arch_path, new_input_equiv, new_output_equiv, num_ipins=40, num_opins=20):
		input_equiv_str = ''
		if new_input_equiv == True:
			input_equiv_str = 'true'
		elif new_input_equiv == False:
			input_equiv_str = 'false'
		else:
			print('Expected True/False for input equiv')
			sys.exit()

		output_equiv_str = ''
		if new_output_equiv == True:
			output_equiv_str = 'true'
		elif new_output_equiv == False:
			output_equiv_str = 'false'
		else:
			print('Expected True/False for output equiv')
			sys.exit()

		#will use negative lookahead to replace the first input/output equiv lines that occur after the clb pb type specification
		clb_regex = '.*pb_type name="clb".*'

		input_equiv_regex = '<input name="I" num_pins="\d+" equivalent="\w+"/>'
		output_equiv_regex = '<output name="O" num_pins="\d+" equivalent="\w+"/>'
		input_equiv_regex += '(?!' + clb_regex + ')'
		output_equiv_regex += '(?!' + clb_regex + ')'

		#new equiv lines
		input_equiv_line = '<input name="I" num_pins="' + str(num_ipins) + '" equivalent="' + input_equiv_str + '"/>'
		output_equiv_line = '<output name="O" num_pins="' + str(num_opins) + '" equivalent="' + output_equiv_str + '"/>'

		replace_pattern_in_file(input_equiv_regex, input_equiv_line, arch_path, count=1)
		replace_pattern_in_file(output_equiv_regex, output_equiv_line, arch_path, count=1)


	#returns list of MCNC benchmarks
	def get_mcnc_benchmarks(self):
		#benchmark names
		benchmarks = [
			'alu4',
			'apex2',
			'apex4',
			'bigkey',
			'clma',
			'des',
			'diffeq',
			'dsip',
			'elliptic',
			'ex1010',
			'ex5p',
			'frisc',
			'misex3',
			'pdc',
			's298',
			's38417',
			's38584.1',
			'seq',
			'spla',
			'tseng'
		]

		#add blif suffix
		benchmarks = [bm + '.pre-vpr.blif' for bm in benchmarks]

		#add full path as prefix
		bm_path = self.vtr_path + '/vtr_flow/benchmarks/blif/wiremap6/'
		benchmarks = [bm_path + bm for bm in benchmarks]

		return benchmarks

	#returns a list of VTR benchmarks
	def get_vtr_benchmarks(self):
		#list of benchmark names
		benchmarks = [
			#'bgm',
			'blob_merge',
			'boundtop',
			'LU8PEEng',
			'mkDelayWorker32B',
			'mkSMAdapter4B',
			'or1200',
			'raygentop',
			'sha',
			'stereovision0',
			'stereovision1'
		]

		#add blif suffix
		benchmarks = [bm + '.blif' for bm in benchmarks]

		#add full path as prefix
		bm_path = self.vtr_path + '/vtr_flow/benchmarks/btr_benchmarks_blif/'
		benchmarks = [bm_path + bm for bm in benchmarks]

		return benchmarks


	#runs provided list of benchmarks and returns geomean outputs based on the provided list of regular expressions
	def run_vpr_benchmarks(self, benchmark_list, 
	                       regex_list, vpr_arch, 
			       vpr_base_opts, 
			       num_threads=1):		#number of concurrent VPR executables to run

		#make 2-d list into which results of each benchmark run will go
		outputs = []
		for tmp in regex_list:
			#an entry for each regex
			outputs += [[]]

		#run benchmarks
		self.make_vpr()
		
		#multithread vpr runs
		iterables = []
		for bm in benchmark_list:
			iterables += [(vpr_arch, bm, vpr_base_opts, benchmark_list, regex_list)]

		mp_pool = multiprocessing.Pool(processes=num_threads)
		outputs = mp_pool.starmap(self.run_vpr_benchmark, iterables)
		mp_pool.close()
		mp_pool.join()

		outputs = np.array(outputs)
		

		#return geomean for each column (?) in the outputs table
		geomean_outputs = []
		for regex in regex_list:
			ind = regex_list.index(regex)
			benchmarks_result_list = outputs[:, ind].tolist()
			geomean_outputs += [ get_geomean(benchmarks_result_list) ]

		return geomean_outputs

	#runs specified vpr benchmark and returns regex'd outputs in a list
	def run_vpr_benchmark(self, vpr_arch, benchmark, vpr_base_opts, benchmark_list, regex_list):
		ind = benchmark_list.index(benchmark)
		print('\t\tbenchmark ' + str(ind))
		output_list = []

		vpr_opts = vpr_arch + ' ' + benchmark + ' ' + vpr_base_opts
		vpr_out = self.run_vpr(vpr_opts)

		#parse outputs according to user's regex list
		for regex in regex_list:
			ind = regex_list.index(regex)
			parsed = float( regex_last_token(vpr_out, regex) )
			output_list += [parsed]

		return output_list





	############ Wotan Related ############
	#compiles wotan
	def make_wotan(self):
		os.chdir( self.wotan_path )
		result = self.run_command("make", [])
		return result

	#runs wotan with specified arguments
	def run_wotan(self, arguments):
		arg_list = self.get_argument_list(arguments)

		#switch to wotan directory
		os.chdir( self.wotan_path )
		output = str(self.run_command("./wotan", arg_list))
		return output

	#commenting out -- opin_demand option now available from wotan's command line
	#replaces 'opin_probability' in wotan's 'wotan_types.cxx' file with the specified value
	#def replace_wotan_pin_demand(self, new_pin_demand):
	#	target_file = self.wotan_path + '/SRC/base/wotan_types.cxx'

	#	if new_pin_demand < 0:
	#		print('requested pin demand < 0')
	#	
	#	opin_probability_regex = '\tthis->opin_probability = \d*\.*\d*;'
	#	new_line = '\tthis->opin_probability = ' + str(new_pin_demand) + ';'

	#	#replace the target line
	#	self.replace_pattern_in_file( opin_probability_regex, new_line, target_file )


	#performs binary search to adjust pin demand in wotan until the target metric is equal to the desired value within some tolerance.
	#returns 3-tuple: (final target value, final pin demand, wotan output)
	def search_for_wotan_pin_demand(self, wotan_opts, test_type,
				target = None, 
				target_tolerance = None,
				target_regex = None,
				pin_demand_low = 0.01,
				pin_demand_high = 5.0,
				max_tries = 15):
		
		if '-opin_demand' in wotan_opts:
			print('-pin_demand option already included in wotan_opts -- can\'t do binary search for pin demand')
			sys.exit()
		
		#true if increasing, false if decreasing
		monotonic_increasing = True
		#what we're searching for in wotan output
		if test_type == 'binary_search_norm_demand':
			if not target_regex:
				target_regex = '.*Normalized demand: (\d+\.\d+).*'
			if not target:
				target = 0.8
			if not target_tolerance:
				target_tolerance = 0.01 
		elif test_type == 'binary_search_pessimistic_prob':
			if not target_regex:
				target_regex = '.*Pessimistic prob: (\d+\.*\d*).*'
			if not target:
				target = 30
			if not target_tolerance:
				target_tolerance = 0.2
			monotonic_increasing = False
		else:
			print('unexpected test_type passed-in to binary search: ' + test_type)
			sys.exit()


		current = 0
		wotan_out = ''

		#perform binary search
		self.make_wotan()
		try_num = 1
		while abs(current - target) > target_tolerance:
			if try_num > max_tries:
				if current < target:
					#the architecture is probably very unroutable and it simply can't get its routability
					#up to 'target' (this tends to happen with some planar out-equiv architectures)
					break
				else:
					print('has taken more than ' + str(max_tries) + ' tries to binary search for correct pin demand. terminating...')
					sys.exit()

			#get next value of pin demand to try
			pin_demand_current = (pin_demand_high + pin_demand_low) / 2

			adjusted_wotan_opts = wotan_opts + ' -opin_demand ' + str(pin_demand_current)

			#commenting out -- can now pass in opin demand to wotan from command line
			#self.replace_wotan_pin_demand(pin_demand_current)

			#run wotan and get the value of the target metric
			self.make_wotan()
			wotan_out = self.run_wotan(adjusted_wotan_opts)
			current = float( regex_last_token(wotan_out, target_regex) )

			if monotonic_increasing:
				if current < target:
					pin_demand_low = pin_demand_current
				else:
					pin_demand_high = pin_demand_current
			else:
				if current > target:
					pin_demand_low = pin_demand_current
				else:
					pin_demand_high = pin_demand_current


			print( '\tat pin demand ' + str(pin_demand_current) + ' current val is ' + str(current) )
			
			if pin_demand_low > pin_demand_high:
				print('low value > high value in binary search!')
				sys.exit()

			try_num += 1

		return (current, pin_demand_current, wotan_out)




	############ Test Suite Related ############
	
	#replaces parameters in VPR architecture file according to the given test suite and an index
	#into the test suite's 'sweep_range' list
	def update_arch_based_on_test_suite(self, arch_path, test_suite, sweep_val, fcs_to_replace='clb'):
		#update VPR arch file based on temporary Arch_Point_Info object
		arch_point_info = Arch_Point_Info.from_wotan_test_suite( test_suite, test_suite.sweep_range.index(sweep_val) )
		self.update_arch_based_on_arch_point(arch_path, arch_point_info, fcs_to_replace)
		
	#replaces parameters in VPR architecture file according to the given Arch_Point_Info object.
	def update_arch_based_on_arch_point(self, arch_path, arch_point_info, fcs_to_replace='clb'):
		self.replace_arch_wirelength( arch_path, arch_point_info.wirelength )
		self.replace_arch_pin_equiv( arch_path, arch_point_info.input_equiv, arch_point_info.output_equiv )
		self.replace_arch_switchblock( arch_path, arch_point_info.switchblock )

		count = 0
		if fcs_to_replace == "clb":
			count = 1
		elif fcs_to_replace == "all":
			count = 0	#will replace all occurances of the fcin/fcout lines
		else:
			print('Unrecognized string specifying how many fcin/fcouts to replace: ' + str(fcs_to_replace))
			sys.exit()

		self.replace_arch_fc(arch_path, arch_point_info.fcin, arch_point_info.fcout, count=count)



	#runs all test suites one by one. plots results for each group of tests on the same graph.
	#writes out results of each individual test to its own file
	def run_all_tests_sequentially(self):
		start_time = time.time()

		#create folder in which to stick results
		results_folder = self.wotan_path + '/python/wotan_tester_run_' + get_date_str('-')
		os.mkdir(results_folder)

		run = 1
		for test_suite_list in self.test_suite_2dlist:
			print('run ' + str(run) + ' out of ' + str(len(self.test_suite_2dlist)))


			############ Setup ############

			#a list of attributes that is different between this list of test suites
			differing_attributes = test_suite_differing_attributes( test_suite_list )
			
			#generate trace labels for each test suite. also check that all test suites sweep same variable & plot same variable & sweep range is the same
			trace_labels = []
			sweep_variable = test_suite_list[0].sweep_type
			sweep_range = test_suite_list[0].sweep_range
			plot_index = test_suite_list[0].plot_index
			for suite in test_suite_list:
				trace_labels += [ suite.attributes_as_string( differing_attributes ) ]
				if suite.sweep_type != sweep_variable:
					print('test suites whose results should be plotted on a single graph should be sweeping the same variable')
					sys.exit()
				if suite.sweep_range != sweep_range:
					print('test suites whole results should be plotted on a single graph should be sweeping the same range')
					sys.exit()
				if suite.plot_index != plot_index:
					print('test suites whose results should be plotted on a single graph should be plotting the same variable')
					sys.exit()
			

			############ Run Tests ############
			results = []
			for suite in test_suite_list:
				results.append( self.run_test_suite(suite, results_folder) )	#run_test_suite automatically writes data to files



			############ Plot/Save Graph ############

			#plot test suite results on the same graph
			results_graph = test_suite_list[0].as_str() + '.png'
			plot_x_index = 0
			plot_y_index = plot_index + 1				#+1 because I prepended a column for the swept variable
			plot_x_label = results[0][0][plot_x_index]
			plot_y_label = results[0][0][plot_y_index]
			plot_title = plot_y_label + ' vs ' + plot_x_label
			plot_subtitle = test_suite_list[0].as_str(', ')

			fig = plt.figure()
			fig.suptitle(plot_title + '\n' + plot_subtitle, fontsize=18)
			plt.xlabel(plot_x_label, fontsize=15)
			plt.ylabel(plot_y_label, fontsize=15)

			#plot each trace
			isuite = 0
			while isuite < len(test_suite_list):
				plot_x_data = [ row[plot_x_index] for row in results[isuite][1:] ]		#get column, skipping label at the top
				plot_y_data = [ row[plot_y_index] for row in results[isuite][1:] ]
				plt.plot(plot_x_data, plot_y_data, label=trace_labels[isuite])
				#print(trace_labels[isuite])
				isuite += 1

			#show trace labels
			plt.legend(loc='best')
			#save the figure 
			fig.savefig(results_folder + '/' + test_suite_list[0].as_str() + '.png')	#TODO: naming multi-suite graph after one suite?? any better way?
			#plt.show()

			run += 1

		print('Tests completed. Took ' + str(time.time() - start_time) + ' seconds') 


	#runs the specified test suite and writes results to a file
	def run_test_suite(self, test_suite, results_directory):
		#make wotan and vpr
		self.make_wotan()
		self.make_vpr()

		#setup a results structure -- a list of lists
		#set up the labels first
		results = []
		results.append([])
		results[0] += [ test_suite.sweep_type ]		#this is the swept variable (i.e. fcin/fcout) at which wotan outputs are tested and recorded
		for label in test_suite.output_label_list:
			results[0] += [ label ]


		#run tests
		run = 1
		for metric in test_suite.sweep_range:
			results.append([])

			print('\trun ' + str(run))

			#update architecture file
			self.update_arch_based_on_test_suite(self.wotan_arch_path, test_suite, metric)

			#rebuild vpr graph
			self.run_vpr( test_suite.vpr_opts )

			#run wotan
			wotan_output = None
			pin_demand = None
			if self.test_type == 'normal':
				wotan_output = self.run_wotan( test_suite.wotan_opts )
			elif 'binary_search_' in self.test_type:
				(tmp, pin_demand, wotan_output) = self.search_for_wotan_pin_demand(test_suite.wotan_opts, self.test_type)
				print('\t\tpin demand: ' + str(pin_demand))
			else:
				print('unrecognized test type: ' + self.test_type)
				sys.exit()

			#parse outputs
			results[run] += [ str(metric) ]
			for regex in test_suite.output_regex_list:
				regexed_result = regex_last_token(wotan_output, regex)
				if self.test_type == 'binary_search_norm_demand' and regex == test_suite.output_regex_list[ test_suite.plot_index ]:
					#normalize result by pin demand
					print('\t\tprobability: ' + regexed_result)
					regexed_result = str( float(regexed_result) * pin_demand )
					print('\t\tadjusted probability: ' + regexed_result)

					#TODO: make a separate column for the plot result; modify it if doing some kind of binary search
					#TODO: in tester script, automatically choose plot_index based on the test type (i.e. binary_search_prob should be looking at pin demand)

					#TODO: I am testing at same-W architecture. Fundamentally, this is different from what VPR does. Data to 'estimate' channel width
					#	would be kind of nice
				results[run] += [ regexed_result ]

			run += 1

		#write results
		results_file = results_directory + '/' + test_suite.as_str() + '.txt'
		#results_file = self.wotan_path + '/python/' + test_suite.as_str() + '.txt'
		#print(results)
		self.write_test_results_to_file(results, results_file)

		return results


	#writes out the input 2d array to the specified file path
	def write_test_results_to_file(self, test_results_2d, file_path):
		#print(file_path)
		with open(file_path, 'w+') as f:
			for sublist in test_results_2d:
				for item in sublist:
					#print(str(item) + '\t')
					f.write(str(item) + '\t')
				f.write('\n')



	#Creates a list of Arch_Point_Info objects based on each architecture point in each test suite.
	def make_arch_point_list_from_test_suites(self):
		result_list = []

		#tests are organized into groups (results for each group are plotted on the same chart).
		#so traverse all groups, then tests in each group, then arch points in each test
		for test_group in self.test_suite_2dlist:
			for test in test_group:
				for ind in test.sweep_range:
					arch_ind = test.sweep_range.index(ind)
					arch_point = Arch_Point_Info.from_wotan_test_suite(test, arch_ind)

					result_list += [arch_point]

		return result_list

	
	#returns a 2-tuple containing two different arch points from the arch point list
	def get_random_arch_point_pair(self):
		num_arch_points = len(self.arch_point_list)
		first_point_ind = random.randint(0, num_arch_points-1)
		second_point_ind = first_point_ind

		while second_point_ind == first_point_ind:
			second_point_ind = random.randint(0, num_arch_points-1)

		#lower point comes first
		if (first_point_ind < second_point_ind):
			first_arch_point = self.arch_point_list[ first_point_ind ]
			second_arch_point = self.arch_point_list[ second_point_ind ]
		else:
			first_arch_point = self.arch_point_list[ second_point_ind ]
			second_arch_point = self.arch_point_list[ first_point_ind ]

		return (first_arch_point, second_arch_point)
		
	
	#returns a list where every entry is a 2-tuple of different architecture points. each entry in the list will be unique
	def make_random_arch_pairs_list(self, num_pairs):
		
		arch_pairs_list = []
		i = 0
		while i < num_pairs:
			arch_pair = self.get_random_arch_point_pair()
			#don't want duplicates
			while arch_pair in arch_pairs_list:
				arch_pair = self.get_random_arch_point_pair()
			arch_pairs_list += [arch_pair]
			i += 1

		return arch_pairs_list


	#compares the architectures of each arch pair in the passed-in list against each other
	#and writes a file with the results
	def run_architecture_comparisons(self, arch_pairs_list, 
	                                 results_file, 
					 wotan_opts, 
					 vpr_opts_for_wotan, 
	                                 compare_against_VPR=False):	#if enabled, a VPR comparison will also be run for each architecture pair (to verify Wotan metrics)


		if self.test_type != 'binary_search_pessimistic_prob':
			print('Unexpected test type: ' + str(self.test_type))
			sys.exit()

		#would like to increase pin demand until ONE of the architectures hits the below probability.
		#at that point the arch with the lower probability will be considered more routable
		target_prob = 0.3
		target_tolerance = 0.02
		target_regex = '.*Pessimistic prob: (\d+\.*\d*).*'	#TODO: should make this a function arg?

		#holds a list of results for each arch pair comparison
		result_table = []
		#run arch pair comparisons -- 
		for arch_pair in arch_pairs_list:
			if len(arch_pair) != 2:
				print('expected two entries in arch pair, got ' + str(len(arch_pair)))
				sys.exit()
			

			############ Compare the architecture pair using Wotan ############
			print(str(arch_pair[0]) + ' VS ' + str(arch_pair[1]))

			#get pin demand / routability based on first architecture as reference
			(arch0_metric, arch1_metric) = self.wotan_arch_metrics_with_first_as_reference(arch_pair[0], arch_pair[1], target_prob,
										       target_tolerance, target_regex, wotan_opts, vpr_opts_for_wotan)

			#if second architecture has a lower metric, then rerun above test with pin demand based on second arch as reference
			if (arch1_metric < arch0_metric):
				(arch1_metric, arch0_metric) = self.wotan_arch_metrics_with_first_as_reference(arch_pair[1], arch_pair[0], target_prob,
											       target_tolerance, target_regex, wotan_opts, vpr_opts_for_wotan)

				if (arch0_metric < arch1_metric):
					print('WARNING: arch1 was worse with arch0 as reference. but now arch0 is worse when arch1 was chosen as reference? inconsistency??')
					#sys.exit()
			
			#which architecture won according to Wotan?
			wotan_winner = 1
			if arch0_metric < arch1_metric:
				wotan_winner = 2
			
			#add results of this comparison to table
			comparison_result = [str(arch_pair[0]), str(arch0_metric), str(arch1_metric), str(arch_pair[1]), str(wotan_winner)]

		
			############ Compare the architecture pair using VPR ############
			if compare_against_VPR:
				vpr_regex_list = ['channel width factor of (\d+)']
				benchmarks = self.get_mcnc_benchmarks()
				vpr_base_opts = '-nodisp'

				#Run the architecture comparison using VPR
				vpr_results = []
				for arch in arch_pair:
					ind = arch_pair.index(arch)
					self.update_arch_based_on_arch_point(self.vpr_arch_path, arch_pair[ind], fcs_to_replace='all' )

					results = self.run_vpr_benchmarks(benchmarks, vpr_regex_list, self.vpr_arch_path, vpr_base_opts, num_threads=7)
					vpr_results += [results[0]]

				#Which architecture won according to VPR? Does that match with Wotan's prediction?
				vpr_winner = 1
				if vpr_results[0] > vpr_results[1]:
					vpr_winner = 2

				wotan_correct = True
				if wotan_winner != vpr_winner:
					wotan_correct = False
				
				comparison_result += [str(round(vpr_results[0], 2)), str(round(vpr_results[1],2)), str(wotan_correct)]


			#Write comparison results as entries in a table
			result_table += [comparison_result]
			print('\t' + str(comparison_result))


		#comparisons done -- print results to file
		with open(results_file, 'w+') as f:
			for arch_pair_result in result_table:
				for result_entry in arch_pair_result:
					f.write(result_entry + '\t')
				f.write('\n')

		#fin



	#returns a 2-tuple containing a metric (based on target_regex) for each of the specified architectures.
	#pin demand is calculated based on the first architecture (and the target/target_tolerance values) using a binary search.
	#metric values for the two architectures are returned based on the aforementioned pin demand 
	def wotan_arch_metrics_with_first_as_reference(self, arch_point1, arch_point2, target, target_tolerance, target_regex,
	                                               wotan_opts, vpr_opts):
	
		arch1_metric = None
		arch2_metric = None

		#get pin demand / routability based on first architecture
		self.update_arch_based_on_arch_point(self.wotan_arch_path, arch_point1)
		self.run_vpr( vpr_opts )
		(arch1_metric, pin_demand, arch1_output) = self.search_for_wotan_pin_demand(wotan_opts = wotan_opts,
											  test_type = self.test_type,
											  target = target,
											  target_tolerance = target_tolerance,
											  target_regex = target_regex)

		#now get the routability of the second architecture
		self.update_arch_based_on_arch_point(self.wotan_arch_path, arch_point2)
		self.run_vpr( vpr_opts )
		arch2_output = self.run_wotan( wotan_opts + ' -opin_demand ' + str(pin_demand) )
		arch2_metric = float( regex_last_token(arch2_output, target_regex) )

		print (arch1_metric, arch2_metric)
		return (arch1_metric, arch2_metric)


#Contains info about an architecture data point. Basically mirrors
# the info contained in Wotan_Test_Suite, except for only one architecture point
class Arch_Point_Info:

	def __init__(self, wirelength,		#see comments in Wotan_Test_Suite
	             input_equiv,
		     output_equiv,
		     switchblock,
		     fcin,
		     fcout):
	
		self.wirelength = wirelength
		self.input_equiv = input_equiv
		self.output_equiv = output_equiv
		self.switchblock = switchblock
		self.fcin = fcin
		self.fcout = fcout

	#overload constructor -- initialize based on Wotan_Test_Suite object
	@classmethod 
	def from_wotan_test_suite(cls, wotan_test_suite, 	#the Wotan_Test_Suite object
	                          sweep_var_index, 		#index into wotan_test_suites 'sweep_range' list
				  fcin_default=0.15, 		#default fcin
				  fcout_default=0.1):		#default fcout ('sweep_var_index' will only chance fcin OR fcout -- the other will remain as specified here)

		wirelength = wotan_test_suite.wirelength
		input_equiv = wotan_test_suite.input_equiv
		output_equiv = wotan_test_suite.output_equiv
		switchblock = wotan_test_suite.switchblock

		fcin = fcin_default
		fcout = fcout_default
		sweep_val = wotan_test_suite.sweep_range[ sweep_var_index ]
		if wotan_test_suite.sweep_type == 'fcin':
			fcin = sweep_val
		else:
			fcout = sweep_val

		return cls(wirelength, input_equiv, output_equiv, switchblock, fcin, fcout)


	#overload constructor -- initialize based on a string. Expecting string to be in the format of this class's 'as_str' function
	@classmethod
	def from_str(cls, s, separator='_'):
		regex_list = (
			'.*len(\d).*',			#wirelength
			'.*fcin(\d+\.*\d*)',		#fcin
			'.*fcout(\d+\.*\d*)'		#fcout
		)

		#get wirelength, fcin, fcout
		tmp_list = []
		for regex in regex_list:
			tmp_list += [regex_last_token(s, regex)]
		
		wirelength = int(tmp_list[0])
		fcin = float(tmp_list[1])
		fcout = float(tmp_list[2])

		#get switchblock
		switchblock = None
		if 'planar' in s:
			switchblock = 'subset'
		elif 'universal' in s:
			switchblock = 'universal'
		elif 'wilton' in s:
			switchblock = 'wilton'
		else:
			print('could not find a switchblock specification in string:\n\t' + s)
			sys.exit()

		#get input equivalence
		input_equiv = False
		if 'in-eq' in s:
			input_equiv = True

		#get output equivalence
		output_equiv = False
		if 'out-eq' in s:
			output_equiv = True

		return cls(wirelength, input_equiv, output_equiv, switchblock, fcin, fcout)
			
		
	#returns a string describing an object of this class
	def as_str(self, separator='_'):
		result = self.wirelength_str() + separator
		if self.inequiv_str() != '':
			result += self.inequiv_str() + separator
		if self.outequiv_str() != '':
			result += self.outequiv_str() + separator
		result += self.switchblock_str() + separator + self.fcin_str() + separator + self.fcout_str()
		return result

	#returns a brief string specifying wirelength
	def wirelength_str(self):
		result = 'len' + str(self.wirelength)
		return result

	#returns brief string specifying input equivalence
	def inequiv_str(self):
		result = ''
		if self.input_equiv:
			result = 'in-eq'
		return result

	#returns brief string specifying output equivalence
	def outequiv_str(self):
		result = ''
		if self.output_equiv:
			result = 'out-eq'
		return result

	#returns a brief string specifying switchblock
	def switchblock_str(self):
		result = ''
		if self.switchblock == 'subset':
			result = 'planar'
		else:
			result = self.switchblock
		return result

	#returns a brief string specifying fcin
	def fcin_str(self):
		result = 'fcin' + str(round(self.fcin,3))
		return result

	#returns a brief string specifying fcout
	def fcout_str(self):
		result = 'fcout' + str(round(self.fcout,3))
		return result


	def __str__(self):
		return self.as_str()
	def __repr__(self):
		return self.as_str()




############ Regex Related ############
#replaces strings in specified file according to regex
#if the count variable is omitted or 0 all lines will be replaced. otherwise count
#specifies the maximum number of pattern occurences to be replaced
def replace_pattern_in_file(line_regex, new_line, file_path, count=0):
	#read the old file into a string
	fid = open(file_path, 'r')
	file_string = fid.read()
	fid.close()

	#replace specified line in the string
	new_file_string = re.sub(line_regex, new_line, file_string, count=count, flags=re.DOTALL)   #DOTALL necessary for '.' character to match newline as well

	#commenting because new file may be intended to be the same as the old file...
	#if file_string == new_file_string:
	#    print("Couldn't regex file: " + file_path)
	#    sys.exit()

	#writeback the file
	fid = open(file_path, 'w')
	fid.write(new_file_string)
	fid.close()


#parses specified string and returns the last value to match pattern
def regex_last_token(string, pattern):
	result = re.findall(pattern, string, flags=re.DOTALL);

	if not result:
		print("could not regex pattern " + pattern)
		print("string is:")
		print(string)
		sys.exit()

	#return the last value to match patter
	return result[-1]




############ Miscellaneous ############
#compares the test suites to each other, and returns a list of 
#attribute strings that are different
def test_suite_differing_attributes(test_suite_list):
	differing_list = []

	compare_suite = test_suite_list[0]

	for suite in test_suite_list[1:]:
		#iterate over each attribute
		for attr, value in suite.__dict__.iteritems():
			#compare attribute value to that of 'compare_suite'
			if getattr(suite, attr) != getattr(compare_suite, attr):
				#make sure this attribute hasn't already been recorded
				if attr in differing_list:
					continue
				#pin equivalence attributes are a bit special...
				if attr == 'input_equiv' and 'output_equiv' in differing_list:
					continue
				elif attr == 'output_equiv' and 'input_equiv' in differing_list:
					continue
				differing_list += [attr]

	return differing_list


#returns date string in year/month/day/hour/minute. the various entries are separated by specified separator string
def get_date_str(separator='-'):
	now = datetime.datetime.now()
	date_str = str(now.year) + separator + str(now.month) + separator + str(now.day) + separator + str(now.hour) + separator + str(now.minute)
	return date_str


#returns geometric mean of list
def get_geomean(my_list):
	result = 1.0
	for num in my_list:
		result *= num
	result **= (1.0/len(my_list))
	return result


#returns a hard-coded list of Arch_Point_Info pairs
def my_custom_arch_pair_list():

	#format: 'len1_in-eq_out-eq_wilton_fcin0.25_fcout_0.5'

	arch_pairs = []
	string_pairs = []

	############ Easy ############
	string_pairs += [['len1_in-eq_wilton_fcin0.45_fcout0.1', 'len1_in-eq_wilton_fcin0.15_fcout0.1']]
	string_pairs += [['len4_in-eq_wilton_fcin0.5_fcout0.1', 'len2_in-eq_wilton_fcin0.5_fcout0.1']]

	############ Moderate ############


	############ Hard ############
	

	#build a list of arch pairs from the list of string pairs
	for string_pair in string_pairs:
		arch_point0 = Arch_Point_Info.from_str(string_pair[0])
		arch_point1 = Arch_Point_Info.from_str(string_pair[1])
		arch_pairs += [[arch_point0, arch_point1]]

	return arch_pairs


## evenly sampled time at 200ms intervals
#t = np.arange(0., 5., 0.2)
#
## red dashes, blue squares and green triangles
#fig = plt.figure()
#plt.plot(t, t, t, t**2, t, t**3)
#fig.suptitle('test title\nsubtitle', fontsize=18)
##fig.savefig('./test1.png')
#
#fig = plt.figure()
#plt.plot(t, t**4, label='gar')
#plt.plot(t, t**5, label='par')
#plt.legend()
#fig.suptitle('another graph')
##fig.savefig('./test2.png')
#plt.show()
#plt.show()
