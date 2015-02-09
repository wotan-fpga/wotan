#ifndef WOTAN_INIT_H
#define WOTAN_INIT_H

#include "wotan_types.h"


/**** Function Declarations ****/
/* Reads in user options and initializes the tool */
void wotan_init(int argc, char **argv, User_Options *user_opts, Arch_Structs *arch_structs, Routing_Structs *routing_structs,
                Analysis_Settings *analysis_settings);



#endif /* WOTAN_INIT_H */
