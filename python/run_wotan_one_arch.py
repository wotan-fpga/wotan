import os
import subprocess
import re

import arch_handler as ah
from my_regex import *

#==================================================================
# Global function
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
# Only runs Wotan for one architecture
class WotanTester:
	def __init__(self, vtrPath, wotanPath, outFileName):
		self._vtrPath = vtrPath
		self._vprPath = vtrPath + "/vpr"
		self._wotanPath = wotanPath
		self._outFile = outFileName

		print("Wotan Path: %s" % self._wotanPath)
		print("VTR Path: %s" % self._vtrPath)
		print("VPR Path: %s" % self._vprPath)
		print("Results File Path: %s" % self._outFile)

	def evaluateArchitecture(self, archPoint, wotanOpts, pathToCustomArch = ""):
		assert isinstance(archPoint, Arch_Point_Info)
		print("Evaluating %s" % archPoint)

		chanWidths = [10] # Use channel widths in list for testing architectures in Wotan
		self._makeWotan()
		self._makeVPR()

		wotanArchPath = ""
		if not pathToCustomArch:
			wotanArchPath = get_arch_to_path(archPoint)
		else:
			wotanArchPath = pathToCustomArch
		print "Arch File Path: ", wotanArchPath

		for chanWidth in chanWidths:
			print("Current channel width: %d" % chanWidth)
			benchmark = 'vtr_benchmarks_blif/sha.blif'
			if archPoint.lut_size == 4:
				benchmark = '4LUT_DSP_vtr_benchmarks_blif/sha.pre-vpr.blif'

						#' -nodisp -dump_rr_structs_file ./dumped_rr_structs.txt ' + \
			vprOpts = wotanArchPath + ' ' + self._vtrPath + '/vtr_flow/benchmarks/' + benchmark + \
						' -dump_rr_structs_file ./dumped_rr_structs.txt ' + \
						'-pack -place -route_chan_width ' + str(chanWidth)
			print(vprOpts)

			# Run VPR to get RRG
			vprOut = self._runVPR(vprOpts)

			# Run Wotan to get routability metric
			wotanOut = self._runWotan(wotanOpts)

	def getArch(self):
		#arch_str = 'k6_s16_subset_topology-single-wirelength_fcin0.2_fcout0.2'
		#arch_str = 'k4_s1_subset_topology-single-wirelength_fcin0.2_fcout0.2'
		arch_str = 'k4_s1_subset_topology-single-wirelength_fcin0.5_fcout0.5'
		#arch_str = 'k4_s2_g16_subset_topology-on-cb-off-sb_fcin0.1_fcout0.1'
		arch = Arch_Point_Info.from_str(arch_str)
		return arch

	def _makeVPR(self):
		print("Making VPR...")
		os.chdir(self._vprPath)
		ret = self._runCommand("make", [])
		return ret

	def _runVPR(self, vprOpts):
		print("Running VPR...")
		os.chdir(self._vprPath)
		argsList = vprOpts.split()
		output = self._runCommand("./vpr", argsList)
		return output

	def _makeWotan(self):
		print("Making Wotan...")
		os.chdir(self._wotanPath)
		ret = self._runCommand("make", [])
		return ret

	def _runWotan(self, wotanOpts):
		print("Running Wotan...")
		os.chdir(self._wotanPath)
		argsList = wotanOpts.split()
		#print(argsList)
		output = self._runCommand("./wotan", argsList)

	def _runCommand(self, command, arguments):
		ret = subprocess.check_output([command] + arguments)
		return ret


#==================================================================
# Main function
def main():
	base_path = "/nfs/ug/homes-4/k/kongnath/code"
	vtr_path = base_path + "/vtr"
	wotan_path = base_path + "/wotan"
	arch_dir = wotan_path + "/arch"
	out_file = wotan_path + "/python/test.txt"
	customArch = arch_dir + "/4LUT_DSP/test/k4_N8_topology-1.0sL1_22nm.xml"

	wt = WotanTester(vtr_path, wotan_path, out_file)
	arch = wt.getArch()

	wotanOpts = '-rr_structs_file ' + vtr_path + '/vpr' + '/dumped_rr_structs.txt -threads 10 ' + \
				'-demand_multiplier 140 -max_connection_length 8 -keep_path_count_history n'
	#wt.evaluateArchitecture(arch, wotanOpts, customArch)

# ./wotan -rr_structs_file /nfs/ug/homes-4/k/kongnath/code/vtr/vpr/dumped_rr_structs.txt -threads 10 -demand_multiplier 140 -max_connection_length 8 -keep_path_count_history n

if __name__ == "__main__":
	main()

