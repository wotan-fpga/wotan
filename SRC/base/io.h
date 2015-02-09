#ifndef WOTAN_IO_H
#define WOTAN_IO_H


#include <iostream>
#include <string>
#include <sstream>
#include "wotan_types.h"

/**** Defines ****/


/**** Enumerations ****/

/**** Function Declarations ****/
/* returns true if extension of 'filename' matches 'extension' */
bool check_file_extension(std::string &filename, std::string extension);
/* function for opening files, with error checking */
void open_file(std::fstream *file, std::string path, std::ios_base::openmode mode);

#endif
