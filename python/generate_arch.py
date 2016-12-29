import os
import subprocess
import re
import getopt
import sys

import arch_handler as ah
from my_regex import *

#==================================================================
# Global functions
def runCommand(command, arguments):
	ret = subprocess.check_output([command] + arguments)
	return ret

def makeWotan(wotanPath):
	print("Making Wotan...")
	os.chdir(wotanPath)
	ret = runCommand("make", [])
	return ret

def makeVPR(vprPath):
	print("Making VPR...")
	os.chdir(vprPath)
	ret = runCommand("make", [])
	return ret

# Copied from .../wotan/python/wotan_tester.py
def get_arch_to_path(arch_point):
	assert isinstance(arch_point, Arch_Point_Info)
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
	
	arch_path = ah.get_path_to_arch(sb_pattern, wire_topology, wirelengths, global_via_repeat, \
									fc_in, fc_out, lut_size)

	return arch_path


#==================================================================
# Class copied from .../wotan/python/wotan_tester.py
class Arch_Point_Info:
	def __init__(self, lut_size,			# Size of the LUT (i.e. K)
					s_wirelength,			# Semi-global wirelength
					g_wirelength,			# Global-layer wirelength; Specify None if not used
					switchblock_pattern,	# wilton/universal/subset
					wire_topology,			# 'single-wirelength', 'on-cb-off-cb', 'on-cb-off-sb',
											# 'on-cb-off-cbsb', 'on-cbsb-off-cbsb', 'on-sb-off-sb'
					fcin,					# cb input flexibility
					fcout,					# cb output flexibility
					arch_string = None):	# Optional string that describes this architecture

		if lut_size not in [4, 6]:
			raise BaseException, 'Unexpected LUT size: %d' % (lut_size)

		if switchblock_pattern not in ['wilton', 'universal', 'subset']:
			raise BaseException, 'Unexpected switch block pattern: %s' % (switchblock_pattern)

		if wire_topology not in ['single-wirelength', 'on-cb-off-cb', 'on-cb-off-sb', \
								 'on-cb-off-cbsb', 'on-cbsb-off-cbsb', 'on-sb-off-sb']:
			raise BaseException, 'Unexpected wire topology: %s' % (wire_topology)

		self.lut_size = lut_size
		self.s_wirelength = s_wirelength
		self.g_wirelength = g_wirelength
		self.switchblock_pattern = switchblock_pattern
		self.wire_topology = wire_topology
		self.fcin = fcin
		self.fcout = fcout
		self.arch_string = arch_string

	# Overload constructor -- initialize based on a string. Expecting string to be in
	# the format of this class' 'as_str' function.
	@classmethod
	def from_str(cls, s):
		regex_list = {
			's_wirelength' : '.*_s(\d+)_.*',
			'g_wirelength' : '.*_g(\d+)_.*',
			'K' : '.*k(\d)_.*',
			'wire_topology' : '.*_topology-([-\w]+)_.*',
			'fcin' : '.*fcin(\d+\.*\d*)',
			'fcout' : '.*fcout(\d+\.*\d*)',
		}

		# Get wirelength, fcin, fcout
		tmp_dict = {}
		for key in regex_list:
			try:
				tmp_dict[key] = regex_last_token(s, regex_list[key])
			except RegexException as exception:
				if key == 'g_wirelength':
					# OK if global wirelength wasn't specified
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

		# Get switchblock
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

	# Returns a string describing an object of this class
	def as_str(self):
		return self.arch_string

	def __str__(self):
		return self.arch_string

	def __repr__(self):
		return self.arch_string


#==================================================================
# Class for running architecture through Wotan
class Wotan:
	def __init__(self, archPath, vtrPath, vprPath, wotanPath, wotanOpts, lut_size):
		self.archPath = archPath
		self.vtrPath = vtrPath
		self.vprPath = vprPath
		self.wotanPath = wotanPath
		self.wotanOpts = wotanOpts
		self.lut_size = lut_size

	def runWotan(self):
		benchmark = 'vtr_benchmarks_blif/sha.blif'
		if self.lut_size == 4:
			benchmark = '4LUT_DSP_vtr_benchmarks_blif/sha.pre-vpr.blif'

		vprOpts = self.archPath + ' ' + self.vtrPath + '/vtr_flow/benchmarks/' + benchmark + \
				  ' -dump_rr_structs_file ./dumped_rr_structs.txt ' + \
				  '-pack -place -route_chan_width ' + str(chanWidth)

		# Run VPR to get RRG
		ret = self._runVPRGetRRG(vprOpts)
		assert ret

		# Run Wotan to get routability metric
		ret = self._runWotan()
		assert ret

	def _runVPRGetRRG(self, vprOpts):
		print("Running VPR to get RRG...")
		os.chdir(self.vprPath)
		argsList = vprOpts.split()
		output = runCommand("./vpr", argsList)
		return output

	def _runWotan(self):
		print("Running Wotan to get routability metric...")
		os.chdir(self.wotanPath)
		argsList = self.wotanOpts.split()
		output = runCommand("./wotan", argsList)
		return output

#==================================================================
# Generates the custom architecture file
class GenerateArch:
	def __init__(self, arch_str):
		self.arch_str = arch_str

	def getArch(self):
		#arch_str = 'k4_s1_subset_topology-single-wirelength_fcin0.3_fcout0.4'
		arch = Arch_Point_Info.from_str(self.arch_str)
		return arch

	def getCustomArch(self, archPoint):
		# Returns the path to the architecture path
		assert isinstance(archPoint, Arch_Point_Info)
		archPath = get_arch_to_path(archPoint)
		print "Arch File Path: ", archPath


#==================================================================
# Main function
def main(arch_str):
	base_path = "/nfs/ug/homes-4/k/kongnath/code"
	vtrPath = base_path + "/vtr"
	vprPath = vtrPath + "/vpr"
	wotan_path = base_path + "/wotan"
	arch_dir = wotan_path + "/arch"

	ga = GenerateArch(arch_str)
	arch = ga.getArch()
	ga.getCustomArch(arch)


if __name__ == "__main__":
	try:
		opts, args = getopt.getopt(sys.argv[1:], "a:")
	except getopt.GetOptError as err:
		print str(err)
		sys.exit(2)

	arch = ""
	for o, a in opts:
		if o == '-a':
			arch = a
		else:
			sys.exit(2)

#	arch = 'k4_s1_subset_topology-single-wirelength_fcin0.3_fcout0.2'
#	arch = 'k4_s1_subset_topology-single-wirelength_fcin0.2_fcout0.1'
	if not arch:
		print "Need arch name."
		sys.exit(2)

	print arch
	main(arch)


