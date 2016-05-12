#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <iostream>
#include <sstream>

/**** Defines ****/

/* Used for throwing exceptions. Auto-includes file and line number. Us as:
	WTHROW(EX_ARCH, "some text" << my_obj << 123 << endl); */
#define WTHROW(ex_type, message) do {std::stringstream exss; exss << message;			\
				Wotan_Exception ex(exss.str(), __FILE__, __LINE__, ex_type);		\
				throw ex;} while(false) 

/**** Enums ****/
/* enumerate various exceptions. has to match ex_type_string variable */
enum e_ex_type{
	EX_OTHER = 0,		/* miscellaneous */
	EX_INIT,		/* during initialization */
	EX_ARCH,		/* architecture file */
	EX_GRAPH,		/* rr graph */
	EX_PATH_ENUM,		/* path enumeration */
	NUM_EXCEPTIONS
};
extern const std::string ex_type_string[NUM_EXCEPTIONS];

/* Classes */
/* Basic exception class. Includes message, file name, and line number */
class Wotan_Exception{
	public:
	std::string message;
	int line;
	std::string file;
	e_ex_type type;

	/* Couple of constructors for different params */
	Wotan_Exception(char *m, char *f, int l, e_ex_type t){
		std::stringstream ss_m, ss_f;
		ss_m << m; ss_f << f;
		message = ss_m.str();
		file = ss_f.str();
		line = l;
		type = t;
	}
	Wotan_Exception(std::string m, char *f, int l, e_ex_type t){
		std::stringstream ss_f;
		ss_f << f;
		message = m;
		file = ss_f.str();
		line = l;
		type = t;
	}

	std::string what(){
		return this->message;
	}
};
/* overlaod << operator for use by cout */
std::ostream& operator<<(std::ostream &os, const Wotan_Exception& ex);

#endif /*EXCEPTION_H*/
