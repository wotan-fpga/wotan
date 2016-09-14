
import subprocess
import os
import sys
import time
import datetime
import re
import random
import multiprocessing
#from enum import Enum
import numpy as np
import matplotlib.pyplot as plt
import copy_reg
import types

from my_regex import *
import arch_handler as ah

###### Enums ######
e_Test_Type = ('normal',
		'binary_search_norm_demand',
		'binary_search_routability_metric'
		)



#redifine the pickling function used by 'pickle' (in turn used by multiprocessing) s.t.
#multiprocessing will work with instancemethods (i.e. class functions).
#this is necessary for python2 compatibility. python3 solves all ills but burst.eecg
#doesn't support it fully yet
def _pickle_method(m):
	if m.im_self is None:
		return getattr, (m.im_class, m.im_func.func_name)
	else:
		return getattr, (m.im_self, m.im_func.func_name)
copy_reg.pickle(types.MethodType, _pickle_method)



###### Classes ######

#represents a suite of Wotan tests to be run
class Wotan_Test_Suite:
	def __init__(self, wirelength,		#wirelength to test (number)
		     switchblock,		#which switchblock to test (wilton/universal/subset)
		     arch_name,			#a string specifying the arch to use. should correspond to an entry in 'arch_dictionaries' variable
		     arch_dictionaries,		#object of type 'Archs' that contains dictionaries of possible archs for use in Wotan and VPR tests
		     sweep_type,		#what value will be swept for this test (fcin/fcout(
		     sweep_range,		#what range should the value be swept over? (this should be a list/tuple)
		     output_regex_list,		#list of regex lines that will be used to parse output (one output per regex)
		     output_label_list,		#list of readable labels to associate with each regex above (for annotating outputs)
		     plot_index,		#index into above two lists. which output should be plotted agains the variable swept in the arch file?
		     wotan_opts,		#a string of wotan options to be used for this test suite
		     extra_string=''):		#a short string that will be prefixed to the string descriptor of this suite. used to help name results file
		

		self.wirelength = wirelength
		
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

		self.extra_descriptor_str = extra_string

		if not isinstance(arch_dictionaries, Archs):
			print('expected arch_dictionaries to be of type Archs')
			sys.exit()
		self.arch_dictionaries = arch_dictionaries
		self.arch_name = arch_name


	#returns path to VPR architecture file used for Wotan tests
	def get_wotan_arch_path(self):
		return self.arch_dictionaries.wotan_archs[ self.arch_name ]

	#returns path to VPR architecture file used for VPR tests
	def get_vpr_arch_path(self):
		return self.arch_dictionaries.vpr_archs[ self.arch_name ]


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

	#returns a brief string specifying the arch file that was used
	def arch_name_str(self):
		result = 'arch:' + self.arch_name
		return result


	#returns a string describing the entire test suite -- useful for naming files and stuff
	def as_str(self, separator='_'):
		test_str = ''
		if self.extra_descriptor_str:
			test_str += self.extra_descriptor_str + separator
		test_str += self.wirelength_str() + separator + self.switchblock_str() + separator + \
				self.sweep_type_str() + separator + self.arch_name_str()
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
		elif attr == 'arch_name':
			return_str = self.arch_name_str()
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
		     test_type,			#string specifying test type (holds one of the values in e_Test_Type)
		     test_suite_2dlist):	#a list of lists. each sublist contains a set of test suites which should be plotted on the same graph

		#initialize wotan-related stuff
		self.wotan_path = wotan_path

		#initialize vtr-related stuff
		self.vtr_path = vtr_path
		self.vpr_path = vtr_path + "/vpr"

		print('\n')
		print('Wotan Path: ' + self.wotan_path)
		print('VPR Path: ' + self.vpr_path)

		if test_type not in e_Test_Type:
			print('unrecognized test type: ' + test_type)
			sys.exit()
		self.test_type = test_type


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
		#benchmarks = [bm + '.pre-vpr.blif' for bm in benchmarks]
		benchmarks = [bm + '.blif' for bm in benchmarks]

		#add full path as prefix
		#bm_path = self.vtr_path + '/vtr_flow/benchmarks/blif/wiremap6/'
		bm_path = self.vtr_path + '/vtr_flow/benchmarks/blif/'
		benchmarks = [bm_path + bm for bm in benchmarks]

		return benchmarks

	#returns a list of VTR benchmarks
	def get_vtr_benchmarks(self, lut_size=6):
		#list of benchmark names
		benchmarks = [
			'bgm',
			'blob_merge',
			'boundtop',
			'mkDelayWorker32B',
			'LU8PEEng',
			'mcml',
			'stereovision2',
			'LU32PEEng',
			'mkSMAdapter4B',
			'or1200',
			'raygentop',
			'sha',
			'stereovision0',
			'stereovision1'
		]

		suffix = '.blif'
		bm_dir = '/vtr_flow/benchmarks/vtr_benchmarks_blif/'
		if lut_size == 4:
			suffix = '.pre-vpr.blif'
			bm_dir = '/vtr_flow/benchmarks/4LUT_DSP_vtr_benchmarks_blif/'

		#add blif suffix
		benchmarks = [bm + suffix for bm in benchmarks]

		#add full path as prefix
		bm_path = self.vtr_path + bm_dir
		benchmarks = [bm_path + bm for bm in benchmarks]

		return benchmarks


	#runs provided list of benchmarks and returns outputs (based on regex_list) averaged over the specified list of seeds.
	#this function basically calls 'run_vpr_benchmarks' for each seed in the list
	def run_vpr_benchmarks_multiple_seeds(self, benchmark_list,
	                                      regex_list, vpr_arch,
					      vpr_seed_list = [1],	#by default run with single seed
					      num_threads = 1):		#by default run with 1 thread
		result_table = []

		if num_threads > len(benchmark_list):
			num_threads = len(benchmark_list)

		#run vpr benchmarks for each specified seed
		for seed in vpr_seed_list:
			print('SEED ' + str(seed) + ' of ' + str(len(vpr_seed_list)))
			seed_result = self.run_vpr_benchmarks(benchmark_list, regex_list, vpr_arch,
			                                      seed, num_threads = num_threads)

			result_table += [seed_result]

		#take average of all results
		result_table = np.array(result_table)

		avg_results = []
		for column in result_table.T:
			column_avg = sum(column) / float(len(column))
			avg_results += [column_avg]

		return avg_results


	#runs provided list of benchmarks and returns geomean outputs based on the provided list of regular expressions
	def run_vpr_benchmarks(self, benchmark_list, 
	                       regex_list, arch_path, 
			       vpr_seed = 1,			#default seed of 1
			       num_threads = 1):		#number of concurrent VPR executables to run

		#VPR should be run with the -nodisp option and some seed
		vpr_base_opts = '-nodisp -timing_analysis off --seed ' + str(vpr_seed)

		#make 2-d list into which results of each benchmark run will go
		outputs = []
		for tmp in regex_list:
			#an entry for each regex
			outputs += [[]]

		#self.change_vpr_rr_struct_dump(self.vpr_path, enable=False)
		self.make_vpr()

		#create a temporary directory for each benchmark to store the vpr executable (otherwise many threads try to write to the same vpr output file)
		temp_dir = self.vpr_path + '/script_temp'
		#cleanup temp directory if it already exists
		if os.path.isdir(temp_dir):
			self.run_command('rm', ['-r', temp_dir])

		try:
			self.run_command('mkdir', [temp_dir])
		except subprocess.CalledProcessError as err:
			print(err.output)
			raise 

		#multithread vpr runs
		iterables = []
		for bm in benchmark_list:
			index = benchmark_list.index(bm)
			bm_dir = temp_dir + '/bm' + str(index)
			try:
				self.run_command('mkdir', [bm_dir])
				self.run_command('cp', [arch_path, bm_dir])
				self.run_command('cp', [self.vpr_path + '/vpr', bm_dir])
			except subprocess.CalledProcessError as err:
				print(err.output)
				raise

			arch_name = (arch_path.split('/'))[-1]
			if 'xml' not in arch_name:
				raise 'WTF'
			new_arch_path = bm_dir + '/' + arch_name
			new_vpr_path = bm_dir + '/vpr'

			iterables += [VPR_Benchmark_Info(new_vpr_path, new_arch_path, bm, vpr_base_opts, benchmark_list, regex_list)]

		os.system("taskset -p 0xffffffff %d" % os.getpid())
		mp_pool = multiprocessing.Pool(processes=num_threads)
		try:
			outputs = mp_pool.map(self.run_vpr_benchmark, iterables)
			mp_pool.close()
			mp_pool.join()
		except KeyboardInterrupt:
			print('Caught KeyboardInterrupt. Terminating threads and exiting.')
			mp_pool.terminate()
			mp_pool.join()
			sys.exit()

		outputs = np.array(outputs)
		

		#return geomean for each column (?) in the outputs table
		geomean_outputs = []
		for regex in regex_list:
			ind = regex_list.index(regex)
			benchmarks_result_list = outputs[:, ind].tolist()
			for out in benchmarks_result_list:
				print( '\t\tbenchmark %d routed at W=%d' % (benchmarks_result_list.index(out), int(out)) )
				sys.stdout.flush()

			geomean_outputs += [ get_geomean(benchmarks_result_list) ]


		return geomean_outputs

	#runs specified vpr benchmark and returns regex'd outputs in a list
	def run_vpr_benchmark(self, bm_info):
		vpr_path = bm_info.vpr_path
		arch_path = bm_info.arch_path
		benchmark = bm_info.benchmark
		vpr_base_opts = bm_info.vpr_base_opts
		benchmark_list = bm_info.benchmark_list
		regex_list = bm_info.regex_list

		run_dir = ''
		for token in (arch_path.split('/'))[:-1]:
			run_dir += token + '/'

		os.chdir( run_dir )

		output_list = []
		
		vpr_opts = arch_path + ' ' + benchmark + ' ' + vpr_base_opts
		try:
			arg_list = self.get_argument_list(vpr_opts)
			vpr_out = str(self.run_command(vpr_path, arg_list))
			#vpr_out = self.run_vpr(vpr_opts)
		except KeyboardInterrupt:
			#dealing with python 2.7 compatibility stupidness... i can't get multiprocessing to terminate on "ctrl-c"
			#unless I write this try-except statement. and even then I have to bang on ctrl-c repeatedly to get the desired effect :(
			print('worker received interrupt. exiting.')
			return

		#parse outputs according to user's regex list
		for regex in regex_list:
			#ind = regex_list.index(regex)
			parsed = float( regex_last_token(vpr_out, regex) )
			output_list += [parsed]

		ind = benchmark_list.index(benchmark)
		print('\t\tbenchmark: ' + str(ind) + ' done')
		#print('\t\t\tvpr opts: ' + vpr_opts)
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


	#performs binary search to adjust demand multiplier in wotan until the target metric is equal to the desired value within some tolerance.
	#returns 3-tuple: (final target value, final demand multiplier, wotan output)
	def search_for_wotan_demand_multiplier(self, wotan_opts, test_type,
				target = None, 
				target_tolerance = None,
				target_regex = None,
				demand_mult_low = 0.0,
				demand_mult_high = 10,
				max_tries = 30):
		
		if '-demand_multiplier' in wotan_opts:
			print('-demand_multiplier option already included in wotan_opts -- can\'t do binary search for pin demand')
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
		elif test_type == 'binary_search_routability_metric':
			if not target_regex:
				target_regex = '.*Routability metric: (\d+\.*\d*).*'
			if not target:
				target = 0.3
			if not target_tolerance:
				target_tolerance = 0.02
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
					#the architecture is probably very unroutable and it simply can't get to the specified target value
					print('\t\tarchitecture looks too unroutable; can\'t meet binary search target of ' + str(target) + '. Returning.')
					break
				else:
					print('WARNING! Binary search has taken more than ' + str(max_tries) + ' tries to binary search for correct pin demand. using last value...')
					break
					#sys.exit()

			#get next value of pin demand to try
			demand_mult_current = (demand_mult_high + demand_mult_low) / 2

			adjusted_wotan_opts = wotan_opts + ' -demand_multiplier ' + str(demand_mult_current)

			#run wotan and get the value of the target metric
			self.make_wotan()
			wotan_out = self.run_wotan(adjusted_wotan_opts)
			regex_val = regex_last_token(wotan_out, target_regex)
			current = float( regex_val )

			if monotonic_increasing:
				if current < target:
					demand_mult_low = demand_mult_current
				else:
					demand_mult_high = demand_mult_current
			else:
				if current > target:
					demand_mult_low = demand_mult_current
				else:
					demand_mult_high = demand_mult_current


			print( '\tat demand mult ' + str(demand_mult_current) + ' current val is ' + str(current) )
			sys.stdout.flush()
			
			if demand_mult_low > demand_mult_high:
				print('low value > high value in binary search!')
				sys.exit()

			try_num += 1

		return (current, demand_mult_current, wotan_out)




	############ Test Suite Related ############
	
	#returns a 2-tuple containing a metric (based on target_regex) for each of the specified architectures.
	#pin demand is calculated based on the first architecture (and the target/target_tolerance values) using a binary search.
	#metric values for the two architectures are returned based on the aforementioned pin demand 
	def wotan_arch_metrics_with_first_as_reference(self, arch_point1, arch_point2, target, target_tolerance, target_regex,
	                                               wotan_opts, vpr_opts_arch1, vpr_opts_arch2):
	
		arch1_metric = None
		arch2_metric = None

		path_arch1 = arch_point1.get_wotan_arch_path()
		path_arch2 = arch_point2.get_wotan_arch_path()

		#get pin demand / routability based on first architecture
		self.update_arch_based_on_arch_point(path_arch1, arch_point1)
		self.run_vpr( vpr_opts_arch1 )
		(arch1_metric, demand_mult, arch1_output) = self.search_for_wotan_demand_multiplier(wotan_opts = wotan_opts,
											  test_type = self.test_type,
											  target = target,
											  target_tolerance = target_tolerance,
											  target_regex = target_regex)

		#now get the routability of the second architecture
		self.update_arch_based_on_arch_point(path_arch2, arch_point2)
		self.run_vpr( vpr_opts_arch2 )
		arch2_output = self.run_wotan( wotan_opts + ' -demand_multiplier ' + str(demand_mult) )
		arch2_metric = float( regex_last_token(arch2_output, target_regex) )

		print (arch1_metric, arch2_metric)
		return (arch1_metric, arch2_metric)


	
	#evaluates routability of each architecture point in the specified list (optionally runs VPR on this list as well).
	#results are written in table form to the specified file
	#	wotan results are sorted best to worst
	#	VPR results, if enabled, are sorter best to worst (in terms on channel width)
	def evaluate_architecture_list(self, arch_list,
	                                     results_file,
					     wotan_opts,
					     vpr_arch_ordering = []):	#if not [], wotan results will be compared against this ordering. otherwise vpr comparisons will be run too

		run_vpr_comparisons = False
		if vpr_arch_ordering == []:
			run_vpr_comparisons = True


		#specifies how architectures should be evaluated.
		#binary search over pin demand until target prob is hit with specified tolerance
		target_prob = 0.5
		target_tolerance = 0.0099
		target_regex = '.*Routability metric: (\d+\.*\d*).*'

		#a list of channel widths over which to evaluate w/ wotan (i.e. geomean)
		chan_widths = [100]
		vpr_evaluation_seeds = [1]
		
		wotan_results = []
		vpr_results = []

		#for each architecture point:
		#- evaluate with wotan 
		#- evaluate with VPR if enabled
		for arch_point in arch_list:
			arch_point_index = arch_list.index(arch_point)
			print('Run ' + str(arch_point_index+1) + '/' + str(len(arch_list)) + '    arch is ' + arch_point.as_str() )


			###### Make Wotan and VPR ######
			self.make_wotan()
			self.make_vpr()
	
			#path to architecture
			wotan_arch_path = get_path_to_arch( arch_point )

			###### Evaluate architecture with Wotan ######
			metric_value_list = []
			for chanw in chan_widths:
				print('W = ' + str(chanw))

				#6LUT and 4LUT benchmarks are from different directories
				benchmark = 'vtr_benchmarks_blif/sha.blif'
				if arch_point.lut_size == 4:
					benchmark = '4LUT_DSP_vtr_benchmarks_blif/sha.pre-vpr.blif'

				vpr_opts = wotan_arch_path + ' ' + self.vtr_path + '/vtr_flow/benchmarks/' + benchmark + ' -nodisp -dump_rr_structs_file ./dumped_rr_structs.txt -pack -place -route_chan_width ' + str(chanw)
				vpr_out = self.run_vpr( vpr_opts )

				#run binary search to find pin demand at which the target_regex hits its target value 
				(target_val, demand_mult, wotan_out) = self.search_for_wotan_demand_multiplier(wotan_opts = wotan_opts,
												       test_type = self.test_type,
												       target = target_prob,
												       target_tolerance = target_tolerance,
												       target_regex = target_regex,
												       demand_mult_high = 200)

				#get metric used for evaluating the architecture
				metric_regex = '.*Demand multiplier: (\d*\.*\d+).*'		#TODO: put this value into arch point info based on test suites? don't want to be hard-coding...
				metric_label = 'Demand Multiplier'
				metric_value_list += [float(regex_last_token(wotan_out, metric_regex))]

			#add metric to list of wotan results
			metric_value = get_geomean(metric_value_list)
			print('geomean score: ' + str(metric_value))
			wotan_result_entry = [arch_point_index, arch_point.as_str(), metric_value]
			wotan_results += [wotan_result_entry]


			###### Evaluate architecture with VPR ######
			if run_vpr_comparisons:
				#what regex / benchmarks to run?
				vpr_regex_list = ['channel width factor of (\d+)']
				benchmarks = self.get_vtr_benchmarks(lut_size = arch_point.lut_size)

				#run VPR and get regex results
				#vpr_arch_path = arch_point.get_vpr_arch_path()
				#self.update_arch_based_on_arch_point(vpr_arch_path, arch_point)

				#TODO: each thread should be executing in its own directory. also, update architecture here?
				results = self.run_vpr_benchmarks_multiple_seeds(benchmarks, vpr_regex_list, wotan_arch_path,
										 vpr_seed_list = vpr_evaluation_seeds,
										 num_threads = 10)

				#add VPR result to running list
				vpr_result_entry = [arch_point_index, arch_point.as_str(), results[0]]
				vpr_results += [vpr_result_entry]
	
		
		#sort results -- descending for wotan, ascending for vpr
		wotan_results.sort(key=lambda x: x[2], reverse=True)
		vpr_results.sort(key=lambda x: x[2])

		try:
			#figure out how many pairwise comparisons of wotan agree with VPR
			# --> compare every architecture result to every other architecture result
			agree_cases = 0
			agree_within_tolerance = 0
			total_cases = 0
			vpr_tolerance = 2
			wotan_arch_ordering = [el[1:3] for el in wotan_results]		#get arch string and score for each element in 'wotan_arch_ordering'
			if run_vpr_comparisons:
				vpr_arch_ordering = [el[1:3] for el in vpr_results]

				[agree_cases, agree_within_tolerance, total_cases] = compare_wotan_vpr_arch_orderings(wotan_arch_ordering, vpr_arch_ordering, vpr_tolerance)
			else:
				#compare wotan results against passed-in list
				if len(wotan_arch_ordering) == len(vpr_arch_ordering):
					[agree_cases, agree_within_tolerance, total_cases] = compare_wotan_vpr_arch_orderings(wotan_arch_ordering, vpr_arch_ordering, vpr_tolerance)
		except TypeError as e:
			print('caught exception:')
			print(e)
			print('continuing anyway')

		#print results to a file
		with open(results_file, 'w+') as f:
			for w_result in wotan_results:
				result_index = wotan_results.index(w_result)

				if run_vpr_comparisons:
					v_result = vpr_results[result_index]
				else:
					v_result = []
				
				for w_elem in w_result:
					f.write(str(w_elem) + '\t')
				f.write('\t')
				for v_elem in v_result:
					f.write(str(v_elem) + '\t')
				f.write('\n')

			f.write('\n')
			f.write('Wotan and VPR agree in ' + str(agree_cases) + '/' + str(total_cases) + ' pairwise comparisons\n')
			f.write(str(agree_within_tolerance) + '/' + str(total_cases) + ' cases agree within VPR minW tolerance of ' + str(vpr_tolerance))


#Contains info about an architecture data point. Basically mirrors
# the info contained in Wotan_Test_Suite, except for only one architecture point
class Arch_Point_Info:

	def __init__(self, lut_size,                    #size of the LUT (i.e. K)
	             s_wirelength,			#semi-global wirelength
	             g_wirelength,			#global-layer wirelength; specify None if not used
		     switchblock_pattern,		#wilton/universal/subset
		     wire_topology,			#'single-wirelength', 'on-cb-off-cb', 'on-cb-off-sb', 'on-cb-off-cbsb', 'on-cbsb-off-cbsb', 'on-sb-off-sb'
		     fcin,				#cb input flexibility
		     fcout,				#cb output flexibility
		     arch_string = None):		#optional string that describes this architecture
	

		if lut_size not in [4, 6]:
			raise BaseException, 'Unexpected LUT size: %d' % (lut_size)

		if switchblock_pattern not in ['wilton', 'universal', 'subset']:
			raise BaseException, 'Unexpected switch block pattern: %s' % (switchblock_pattern)

		if wire_topology not in ['single-wirelength', 'on-cb-off-cb', 'on-cb-off-sb', 'on-cb-off-cbsb', 'on-cbsb-off-cbsb', 'on-sb-off-sb']:
			raise BaseException, 'Unexpected wire topology: %s' % (wire_topology)


		self.lut_size = lut_size
		self.s_wirelength = s_wirelength
		self.g_wirelength = g_wirelength
		self.switchblock_pattern = switchblock_pattern
		self.wire_topology = wire_topology
		self.fcin = fcin
		self.fcout = fcout
		self.arch_string = arch_string



	#overload constructor -- initialize based on a string. Expecting string to be in the format of this class' 'as_str' function
	@classmethod
	def from_str(cls, s):
		#this should be a dictionary...
		regex_list = {
			's_wirelength' : '.*_s(\d+)_.*',
			'g_wirelength' : '.*_g(\d+)_.*',
			'K' : '.*k(\d)_.*',
			'wire_topology' : '.*_topology-([-\w]+)_.*',
			'fcin' : '.*fcin(\d+\.*\d*)',
			'fcout' : '.*fcout(\d+\.*\d*)',
		}

		#get wirelength, fcin, fcout
		tmp_dict = {}
		for key in regex_list:
			try:
				tmp_dict[key] = regex_last_token(s, regex_list[key])
			except RegexException as exception:
				if key == 'g_wirelength':
					#it's OK if global wirelength wasn't specified
					tmp_dict[key] = None
					continue
				else:
					raise
		
		s_wirelength = int(tmp_dict['s_wirelength'])
		g_wirelength = tmp_dict['g_wirelength']
		if g_wirelength != None:
			g_wirelength = int(g_wirelength)
		lut_size = int(tmp_dict['K'])
		wire_topology = tmp_dict['wire_topology']
		fcin = float(tmp_dict['fcin'])
		fcout = float(tmp_dict['fcout'])

		#get switchblock
		switchblock = None
		if 'subset' in s:
			switchblock = 'subset'
		elif 'universal' in s:
			switchblock = 'universal'
		elif 'wilton' in s:
			switchblock = 'wilton'
		else:
			print('could not find a switchblock specification in string:\n\t' + s)
			sys.exit()

		return cls(lut_size, s_wirelength, g_wirelength, switchblock, wire_topology, fcin, fcout, s)
			
		
	#returns a string describing an object of this class
	def as_str(self):
		return self.arch_string

	def __str__(self):
		return self.arch_string
	def __repr__(self):
		return self.arch_string


#class used for passing info in multithreading VPR benchmark runs 
class VPR_Benchmark_Info():
	def __init__(self, vpr_path,
	             arch_path,
	             benchmark,
		     vpr_base_opts,
		     benchmark_list,
		     regex_list
                     ):
		
		self.vpr_path = vpr_path
		self.arch_path = arch_path
		self.benchmark = benchmark
		self.vpr_base_opts = vpr_base_opts
		self.benchmark_list = benchmark_list
		self.regex_list = regex_list
		
		


############ Miscellaneous ############
#reads each line in a file into a list of strings
def read_file_into_string_list(file_path):
	string_list = []
	with open(file_path) as f:
		string_list = f.readlines()
	
	#remove leading/trailing whitespace from strings
	for string in string_list:
		ind = string_list.index(string)
		string_list[ind] = string_list[ind].strip()
	
	return string_list

#reads each line in a file into a 2d list of strings. each string is split into a list of strings with spaces/tabs as delimiters
def read_file_into_split_string_list(file_path):
	split_string_list = []
	with open(file_path) as f:
		split_string_list = f.readlines()

	#now remove leading/trailing whitespace and split strings
	for string in split_string_list:
		ind = split_string_list.index(string)
		split_string_list[ind] = split_string_list[ind].strip().split()

	return split_string_list

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


#returns (x,y) index of 'val' in 'my_list' 
def index_2d(my_list, val):
	result_x = None
	result_y = None
	
	for sublist in my_list:
		if val in sublist:
			result_x = my_list.index(sublist)
			result_y = sublist.index(val)
			break

	return (result_x, result_y)


#returns the number of pairwise comparisons where wotan ordering agrees with vpr ordering.
#basically match every architecture against every other architecture for wotan, and then see if this pairwise odering
#agrees with VPR.
# - assumed that architectures are ordered best to worst. first entry is architecture name, second entry is architecture 'score' (min W for VPR)
def compare_wotan_vpr_arch_orderings(wotan_ordering, vpr_ordering, 
                                     vpr_tolerance=2):			#Wotan predictions always treated as correct for architectures within specified VPR score tolerance

	#make sure both ordered lists are the same size
	if len(wotan_ordering) != len(vpr_ordering):
		print('expected wotan and vpr ordered list to be the same size')
		sys.exit()

	total_cases = 0
	agree_cases = 0
	agree_within_tolerance = 0
	
	i = 0
	while i < len(wotan_ordering)-1:
		j = i+1
		while j < len(wotan_ordering):
			arch_one = wotan_ordering[i][0]
			arch_two = wotan_ordering[j][0]

			#now get the index of these two arch points in the vpr ordered list. since the lists are sorted from best to worst,
			#a lower index means a better architecture
			vpr_ind_one, dummy = index_2d(vpr_ordering, arch_one)
			vpr_ind_two, dummy = index_2d(vpr_ordering, arch_two)

			vpr_score_one = float(vpr_ordering[ vpr_ind_one ][1])
			vpr_score_two = float(vpr_ordering[ vpr_ind_two ][1])

			if vpr_ind_one < vpr_ind_two:
				agree_cases += 1
				agree_within_tolerance += 1
			elif abs(vpr_score_one - vpr_score_two) <= vpr_tolerance:
				agree_within_tolerance += 1
			else:
				print('Disagreed with VPR ordering:\t' + vpr_ordering[vpr_ind_one][0] + ' (' + str(vpr_score_one) + ') VS ' + vpr_ordering[vpr_ind_two][0] + ' (' + str(vpr_score_two) + ')')

			total_cases += 1
			j += 1

		i += 1

	return (agree_cases, agree_within_tolerance, total_cases)


#returns a hard-coded list of Arch_Point_Info elements (use my_custom_arch_pair_list for pairwise comparisons)
def my_custom_archs_list():

	arch_list = []
	arch_strings = []

	#Legend (corresponds to arch_handler.py):
	#k<LUT_size>   s<semi-global segment length>    g<global segment length>    <switchblock (universal/subset/wilton)>
        #                  topology-<interconect topology>       fcin<input Fc>     fcout<output Fc>


	#### 100 random 6LUT architectures ####
	#arch_strings += ['k6_s4_g8_universal_topology-on-cb-off-cb_fcin0.2_fcout0.05']
	#arch_strings += ['k6_s4_wilton_topology-single-wirelength_fcin0.05_fcout0.05']
	#arch_strings += ['k6_s2_wilton_topology-single-wirelength_fcin0.05_fcout0.2']
	#arch_strings += ['k6_s2_subset_topology-single-wirelength_fcin0.2_fcout0.1']
	#arch_strings += ['k6_s4_g16_universal_topology-on-cb-off-cb_fcin0.1_fcout0.1']
	#arch_strings += ['k6_s8_wilton_topology-single-wirelength_fcin0.1_fcout0.4']
	#arch_strings += ['k6_s4_universal_topology-single-wirelength_fcin0.1_fcout0.05']
	#arch_strings += ['k6_s4_universal_topology-single-wirelength_fcin0.6_fcout0.4']
	#arch_strings += ['k6_s4_g8_subset_topology-on-sb-off-sb_fcin0.4_fcout0.05']
	#arch_strings += ['k6_s4_g8_wilton_topology-on-sb-off-sb_fcin0.4_fcout0.6']
	#arch_strings += ['k6_s4_wilton_topology-single-wirelength_fcin0.05_fcout0.4']
	#arch_strings += ['k6_s4_g16_wilton_topology-on-cbsb-off-cbsb_fcin0.6_fcout0.6']
	#arch_strings += ['k6_s1_universal_topology-single-wirelength_fcin0.1_fcout0.6']
	#arch_strings += ['k6_s4_g8_universal_topology-on-cb-off-sb_fcin0.1_fcout0.05']
	#arch_strings += ['k6_s4_wilton_topology-single-wirelength_fcin0.4_fcout0.1']
	#arch_strings += ['k6_s16_subset_topology-single-wirelength_fcin0.1_fcout0.6']
	#arch_strings += ['k6_s8_universal_topology-single-wirelength_fcin0.4_fcout0.1']
	#arch_strings += ['k6_s4_g4_wilton_topology-on-cb-off-sb_fcin0.4_fcout0.6']
	#arch_strings += ['k6_s4_g4_subset_topology-on-cbsb-off-cbsb_fcin0.1_fcout0.1']
	#arch_strings += ['k6_s16_wilton_topology-single-wirelength_fcin0.1_fcout0.1']
	#arch_strings += ['k6_s4_g8_universal_topology-on-cbsb-off-cbsb_fcin0.1_fcout0.6']
	#arch_strings += ['k6_s8_universal_topology-single-wirelength_fcin0.6_fcout0.2']
	#arch_strings += ['k6_s2_universal_topology-single-wirelength_fcin0.1_fcout0.2']
	#arch_strings += ['k6_s8_wilton_topology-single-wirelength_fcin0.4_fcout0.1']
	#arch_strings += ['k6_s2_universal_topology-single-wirelength_fcin0.6_fcout0.6']
	#arch_strings += ['k6_s8_universal_topology-single-wirelength_fcin0.6_fcout0.1']
	#arch_strings += ['k6_s1_subset_topology-single-wirelength_fcin0.2_fcout0.4']
	#arch_strings += ['k6_s4_g16_subset_topology-on-cbsb-off-cbsb_fcin0.1_fcout0.05']
	#arch_strings += ['k6_s4_g16_universal_topology-on-sb-off-sb_fcin0.05_fcout0.4']
	#arch_strings += ['k6_s4_g8_wilton_topology-on-cb-off-sb_fcin0.2_fcout0.05']
	#arch_strings += ['k6_s4_g4_universal_topology-on-cb-off-cb_fcin0.4_fcout0.05']
	#arch_strings += ['k6_s4_wilton_topology-single-wirelength_fcin0.2_fcout0.2']
	#arch_strings += ['k6_s1_subset_topology-single-wirelength_fcin0.2_fcout0.6']
	#arch_strings += ['k6_s2_wilton_topology-single-wirelength_fcin0.2_fcout0.05']
	#arch_strings += ['k6_s4_universal_topology-single-wirelength_fcin0.2_fcout0.6']
	#arch_strings += ['k6_s2_subset_topology-single-wirelength_fcin0.05_fcout0.1']
	#arch_strings += ['k6_s8_wilton_topology-single-wirelength_fcin0.05_fcout0.2']
	#arch_strings += ['k6_s1_universal_topology-single-wirelength_fcin0.4_fcout0.6']
	#arch_strings += ['k6_s4_g8_wilton_topology-on-cbsb-off-cbsb_fcin0.6_fcout0.4']
	#arch_strings += ['k6_s4_g16_universal_topology-on-cbsb-off-cbsb_fcin0.1_fcout0.6']
	#arch_strings += ['k6_s4_subset_topology-single-wirelength_fcin0.6_fcout0.6']
	#arch_strings += ['k6_s4_g4_wilton_topology-on-cb-off-cb_fcin0.2_fcout0.6']
	#arch_strings += ['k6_s8_subset_topology-single-wirelength_fcin0.05_fcout0.2']
	#arch_strings += ['k6_s16_subset_topology-single-wirelength_fcin0.2_fcout0.2']
	#arch_strings += ['k6_s16_wilton_topology-single-wirelength_fcin0.1_fcout0.6']
	#arch_strings += ['k6_s2_universal_topology-single-wirelength_fcin0.05_fcout0.4']
	#arch_strings += ['k6_s2_universal_topology-single-wirelength_fcin0.2_fcout0.4']
	#arch_strings += ['k6_s4_g16_universal_topology-on-cb-off-cb_fcin0.1_fcout0.4']
	#arch_strings += ['k6_s4_g4_universal_topology-on-cbsb-off-cbsb_fcin0.05_fcout0.6']
	#arch_strings += ['k6_s1_subset_topology-single-wirelength_fcin0.4_fcout0.1']
	#arch_strings += ['k6_s1_wilton_topology-single-wirelength_fcin0.05_fcout0.4']
	#arch_strings += ['k6_s4_g8_subset_topology-on-cb-off-cbsb_fcin0.1_fcout0.2']
	#arch_strings += ['k6_s4_g4_universal_topology-on-cb-off-cbsb_fcin0.4_fcout0.6']
	#arch_strings += ['k6_s4_g16_universal_topology-on-cbsb-off-cbsb_fcin0.2_fcout0.2']
	#arch_strings += ['k6_s4_g4_wilton_topology-on-cb-off-sb_fcin0.6_fcout0.2']
	#arch_strings += ['k6_s4_wilton_topology-single-wirelength_fcin0.1_fcout0.6']
	#arch_strings += ['k6_s4_universal_topology-single-wirelength_fcin0.6_fcout0.05']
	#arch_strings += ['k6_s4_g4_universal_topology-on-cb-off-cb_fcin0.6_fcout0.2']
	#arch_strings += ['k6_s4_g16_wilton_topology-on-sb-off-sb_fcin0.6_fcout0.05']
	#arch_strings += ['k6_s4_g4_wilton_topology-on-sb-off-sb_fcin0.05_fcout0.05']
	#arch_strings += ['k6_s4_g8_subset_topology-on-cb-off-cbsb_fcin0.2_fcout0.05']
	#arch_strings += ['k6_s2_wilton_topology-single-wirelength_fcin0.4_fcout0.2']
	#arch_strings += ['k6_s2_universal_topology-single-wirelength_fcin0.6_fcout0.05']
	#arch_strings += ['k6_s4_subset_topology-single-wirelength_fcin0.6_fcout0.05']
	#arch_strings += ['k6_s16_wilton_topology-single-wirelength_fcin0.4_fcout0.4']
	#arch_strings += ['k6_s16_subset_topology-single-wirelength_fcin0.2_fcout0.4']
	#arch_strings += ['k6_s4_g4_subset_topology-on-cb-off-sb_fcin0.2_fcout0.1']
	#arch_strings += ['k6_s16_universal_topology-single-wirelength_fcin0.05_fcout0.05']
	#arch_strings += ['k6_s8_wilton_topology-single-wirelength_fcin0.05_fcout0.4']
	#arch_strings += ['k6_s4_g4_universal_topology-on-sb-off-sb_fcin0.4_fcout0.05']
	#arch_strings += ['k6_s4_g16_subset_topology-on-cb-off-cb_fcin0.4_fcout0.05']
	#arch_strings += ['k6_s4_g4_universal_topology-on-cb-off-cbsb_fcin0.1_fcout0.4']
	#arch_strings += ['k6_s8_subset_topology-single-wirelength_fcin0.2_fcout0.05']
	#arch_strings += ['k6_s4_g8_universal_topology-on-cb-off-cb_fcin0.6_fcout0.2']
	#arch_strings += ['k6_s4_g16_wilton_topology-on-cb-off-cbsb_fcin0.05_fcout0.2']
	#arch_strings += ['k6_s4_g8_subset_topology-on-sb-off-sb_fcin0.05_fcout0.6']
	#arch_strings += ['k6_s8_wilton_topology-single-wirelength_fcin0.6_fcout0.2']
	#arch_strings += ['k6_s4_g16_wilton_topology-on-cbsb-off-cbsb_fcin0.6_fcout0.2']
	#arch_strings += ['k6_s2_universal_topology-single-wirelength_fcin0.1_fcout0.4']
	#arch_strings += ['k6_s8_wilton_topology-single-wirelength_fcin0.05_fcout0.6']
	#arch_strings += ['k6_s4_g8_subset_topology-on-cb-off-cbsb_fcin0.6_fcout0.05']
	#arch_strings += ['k6_s4_g4_subset_topology-on-sb-off-sb_fcin0.4_fcout0.4']
	#arch_strings += ['k6_s2_universal_topology-single-wirelength_fcin0.2_fcout0.6']
	#arch_strings += ['k6_s2_wilton_topology-single-wirelength_fcin0.1_fcout0.6']
	#arch_strings += ['k6_s2_subset_topology-single-wirelength_fcin0.1_fcout0.05']
	#arch_strings += ['k6_s4_g16_universal_topology-on-cb-off-cbsb_fcin0.2_fcout0.2']
	#arch_strings += ['k6_s4_g8_wilton_topology-on-cb-off-sb_fcin0.6_fcout0.1']
	#arch_strings += ['k6_s4_g16_universal_topology-on-cbsb-off-cbsb_fcin0.05_fcout0.05']
	#arch_strings += ['k6_s4_universal_topology-single-wirelength_fcin0.4_fcout0.2']
	#arch_strings += ['k6_s1_wilton_topology-single-wirelength_fcin0.6_fcout0.05']
	#arch_strings += ['k6_s2_wilton_topology-single-wirelength_fcin0.1_fcout0.05']
	#arch_strings += ['k6_s1_subset_topology-single-wirelength_fcin0.05_fcout0.4']
	#arch_strings += ['k6_s4_g8_universal_topology-on-cbsb-off-cbsb_fcin0.05_fcout0.05']
	#arch_strings += ['k6_s4_subset_topology-single-wirelength_fcin0.4_fcout0.4']
	#arch_strings += ['k6_s4_g16_subset_topology-on-cb-off-cbsb_fcin0.4_fcout0.1']
	#arch_strings += ['k6_s16_universal_topology-single-wirelength_fcin0.1_fcout0.4']
	#arch_strings += ['k6_s1_subset_topology-single-wirelength_fcin0.6_fcout0.4']
	#arch_strings += ['k6_s2_universal_topology-single-wirelength_fcin0.6_fcout0.1']
	#arch_strings += ['k6_s4_g8_subset_topology-on-sb-off-sb_fcin0.6_fcout0.05']
	#arch_strings += ['k6_s1_wilton_topology-single-wirelength_fcin0.2_fcout0.6']

	#### 100 random 4LUT architectures ####
	arch_strings += ['k4_s2_g16_wilton_topology-on-cbsb-off-cbsb_fcin0.2_fcout0.4']
	arch_strings += ['k4_s1_wilton_topology-single-wirelength_fcin0.3_fcout0.2']
	arch_strings += ['k4_s4_subset_topology-single-wirelength_fcin0.4_fcout0.2']
	arch_strings += ['k4_s1_universal_topology-single-wirelength_fcin0.4_fcout0.2']
	arch_strings += ['k4_s2_g8_wilton_topology-on-sb-off-sb_fcin0.1_fcout0.3']
	arch_strings += ['k4_s2_wilton_topology-single-wirelength_fcin0.2_fcout0.1']
	arch_strings += ['k4_s2_g16_subset_topology-on-cbsb-off-cbsb_fcin0.4_fcout0.4']
	arch_strings += ['k4_s2_universal_topology-single-wirelength_fcin0.1_fcout0.1']
	arch_strings += ['k4_s2_g16_subset_topology-on-sb-off-sb_fcin0.1_fcout0.1']
	arch_strings += ['k4_s2_g16_subset_topology-on-cb-off-cbsb_fcin0.2_fcout0.2']
	arch_strings += ['k4_s8_wilton_topology-single-wirelength_fcin0.1_fcout0.1']
	arch_strings += ['k4_s2_g16_universal_topology-on-sb-off-sb_fcin0.1_fcout0.3']
	arch_strings += ['k4_s2_g8_universal_topology-on-sb-off-sb_fcin0.3_fcout0.4']
	arch_strings += ['k4_s8_wilton_topology-single-wirelength_fcin0.6_fcout0.1']
	arch_strings += ['k4_s2_universal_topology-single-wirelength_fcin0.6_fcout0.4']
	arch_strings += ['k4_s2_g16_subset_topology-on-cb-off-cbsb_fcin0.4_fcout0.3']
	arch_strings += ['k4_s4_subset_topology-single-wirelength_fcin0.6_fcout0.6']
	arch_strings += ['k4_s4_universal_topology-single-wirelength_fcin0.6_fcout0.1']
	arch_strings += ['k4_s1_subset_topology-single-wirelength_fcin0.6_fcout0.6']
	arch_strings += ['k4_s2_wilton_topology-single-wirelength_fcin0.6_fcout0.6']
	arch_strings += ['k4_s4_wilton_topology-single-wirelength_fcin0.6_fcout0.4']
	arch_strings += ['k4_s1_wilton_topology-single-wirelength_fcin0.6_fcout0.3']
	arch_strings += ['k4_s2_g4_universal_topology-on-sb-off-sb_fcin0.6_fcout0.6']
	arch_strings += ['k4_s1_universal_topology-single-wirelength_fcin0.6_fcout0.1']
	arch_strings += ['k4_s2_g8_subset_topology-on-cb-off-cbsb_fcin0.1_fcout0.2']
	arch_strings += ['k4_s2_g8_subset_topology-on-cb-off-sb_fcin0.3_fcout0.6']
	arch_strings += ['k4_s2_g4_subset_topology-on-sb-off-sb_fcin0.3_fcout0.6']
	arch_strings += ['k4_s8_universal_topology-single-wirelength_fcin0.3_fcout0.2']
	arch_strings += ['k4_s1_subset_topology-single-wirelength_fcin0.2_fcout0.1']
	arch_strings += ['k4_s2_g4_universal_topology-on-cbsb-off-cbsb_fcin0.1_fcout0.6']
	arch_strings += ['k4_s1_subset_topology-single-wirelength_fcin0.6_fcout0.4']
	arch_strings += ['k4_s1_subset_topology-single-wirelength_fcin0.2_fcout0.3']
	arch_strings += ['k4_s2_g16_subset_topology-on-cb-off-sb_fcin0.3_fcout0.2']
	arch_strings += ['k4_s2_g8_universal_topology-on-cbsb-off-cbsb_fcin0.4_fcout0.2']
	arch_strings += ['k4_s4_wilton_topology-single-wirelength_fcin0.2_fcout0.1']
	arch_strings += ['k4_s2_g4_wilton_topology-on-cb-off-sb_fcin0.3_fcout0.6']
	arch_strings += ['k4_s4_wilton_topology-single-wirelength_fcin0.3_fcout0.6']
	arch_strings += ['k4_s2_universal_topology-single-wirelength_fcin0.3_fcout0.2']
	arch_strings += ['k4_s4_wilton_topology-single-wirelength_fcin0.6_fcout0.3']
	arch_strings += ['k4_s2_subset_topology-single-wirelength_fcin0.1_fcout0.6']
	arch_strings += ['k4_s2_g8_universal_topology-on-cb-off-cb_fcin0.3_fcout0.6']
	arch_strings += ['k4_s4_subset_topology-single-wirelength_fcin0.6_fcout0.3']
	arch_strings += ['k4_s1_wilton_topology-single-wirelength_fcin0.1_fcout0.4']
	arch_strings += ['k4_s8_universal_topology-single-wirelength_fcin0.3_fcout0.4']
	arch_strings += ['k4_s2_subset_topology-single-wirelength_fcin0.2_fcout0.3']
	arch_strings += ['k4_s1_universal_topology-single-wirelength_fcin0.1_fcout0.6']
	arch_strings += ['k4_s2_g4_wilton_topology-on-cb-off-sb_fcin0.6_fcout0.2']
	arch_strings += ['k4_s2_g4_subset_topology-on-sb-off-sb_fcin0.6_fcout0.3']
	arch_strings += ['k4_s4_wilton_topology-single-wirelength_fcin0.4_fcout0.6']
	arch_strings += ['k4_s4_subset_topology-single-wirelength_fcin0.6_fcout0.4']
	arch_strings += ['k4_s2_g16_universal_topology-on-cbsb-off-cbsb_fcin0.3_fcout0.3']
	arch_strings += ['k4_s2_wilton_topology-single-wirelength_fcin0.3_fcout0.1']
	arch_strings += ['k4_s8_subset_topology-single-wirelength_fcin0.3_fcout0.6']
	arch_strings += ['k4_s2_g16_subset_topology-on-sb-off-sb_fcin0.4_fcout0.2']
	arch_strings += ['k4_s2_g16_universal_topology-on-cb-off-cb_fcin0.1_fcout0.2']
	arch_strings += ['k4_s4_wilton_topology-single-wirelength_fcin0.2_fcout0.3']
	arch_strings += ['k4_s2_subset_topology-single-wirelength_fcin0.4_fcout0.2']
	arch_strings += ['k4_s4_universal_topology-single-wirelength_fcin0.4_fcout0.6']
	arch_strings += ['k4_s2_g8_subset_topology-on-cb-off-sb_fcin0.3_fcout0.1']
	arch_strings += ['k4_s2_g4_wilton_topology-on-cb-off-sb_fcin0.1_fcout0.6']
	arch_strings += ['k4_s2_g16_subset_topology-on-cb-off-cb_fcin0.3_fcout0.4']
	arch_strings += ['k4_s2_g8_universal_topology-on-cb-off-cb_fcin0.4_fcout0.2']
	arch_strings += ['k4_s8_wilton_topology-single-wirelength_fcin0.2_fcout0.3']
	arch_strings += ['k4_s2_universal_topology-single-wirelength_fcin0.1_fcout0.3']
	arch_strings += ['k4_s2_g16_universal_topology-on-cb-off-sb_fcin0.1_fcout0.3']
	arch_strings += ['k4_s2_g4_wilton_topology-on-cb-off-cb_fcin0.4_fcout0.6']
	arch_strings += ['k4_s4_wilton_topology-single-wirelength_fcin0.3_fcout0.3']
	arch_strings += ['k4_s2_g4_universal_topology-on-cb-off-sb_fcin0.3_fcout0.3']
	arch_strings += ['k4_s2_g8_universal_topology-on-cb-off-sb_fcin0.6_fcout0.6']
	arch_strings += ['k4_s4_wilton_topology-single-wirelength_fcin0.1_fcout0.6']
	arch_strings += ['k4_s2_g8_universal_topology-on-cbsb-off-cbsb_fcin0.4_fcout0.4']
	arch_strings += ['k4_s8_subset_topology-single-wirelength_fcin0.1_fcout0.2']
	arch_strings += ['k4_s8_subset_topology-single-wirelength_fcin0.6_fcout0.6']
	arch_strings += ['k4_s2_g4_subset_topology-on-sb-off-sb_fcin0.2_fcout0.3']
	arch_strings += ['k4_s2_g16_universal_topology-on-cb-off-sb_fcin0.3_fcout0.3']
	arch_strings += ['k4_s1_universal_topology-single-wirelength_fcin0.6_fcout0.4']
	arch_strings += ['k4_s2_g16_subset_topology-on-cbsb-off-cbsb_fcin0.6_fcout0.6']
	arch_strings += ['k4_s2_g8_wilton_topology-on-cb-off-cb_fcin0.1_fcout0.1']
	arch_strings += ['k4_s2_g8_subset_topology-on-cbsb-off-cbsb_fcin0.3_fcout0.3']
	arch_strings += ['k4_s8_wilton_topology-single-wirelength_fcin0.1_fcout0.4']
	arch_strings += ['k4_s2_g16_universal_topology-on-cb-off-cbsb_fcin0.2_fcout0.4']
	arch_strings += ['k4_s2_g4_subset_topology-on-cb-off-sb_fcin0.1_fcout0.4']
	arch_strings += ['k4_s2_g8_wilton_topology-on-cbsb-off-cbsb_fcin0.3_fcout0.1']
	arch_strings += ['k4_s8_universal_topology-single-wirelength_fcin0.6_fcout0.3']
	arch_strings += ['k4_s4_wilton_topology-single-wirelength_fcin0.4_fcout0.4']
	arch_strings += ['k4_s2_g4_subset_topology-on-cb-off-cb_fcin0.3_fcout0.6']
	arch_strings += ['k4_s2_g16_universal_topology-on-sb-off-sb_fcin0.1_fcout0.1']
	arch_strings += ['k4_s2_g8_universal_topology-on-cbsb-off-cbsb_fcin0.3_fcout0.6']
	arch_strings += ['k4_s4_subset_topology-single-wirelength_fcin0.4_fcout0.6']
	arch_strings += ['k4_s2_g8_wilton_topology-on-cb-off-sb_fcin0.2_fcout0.1']
	arch_strings += ['k4_s2_wilton_topology-single-wirelength_fcin0.2_fcout0.6']
	arch_strings += ['k4_s1_subset_topology-single-wirelength_fcin0.6_fcout0.3']
	arch_strings += ['k4_s2_wilton_topology-single-wirelength_fcin0.4_fcout0.4']
	arch_strings += ['k4_s1_universal_topology-single-wirelength_fcin0.4_fcout0.1']
	arch_strings += ['k4_s2_g16_wilton_topology-on-cb-off-sb_fcin0.1_fcout0.2']
	arch_strings += ['k4_s8_universal_topology-single-wirelength_fcin0.2_fcout0.1']
	arch_strings += ['k4_s2_g4_subset_topology-on-sb-off-sb_fcin0.3_fcout0.1']
	arch_strings += ['k4_s2_g8_universal_topology-on-cb-off-sb_fcin0.3_fcout0.1']
	arch_strings += ['k4_s4_wilton_topology-single-wirelength_fcin0.2_fcout0.2']
	arch_strings += ['k4_s8_wilton_topology-single-wirelength_fcin0.4_fcout0.2']

	#build a list of arch points based on the arch strings
	for arch_str in arch_strings:
		arch_point = Arch_Point_Info.from_str(arch_str)
		arch_list += [arch_point]

	return arch_list


#just a wrapper for the function of the same name in arch_handler.py
def get_path_to_arch(arch_point):
	arch_path = ''

	sb_pattern = arch_point.switchblock_pattern
	wire_topology = arch_point.wire_topology
	wirelengths = {}
	wirelengths['semi-global'] = arch_point.s_wirelength
	if arch_point.g_wirelength != None:
		wirelengths['global'] = arch_point.g_wirelength
	global_via_repeat = 4
	fc_in = arch_point.fcin
	fc_out = arch_point.fcout
	lut_size = str(arch_point.lut_size) + 'LUT'

	arch_path = ah.get_path_to_arch(sb_pattern, wire_topology, wirelengths, global_via_repeat, fc_in, fc_out, lut_size)

	return arch_path


