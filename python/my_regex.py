import re


#replaces strings in specified file according to regex
#if the count variable is omitted or 0 all lines will be replaced. otherwise count
#specifies the maximum number of pattern occurences to be replaced
def replace_pattern_in_file(line_regex, new_line, file_path, count=0, multiline=False):
	#read the old file into a string
	fid = open(file_path, 'r')
	file_string = fid.read()
	fid.close()

	#replace specified line in the string
	flags = re.DOTALL
	if multiline:
		flags |= re.MULTILINE
	new_file_string = re.sub(line_regex, new_line, file_string, count=count, flags=flags)   #DOTALL necessary for '.' character to match newline as well

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
		msg = 'could not regex pattern %s; string is:\n%s' % (pattern, string)
		raise RegexException, msg

	#return the last value to match patter
	return result[-1]


class RegexException(BaseException):
	pass


