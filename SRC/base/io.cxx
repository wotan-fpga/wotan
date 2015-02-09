
#include <fstream>
#include "io.h"
#include "exception.h"

using namespace std;


/**** Function Definitions ****/
/* compares extension of specified filename with specified string
*  returns true if they are the same, false otherwise */
bool check_file_extension(string &filename, string extension){

	bool result = false;
	int ext_len = extension.length();
	int file_len = filename.length();

	/* check if either string is empty */
	if ( 0 == file_len || 0 == ext_len ){ 
		result = false;
	} else if (file_len <= ext_len){
		result = false;
	} else {
		string file_ext = filename.substr( file_len - ext_len, file_len - 1 );
		if ( file_ext.compare( extension ) == 0 ){
			/* compare returns 0 if the two strings are the same */
			result = true;
		} else {
			result = false;
		}
	}
	return result;
}

/* open file with error checking. 'mode' is ios::in, ios::out, or (ios::in | ios::out)
   ios::in is used to write things to file, ios::out is for read. if opening for write
   and want to clear contents, also do ios::trunc */
void open_file(fstream *file, string path, ios_base::openmode mode){
	if (NULL == file){
		WTHROW(EX_OTHER, "Specified file pointer is null!");
	}	

	file->close();
	file->open(path.c_str(), mode);
	if (file->is_open() && file->good()){
		//cout << "Opened file: " << path << endl;
	} else {
		WTHROW(EX_OTHER, "Could not open file: " << path);
		
	}
}

