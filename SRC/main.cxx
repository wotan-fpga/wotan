#include <iostream>
#include <ctime>
#include <string>
#include "io.h"
#include "exception.h"
#include "wotan_init.h"
#include "wotan_types.h"
#include "globals.h"
#include "analysis_main.h"
#include "wotan_cleanup.h"

#include <sstream>

using namespace std;


int main(int argc, char **argv){
	
	User_Options user_opts;	
	Arch_Structs arch_structs;
	Routing_Structs routing_structs;
	Analysis_Settings analysis_settings;

	try{
		clock_t begin_time = clock();		

		/* initialize structures */
		wotan_init(argc, argv, &user_opts, &arch_structs, &routing_structs, &analysis_settings);

		/* perform path enumeration and/or probability analysis */
		run_analysis(&user_opts, &analysis_settings, &arch_structs, &routing_structs);

		/* clean up */
		free_wotan_structures(&arch_structs, &routing_structs);

		clock_t end_time = clock();
		double elapsed_sec = double(end_time - begin_time) / CLOCKS_PER_SEC;

		cout << "Execution took " << elapsed_sec << " seconds" << endl;

	} catch (Wotan_Exception &ex){
		cout << ex;
	}
	return 0;
}

