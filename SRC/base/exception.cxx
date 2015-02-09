#include <string>
#include <sstream>
#include "exception.h"

using namespace std ;

/* This must match e_ex_type list */
const string ex_type_string[NUM_EXCEPTIONS]{
	"EX_OTHER",
	"EX_INIT",
	"EX_ARCH",
	"EX_GRAPH",
	"EX_PATH_ENUM"
	};



/**** Class Function Definitions ****/

/*==== Wotan_Exception Class ====*/
/* Overload the << operator for the base exception class */
ostream& operator<<(ostream &os, const Wotan_Exception& ex){
	os << "\nEXCEPTION " << ex_type_string[ex.type] << ": In " << ex.file << " on line " << ex.line << ". Message:\n" << ex.message << endl;
	return os;
}
/*=== End Wotan_Exception Class ===*/
