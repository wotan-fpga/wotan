#ifndef DRAW_H
#define DRAW_H

#include "graphics.h"
#include "wotan_types.h"

/**** Function Declarations ****/
/* initializes coordinate system */
void init_draw_coords(float clb_width, Routing_Structs *routing_structs, Arch_Structs *arch_structs);

/* updates screen */
void update_screen(Routing_Structs *routing_structs, Arch_Structs *arch_structs, User_Options *user_opts);



#endif
