
import random
from my_regex import *

#write requirements of this script here


arch_base = '/home/opetelin/wotan/arch'

valid_switchblocks = ['wilton', 'universal', 'subset']
valid_topologies = ['on-cb-off-cb',
		    'on-cb-off-sb',
		    'on-cb-off-cbsb',
		    'on-cbsb-off-cbsb',
		    'on-sb-off-sb',
		    'single-wirelength',
		    'single-wirelength',
		    'single-wirelength',
		    'single-wirelength',
		    'single-wirelength']
valid_wirelengths = ['1', '2', '4', '8', '16',
	'4-4', '4-8', '4-16', '2-4', '2-8', '2-16']




#key: length1, length2, 4LUT/6LUT, regular/prime
arch_map = {
('1', '',   '6LUT', 'regular'): '6LUT/L1/k6_N10_topology-1.0sL1_22nm.xml',
('2', '',   '6LUT', 'regular'): '6LUT/L2/k6_N10_topology-1.0sL2_22nm.xml',
('4', '',   '6LUT', 'regular'): '6LUT/L4/k6_N10_topology-1.0sL4_22nm.xml',
('8', '',   '6LUT', 'regular'): '6LUT/L8/k6_N10_topology-1.0sL8_22nm.xml',
('16', '',  '6LUT', 'regular'): '6LUT/L16/k6_N10_topology-1.0sL16_22nm.xml',
('4', '4',  '6LUT', 'regular'): '6LUT/L4-4/k6_N10_topology-0.85sL4-0.15gL4_22nm.xml',
('4', '8',  '6LUT', 'regular'): '6LUT/L4-8/k6_N10_topology-0.85sL4-0.15gL8_22nm.xml',
('4', '16', '6LUT', 'regular'): '6LUT/L4-16/k6_N10_topology-0.85sL4-0.15gL16_22nm.xml',
('4', '4',  '6LUT', 'prime'):   '6LUT/L4-4/k6_N10_topology-0.55sL4-0.3spL4-0.15gL4_22nm.xml',
('4', '8',  '6LUT', 'prime'):   '6LUT/L4-8/k6_N10_topology-0.55sL4-0.3spL4-0.15gL8_22nm.xml',
('4', '16', '6LUT', 'prime'):   '6LUT/L4-16/k6_N10_topology-0.55sL4-0.3spL4-0.15gL16_22nm.xml',

('1', '',   '4LUT', 'regular'): '4LUT_DSP/L1/k4_N8_topology-1.0sL1_22nm.xml',
('2', '',   '4LUT', 'regular'): '4LUT_DSP/L2/k4_N8_topology-1.0sL2_22nm.xml',
('4', '',   '4LUT', 'regular'): '4LUT_DSP/L4/k4_N8_topology-1.0sL4_22nm.xml',
('8', '',   '4LUT', 'regular'): '4LUT_DSP/L8/k4_N8_topology-1.0sL8_22nm.xml',
#('16', '',  '4LUT', 'regular'): '4LUT_DSP/L16/k4_N8_topology-1.0sL16_22nm.xml',
('2', '4',  '4LUT', 'regular'): '4LUT_DSP/L2-4/k4_N8_topology-0.85sL2-0.15gL4_22nm.xml',
('2', '8',  '4LUT', 'regular'): '4LUT_DSP/L2-8/k4_N8_topology-0.85sL2-0.15gL8_22nm.xml',
('2', '16', '4LUT', 'regular'): '4LUT_DSP/L2-16/k4_N8_topology-0.85sL2-0.15gL16_22nm.xml',
('2', '4',  '4LUT', 'prime'):   '4LUT_DSP/L2-4/k4_N8_topology-0.65sL2-0.2spL2-0.15gL4_22nm.xml',
('2', '8',  '4LUT', 'prime'):   '4LUT_DSP/L2-8/k4_N8_topology-0.65sL2-0.2spL2-0.15gL8_22nm.xml',
('2', '16', '4LUT', 'prime'):   '4LUT_DSP/L2-16/k4_N8_topology-0.65sL2-0.2spL2-0.15gL16_22nm.xml'
}


#Can represent the architecture point as a string. Will need to specify:
# - LUT size
# - sb pattern
# - topology (on-cb-off-cb, etc)
# - wirelengths (length1:2-length2:4)
# - fc_in
# - fc_out

#format:
#k4_s2_g8_wilton_topology-on-cb-off_sb_fcin0.15_fcout0.1


#returns the full path to a reference architecture that has the specified parameters
def get_path_to_arch(sb_pattern, wire_topology, wirelengths, global_via_repeat, fc_in, fc_out, lut_size):
	result = ''

	#### Error Checks ####
	#wirelengths should match the topology
	if wire_topology != 'single-wirelength' and not wirelengths.has_key('global'):
		raise ArchException, 'Complex topologies must specify a global wirelength'
	
	if lut_size != '4LUT' and lut_size != '6LUT':
		raise ArchException, 'Illegal LUT size %d' % (lut_size)

	#### Get strings for the switch block, depopulation and conn block flexibility ####
	sb_string = get_custom_switchblock(sb_pattern, wire_topology, wirelengths, global_via_repeat)
	cb_depop_string = ''
	if wire_topology != 'single-wirelength':
		cb_depop_string = get_cb_depop_string( wirelengths['global'], global_via_repeat )
	clb_fc_string = get_fc_string(wire_topology, wirelengths, fc_in, fc_out)
	other_fc_string = get_fc_string(wire_topology, wirelengths, 1.0, 1.0)

	#print(sb_string)
	#print(cb_depop_string)
	#print(fc_string)


	#### get path to reference architecture ####
	length1 = wirelengths['semi-global']
	length2 = ''
	if wirelengths.has_key('global'):
		length2 = wirelengths['global']

	arch_type = 'regular'
	if wire_topology == 'on-sb-off-sb' or wire_topology == 'on-cbsb-off-cbsb':
		arch_type = 'prime'

	arch_map_key = ( str(length1), str(length2), lut_size, arch_type )

	if not arch_map.has_key(arch_map_key):
		raise ArchException, 'No arch map key: %s ' % (str(arch_map_key))

	reference_arch_path = arch_base + '/' + arch_map[ arch_map_key ]
	print(reference_arch_path)


	#### regex new parameters into reference architecture ####
	replace_custom_switchblock(reference_arch_path, sb_string)
	replace_fc(reference_arch_path, clb_fc_string, other_fc_string)
	replace_cb_depop(reference_arch_path, cb_depop_string)

	result = reference_arch_path
	return result


def replace_custom_switchblock(arch_path, new_switchblock_string):
	switchblock_regex = '\h*<switchblocklist>.*</switchblocklist>\h*'
	replace_pattern_in_file(switchblock_regex, new_switchblock_string, arch_path, multiline=True)

def replace_fc(arch_path, new_clb_fc_string, new_other_fc_string):
	#make sure all Fc's are set to 1.0...
	fc_regex = '\h*<fc def[^-]*</fc>\h*'	#hack, but [^-]* catches XML comments and makes sure we don't replace everything between first and last fc
	replace_pattern_in_file(fc_regex, new_other_fc_string, arch_path, multiline=True)

	#now replace the CLB Fc with the intended Fc
	fc_regex = '\h*<fc def[^-]*</fc>\h*(?!.*pb_type name="clb".*)'
	replace_pattern_in_file(fc_regex, new_clb_fc_string, arch_path, count=1, multiline=True)

def replace_cb_depop(arch_path, new_cb_depop_string):
	if new_cb_depop_string == '':
		return

	cb_depop_regex = '<cb type="pattern">[1,0,\s]+</cb>(?!.*name="l\d+g".*)'
	replace_pattern_in_file(cb_depop_regex, new_cb_depop_string, arch_path)


#returns a fc string according to the specified parameters
def get_fc_string(topology, wirelengths, fc_in, fc_out):
	result = '<fc default_in_type="frac" default_in_val="%f" default_out_type="frac" default_out_val="%f">\n' % (fc_in, fc_out)

	#wire-type based overrides are specified for the on-cb-off-sb topology
	if topology == 'on-cb-off-sb':
		global_length = wirelengths['global']
		result += '\t\t<segment name="l%dg" in_val="0" out_val="%f"/>\n' % (global_length, fc_out)
	
	result += '\t</fc>'
	return result
	

#returns a cb depopulation string with the specified parameters
def get_cb_depop_string(wirelength, via_repeat):
	result = '<cb type="pattern">'

	for cb_index in range(0, wirelength, 1):
		char = '0'
		if cb_index % via_repeat == 0:
			char = '1'
		result += char + ' '
	result += '</cb>'
	return result


#returns a string with the specified custom switch block topology
def get_custom_switchblock(pattern, topology, wirelengths, global_via_repeat):


	#check pattern type
	if pattern not in valid_switchblocks:
		msg = 'Unrecognized switch pattern: ' + str(pattern)	
		raise ArchException, msg

	#check topology type
	if topology not in valid_topologies:
		msg = 'Unrecognized topology: ' + str(topology)
		raise ArchException, msg

	permutations_bends, permutations_straight = get_sb_switch_funcs(pattern)

	wireconns_turn_core, wireconns_turn_perimeter, wireconns_straight = get_sb_wireconns(topology, wirelengths, global_via_repeat)

	#build the switch blocks
	result = ''
	result = '''<switchblocklist>
	\t<switchblock name="%s_turn_core" type="unidir">
	\t\t<switchblock_location type="CORE"/>
	\t\t<switchfuncs>
	%s
	\t\t</switchfuncs>
	%s
	\t</switchblock>
	\t<switchblock name="%s_turn_perimeter" type="unidir">
	\t\t<switchblock_location type="PERIMETER"/>
	\t\t<switchfuncs>
	%s
	\t\t</switchfuncs>
	%s
	\t</switchblock>
	\t<switchblock name="%s_straight" type="unidir">
	\t\t<switchblock_location type="EVERYWHERE"/>
	\t\t<switchfuncs>
	%s
	\t\t</switchfuncs>
	%s
	\t</switchblock>
	</switchblocklist>''' % (pattern, permutations_bends, wireconns_turn_core, pattern, permutations_bends, wireconns_turn_perimeter, pattern, permutations_straight, wireconns_straight)

	return result


#return the core wireconn connections for the specified topology and wirelenghts
def get_sb_wireconns(topology, wirelengths, global_via_repeat):

	#get wirelengths
	length1 = wirelengths['semi-global']
	length2 = -1
	if wirelengths.has_key('global'):
		length2 = wirelengths['global']

	#check that global wirelength is defined if using a complex topology
	if length2 == -1 and topology != 'single-wirelength':
		raise ArchException, 'Topology ' + str(topology) + ' must specify a global wirelength!'

	result_turn_core = ''
	result_turn_perimeter = ''
	result_straight = ''

	#get wireconns for the specified topology
	if topology == 'on-cb-off-cb':
		#turn core
		for sp in range(0, length1, 1):
			result_turn_core += '\t\t\t<wireconn FT="l%ds" TT="l%ds" FP="%d" TP="0"/>\n' % (length1, length1, sp)
		for sp in range(0, length2, global_via_repeat):
			result_turn_core += '\t\t\t<wireconn FT="l%dg" TT="l%dg" FP="%d" TP="0"/>\n' % (length2, length2, sp)
		#turn perimeter
		result_turn_perimeter += '\t\t\t<wireconn FT="l%ds" TT="l%ds" FP="' % (length1, length1)
		for sp in range(0, length1, 1):
			result_turn_perimeter += '%d,' % (sp)
		result_turn_perimeter = result_turn_perimeter[:-1]	#getting rid of final comma
		result_turn_perimeter += '" TP="0"/>\n'

		result_turn_perimeter += '\t\t\t<wireconn FT="l%dg" TT="l%dg" FP="' % (length2, length2)
		for sp in range(0, length2, global_via_repeat):
			result_turn_perimeter += '%d,' % (sp)
		result_turn_perimeter = result_turn_perimeter[:-1]	#getting rid of final comma
		result_turn_perimeter += '" TP="0"/>\n'
		#straight
		result_straight  += '\t\t\t<wireconn FT="l%ds" TT="l%ds" FP="0" TP="0"/>\n' % (length1, length1)
		for sp in range(0, length2, global_via_repeat):
			result_straight += '\t\t\t<wireconn FT="l%dg" TT="l%dg" FP="%d" TP="0"/>\n' % (length2, length2, sp)

	elif topology == 'on-cb-off-sb' or topology == 'on-cb-off-cbsb':
		#turn core
		for sp in range(0, length1, 1):
			result_turn_core += '\t\t\t<wireconn FT="l%ds" TT="l%ds" FP="%d" TP="0"/>\n' % (length1, length1, sp)
		for sp in range(0, length2, global_via_repeat):
			result_turn_core += '\t\t\t<wireconn FT="l%dg" TT="l%dg" FP="%d" TP="0"/>\n' % (length2, length2, sp)
		for sp in range(0, length2, global_via_repeat):
			result_turn_core += '\t\t\t<wireconn FT="l%dg" TT="l%ds" FP="%d" TP="0"/>\n' % (length2, length1, sp)
		#turn perimeter
		result_turn_perimeter += '\t\t\t<wireconn FT="l%ds" TT="l%ds" FP="' % (length1, length1)
		for sp in range(0, length1, 1):
			result_turn_perimeter += '%d,' % (sp)
		result_turn_perimeter = result_turn_perimeter[:-1]	#getting rid of final comma
		result_turn_perimeter += '" TP="0"/>\n'

		result_turn_perimeter += '\t\t\t<wireconn FT="l%dg" TT="l%dg" FP="' % (length2, length2)
		for sp in range(0, length2, global_via_repeat):
			result_turn_perimeter += '%d,' % (sp)
		result_turn_perimeter = result_turn_perimeter[:-1]	#getting rid of final comma
		result_turn_perimeter += '" TP="0"/>\n'

		result_turn_perimeter += '\t\t\t<wireconn FT="l%dg" TT="l%ds" FP="' % (length2, length1)
		for sp in range(0, length2, global_via_repeat):
			result_turn_perimeter += '%d,' % (sp)
		result_turn_perimeter = result_turn_perimeter[:-1]	#getting rid of final comma
		result_turn_perimeter += '" TP="0"/>\n'
		#straight
		result_straight  += '\t\t\t<wireconn FT="l%ds" TT="l%ds" FP="0" TP="0"/>\n' % (length1, length1)
		for sp in range(0, length2, global_via_repeat):
			result_straight += '\t\t\t<wireconn FT="l%dg" TT="l%dg" FP="%d" TP="0"/>\n' % (length2, length2, sp)
		for sp in range(0, length2, global_via_repeat):
			result_straight += '\t\t\t<wireconn FT="l%dg" TT="l%ds" FP="%d" TP="0"/>\n' % (length2, length1, sp)

	elif topology == 'on-cbsb-off-cbsb' or topology == 'on-sb-off-sb':
		#turn core
		for sp in range(0, length1, 1):
			result_turn_core += '\t\t\t<wireconn FT="l%ds" TT="l%ds" FP="%d" TP="0"/>\n' % (length1, length1, sp)
		for sp in range(0, length1, 1):
			result_turn_core += '\t\t\t<wireconn FT="l%dsprime" TT="l%dsprime" FP="%d" TP="0"/>\n' % (length1, length1, sp)
		result_turn_core += '\t\t\t<wireconn FT="l%dsprime" TT="l%dg" FP="0" TP="0"/>\n' % (length1, length2)
		for sp in range(0, length2, global_via_repeat):
			result_turn_core += '\t\t\t<wireconn FT="l%dg" TT="l%dg" FP="%d" TP="0"/>\n' % (length2, length2, sp)
		for sp in range(0, length2, global_via_repeat):
			result_turn_core += '\t\t\t<wireconn FT="l%dg" TT="l%dsprime" FP="%d" TP="0"/>\n' % (length2, length1, sp)

		#turn perimeter
		result_turn_perimeter += '\t\t\t<wireconn FT="l%ds" TT="l%ds" FP="' % (length1, length1)
		for sp in range(0, length1, 1):
			result_turn_perimeter += '%d,' % (sp)
		result_turn_perimeter = result_turn_perimeter[:-1]	#getting rid of final comma
		result_turn_perimeter += '" TP="0"/>\n'

		result_turn_perimeter += '\t\t\t<wireconn FT="l%dsprime" TT="l%dsprime" FP="' % (length1, length1)
		for sp in range(0, length1, 1):
			result_turn_perimeter += '%d,' % (sp)
		result_turn_perimeter = result_turn_perimeter[:-1]	#getting rid of final comma
		result_turn_perimeter += '" TP="0"/>\n'

		result_turn_perimeter += '\t\t\t<wireconn FT="l%dsprime" TT="l%dg" FP="0" TP="0"/>\n' % (length1, length2)
		#for sp in range(0, length1, 1):
		#	result_turn_perimeter += '%d,' % (sp)
		#result_turn_perimeter = result_turn_perimeter[:-1]	#getting rid of final comma
		#result_turn_perimeter += '" TP="0"/>\n'

		result_turn_perimeter += '\t\t\t<wireconn FT="l%dg" TT="l%dg" FP="' % (length2, length2)
		for sp in range(0, length2, global_via_repeat):
			result_turn_perimeter += '%d,' % (sp)
		result_turn_perimeter = result_turn_perimeter[:-1]	#getting rid of final comma
		result_turn_perimeter += '" TP="0"/>\n'

		result_turn_perimeter += '\t\t\t<wireconn FT="l%dg" TT="l%dsprime" FP="' % (length2, length1)
		for sp in range(0, length2, global_via_repeat):
			result_turn_perimeter += '%d,' % (sp)
		result_turn_perimeter = result_turn_perimeter[:-1]	#getting rid of final comma
		result_turn_perimeter += '" TP="0"/>\n'

		#straight
		result_straight  += '\t\t\t<wireconn FT="l%ds" TT="l%ds" FP="0" TP="0"/>\n' % (length1, length1)
		result_straight  += '\t\t\t<wireconn FT="l%dsprime" TT="l%dsprime" FP="0" TP="0"/>\n' % (length1, length1)
		result_straight  += '\t\t\t<wireconn FT="l%dsprime" TT="l%dg" FP="0" TP="0"/>\n' % (length1, length2)
		for sp in range(0, length2, global_via_repeat):
			result_straight += '\t\t\t<wireconn FT="l%dg" TT="l%dg" FP="%d" TP="0"/>\n' % (length2, length2, sp)
		for sp in range(0, length2, global_via_repeat):
			result_straight += '\t\t\t<wireconn FT="l%dg" TT="l%dsprime" FP="%d" TP="0"/>\n' % (length2, length1, sp)

	elif topology == 'single-wirelength':
		#only have one wirelength
		result_straight  += '\t\t\t<wireconn FT="l%ds" TT="l%ds" FP="0" TP="0"/>' % (length1, length1)
		result_turn_perimeter += '\t\t\t<wireconn FT="l%ds" TT="l%ds" FP="' % (length1, length1)
		for sp in range(0, length1, 1):
			result_turn_core += '\t\t\t<wireconn FT="l%ds" TT="l%ds" FP="%d" TP="0"/>\n' % (length1, length1, sp)
			result_turn_perimeter += '%d,' % (sp)
		result_turn_perimeter = result_turn_perimeter[:-1]	#getting rid of final comma
		result_turn_perimeter += '" TP="0"/>'
	else:
		raise ArchException, 'Unrecognized topology: ' + str(topology)

	return result_turn_core, result_turn_perimeter, result_straight


#return permutation functions for the specified pattern
def get_sb_switch_funcs(pattern):
	result_bends = ''
	result_straight = ''

	if pattern == 'wilton':
		result_bends = """\t\t\t\t<func type="lt" formula="W-t"/>
		\t\t\t<func type="lb" formula="t-1"/>
		\t\t\t<func type="rt" formula="t-1"/>
		\t\t\t<func type="br" formula="W-t-2"/>
		\t\t\t<func type="tl" formula="W-t"/>
		\t\t\t<func type="bl" formula="t+1"/>
		\t\t\t<func type="tr" formula="t+1"/>
		\t\t\t<func type="rb" formula="W-t-2"/>\n"""
		result_straight = """\t\t\t\t<func type="lr" formula="t"/>
		\t\t\t<func type="bt" formula="t"/>
		\t\t\t<func type="rl" formula="t"/>
		\t\t\t<func type="tb" formula="t"/>\n"""
	elif pattern == 'universal':
		result_bends = """\t\t\t\t<func type="lt" formula="W-t-1"/>
		\t\t\t<func type="lb" formula="t"/>
		\t\t\t<func type="rt" formula="t"/>
		\t\t\t<func type="br" formula="W-t-1"/>
		\t\t\t<func type="tl" formula="W-t-1"/>
		\t\t\t<func type="bl" formula="t"/>
		\t\t\t<func type="tr" formula="t"/>
		\t\t\t<func type="rb" formula="W-t-1"/>\n"""
		result_straight = """\t\t\t\t<func type="lr" formula="t"/>
		\t\t\t<func type="bt" formula="t"/>
		\t\t\t<func type="rl" formula="t"/>
		\t\t\t<func type="tb" formula="t"/>\n"""
	elif pattern == 'subset':
		result_bends = """\t\t\t\t<func type="lt" formula="t"/>
		\t\t\t<func type="lb" formula="t"/>
		\t\t\t<func type="rt" formula="t"/>
		\t\t\t<func type="br" formula="t"/>
		\t\t\t<func type="tl" formula="t"/>
		\t\t\t<func type="bl" formula="t"/>
		\t\t\t<func type="tr" formula="t"/>
		\t\t\t<func type="rb" formula="t"/>\n"""
		result_straight = """\t\t\t\t<func type="lr" formula="t"/>
		\t\t\t<func type="bt" formula="t"/>
		\t\t\t<func type="rl" formula="t"/>
		\t\t\t<func type="tb" formula="t"/>\n"""
	else:
		raise ArchException, 'Unrecognized switch pattern: ' + str(pattern)

	return result_bends, result_straight




#return the specified number of architectures that have the specified LUT size
def get_random_arch_names(num_archs, lut_size):

	if lut_size not in [4, 6]:
		raise ArchException, 'Invalid LUT size: %d' % (lut_size)

	fc_in_vals = ['0.05', '0.1', '0.2', '0.4', '0.6']
	fc_out_vals = ['0.05', '0.1', '0.2', '0.4', '0.6']
	if lut_size == 4:
		fc_in_vals = ['0.1', '0.2', '0.3', '0.4', '0.6']
		fc_out_vals = ['0.1', '0.2', '0.3', '0.4', '0.6']


	arch_string_list = []
	

	while len(arch_string_list) != num_archs:

		#get random switch block pattern
		switchblock = random.choice(valid_switchblocks)

		#get random wirelength mix
		wirelength_mix = random.choice(valid_wirelengths)
		wirelengths = wirelength_mix.split('-')
		s_wirelength = wirelengths[0]
		g_wirelength = None
		if len(wirelengths) == 2:
			g_wirelength = wirelengths[1]

		#check that the wirelength mix makes sense with the given LUT size
		if ( (lut_size == 4 and (wirelength_mix == '4-4' or wirelength_mix == '4-8' or wirelength_mix == '4-16' or wirelength_mix == '16')) 
		    or (lut_size == 6 and (wirelength_mix == '2-4' or wirelength_mix == '2-8' or wirelength_mix == '2-16')) ):
			continue

		#get random wire topology
		topology = random.choice(valid_topologies)

		#check that wire topology matches up with the wire mix
		if (g_wirelength == None and topology != 'single-wirelength') or (g_wirelength != None and topology == 'single-wirelength'):
			continue

		#get random fc_in and fc_out
		fc_in = random.choice(fc_in_vals)
		fc_out = random.choice(fc_out_vals)

		#make the arch string
		arch_string = 'k%d_s%s_' % (lut_size, s_wirelength)
		if g_wirelength != None:
			arch_string += 'g%s_' % (g_wirelength)
		arch_string += '%s_topology-%s_fcin%s_fcout%s' % (switchblock, topology, fc_in, fc_out)

		#check for duplicates
		if arch_string in arch_string_list:
			continue

		arch_string_list += [arch_string]

	return arch_string_list



class ArchException(BaseException):
	pass











##replaces CLB Fc_in and Fc_out in the architecture file according to specified parameters
##count specifies how many instances of the fcin/fcout line to replace. If 0 or omitted, replace all.
#def replace_arch_fc(arch_path, fc_in, fc_out, count=0):
#	clb_regex = '.*pb_type name="clb".*'
#	fc_regex = 'fc default_in_type="frac" default_in_val="\d+\.\d*" default_out_type="frac" default_out_val="\d+\.\d*"'
#	new_line = 'fc default_in_type="frac" default_in_val="' + str(fc_in) + '" default_out_type="frac" default_out_val="' + str(fc_out) + '"'
#
#	if count == 1:
#		#use negative lookahead to replace the first 'fc def...' line that occurs after the clb pb type specification
#		lookahead_regex = '(?!' + clb_regex + ')'
#		line_regex = fc_regex + lookahead_regex
#	else:
#		#if we want to replace multiple instances of the pattern then it doesn't make sense to use lookahead
#		line_regex = fc_regex
#
#	replace_pattern_in_file(line_regex, new_line, arch_path, count=count)
#
#
##replaces wirelength in the architecture with the specified length. switch population is full
#def replace_arch_wirelength(arch_path, new_wirelength):
#	#regex for old lines
#	#segment_regex = '<segment freq="\d\.\d*" length="\d+" type="unidir" Rmetal="101" Cmetal="22.5e-15">'	#TODO: this changes for 22nm arch
#	segment_regex = '<segment freq="\d\.\d*" length="\d+" type="unidir" Rmetal="0.0" Cmetal="0.0">'	#TODO: this changes for 22nm arch
#	sb_type_regex = '<sb type="pattern">[1,\s]+</sb>'
#	cb_type_regex = '<cb type="pattern">[1,\s]+</cb>'
#
#	#create regex for new lines
#	new_sb_pattern = ('1 ' * (new_wirelength+1)).strip()
#	new_cb_pattern = ('1 ' * new_wirelength).strip()
#
#	#new_segment = '<segment freq="1.0" length="' + str(new_wirelength) + '" type="unidir" Rmetal="101" Cmetal="22.5e-15">'
#	new_segment = '<segment freq="1.0" length="' + str(new_wirelength) + '" type="unidir" Rmetal="0.0" Cmetal="0.0">'
#	new_sb_type = '<sb type="pattern">' + new_sb_pattern + '</sb>'
#	new_cb_type = '<cb type="pattern">' + new_cb_pattern + '</cb>'
#
#	#do regex replacements
#	replace_pattern_in_file(segment_regex, new_segment, arch_path, count=1)
#	replace_pattern_in_file(sb_type_regex, new_sb_type, arch_path, count=1)
#	replace_pattern_in_file(cb_type_regex, new_cb_type, arch_path, count=1)
#
#
##replaces switch block in the architecture
#def replace_arch_switchblock(arch_path, new_switchblock):
#	if new_switchblock != 'subset' and new_switchblock != 'universal' and new_switchblock != 'wilton':
#		print('unrecognized switch block: ' + str(new_switchblock))
#		sys.exit()
#
#	#regex for old switch block
#	switchblock_regex = '<switch_block type="\w+" fs="3"/>'
#
#	#new switch block line
#	new_switchblock_line = '<switch_block type="' + new_switchblock + '" fs="3"/>'
#
#	replace_pattern_in_file(switchblock_regex, new_switchblock_line, arch_path, count=1)
#
##replaces pin equivalence in architecture
#def replace_arch_pin_equiv(arch_path, new_input_equiv, new_output_equiv, num_ipins=40, num_opins=20):
#	input_equiv_str = ''
#	if new_input_equiv == True:
#		input_equiv_str = 'true'
#	elif new_input_equiv == False:
#		input_equiv_str = 'false'
#	else:
#		print('Expected True/False for input equiv')
#		sys.exit()
#
#	output_equiv_str = ''
#	if new_output_equiv == True:
#		output_equiv_str = 'true'
#	elif new_output_equiv == False:
#		output_equiv_str = 'false'
#	else:
#		print('Expected True/False for output equiv')
#		sys.exit()
#
#	#will use negative lookahead to replace the first input/output equiv lines that occur after the clb pb type specification
#	clb_regex = '.*pb_type name="clb".*'
#
#	input_equiv_regex = '<input name="I" num_pins="\d+" equivalent="\w+"/>'
#	output_equiv_regex = '<output name="O" num_pins="\d+" equivalent="\w+"/>'
#	input_equiv_regex += '(?!' + clb_regex + ')'
#	output_equiv_regex += '(?!' + clb_regex + ')'
#
#	#new equiv lines
#	input_equiv_line = '<input name="I" num_pins="' + str(num_ipins) + '" equivalent="' + input_equiv_str + '"/>'
#	output_equiv_line = '<output name="O" num_pins="' + str(num_opins) + '" equivalent="' + output_equiv_str + '"/>'
#
#	replace_pattern_in_file(input_equiv_regex, input_equiv_line, arch_path, count=1)
#	replace_pattern_in_file(output_equiv_regex, output_equiv_line, arch_path, count=1)
#
#

if __name__ == '__main__':
	#### Some test code ####
	wirelengths = {}
	lut_size = '6LUT'
	wirelengths['semi-global'] = 4
	wirelengths['global'] = 8
	sb_pattern = 'universal'
	wire_topology = 'on-cbsb-off-cbsb'
	global_via_repeat = 2
	fc_in = 0.77
	fc_out = 0.66

	#arch_path = get_path_to_arch(sb_pattern, wire_topology, wirelengths, global_via_repeat, fc_in, fc_out, lut_size)

	arch_name_list = get_random_arch_names(100, 6)
	for a in arch_name_list:
		print(a)

