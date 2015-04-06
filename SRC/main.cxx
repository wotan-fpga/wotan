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




///************
//Goal: provide a minimal communication interface for daughterboards.
//Ideally one easy 'interface' for transmit and receive
//************/
//
///* create a data struct/class for each board */
//struct s_imu_data{
//	//data
//};
//
//struct s_pyro_data{
//	//data
//};
////other board structs.
////TODO: do you need separate board structs for arbiter transmitting data vs receiving? i.e. can you re-use s_pyro_data for both arbiter-->pyro command
////and pyro-->arbiter status reports?
//
//
///* example function to transmit specified number of bytes based on void data pointer */
//void transmit_I2C_slave(size_t data_bytes, void* data){
//	
//	/* get pointer to data */
//	uint8_t *data_ptr = (uint8_t*)data;
//
//	/* walk along pointer and transmit data. can compute CRC as you go? */
//	for (uint8_t ibyte = 0; ibyte < data_size; ibyte++){
//		uint8_t byte = data_ptr[ibyte];
//		wire_transmit( byte );
//	}
//
//	//transmit CRC byte?
//}
//
///* how you'd call the above function */
//transmit_I2C_slave(sizeof(s_pyro_data), (void*)my_pyro_data);
//
////TODO: what if you want to poll a board as a master?
////Can you combine slave transmit and master poll into one function, or should you use two separate functions?
////What happens if master is polling a daughterboard but daughterboard doesn't respond? Should that poll time-out be specified as a parameter to the function or does the
////onus of the timeout check fall on the piece of code calling this function?
//
////TODO: how would you receive data via the Wire interrupt?
////...
////OK, say your interrupt has received all the data into an array of some sort. You type-cast it back into the appropriate struct (i.e. (s_pyro_data*)some_void_ptr).
////How do you notify the program that a new command has been received? i.e. do you expect the main program loop to occasionally check some flag to see if it has data pending? something else?
////Basically the comms interface should be robust and should require minimal effort from the daughterboard programmer.

