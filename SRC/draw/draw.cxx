
#include <sstream>
#include <cmath>
#include "graphics.h"
#include "draw_types.h"
#include "draw.h"
#include "exception.h"

using namespace std;

/**** File-Scope Globals ****/
t_draw_coords f_draw_coords;
Routing_Structs *f_routing_structs_ptr;
Arch_Structs *f_arch_structs_ptr;
User_Options *f_user_opts_ptr;

/**** Function Declarations ****/
/* the main drawing function */
static void drawscreen();
/* handles button presses by user */
static void handle_button_press(float, float, t_event_buttonPressed event_buttonPressed);
/* draws blocks */
static void drawplace();
/* draws the routing resources of the FPGA */
static void draw_rr(void);
/* returns color of the node based on its demand */
static t_color get_node_color(int node_ind, t_rr_node &rr_node);
/* draws the wire for the specified chanx/chany node */
static void draw_rr_chan(int node_ind, int track_index, t_color node_color, e_rr_type node_type);


/**** Function Definitions ****/

/* initializes coordinate system and launches the display */
void init_draw_coords(float clb_width, Routing_Structs *routing_structs, Arch_Structs *arch_structs){

	t_draw_coords* draw_coords = &f_draw_coords;

	int i;
	int j;

	/* get some variables */
	int num_types = arch_structs->get_num_block_types();
	t_block_type &type_descriptors = arch_structs->block_type;

	int grid_size_x, grid_size_y;
	arch_structs->get_grid_size(&grid_size_x, &grid_size_y);

	t_chanwidth &chan_width_x = arch_structs->chanwidth_x;
	t_chanwidth &chan_width_y = arch_structs->chanwidth_y;

	/* allocate tile x/y structs */
	draw_coords->alloc_tile_x_y(grid_size_x, grid_size_y);

	/* set pin size */
	draw_coords->tile_width = clb_width;
	draw_coords->pin_size = 0.3;
	for (i = 0; i < num_types; ++i) {
		int num_type_pins = type_descriptors[i].get_num_pins();
		if (num_type_pins > 0) {
			draw_coords->pin_size = min(draw_coords->pin_size,
					(draw_coords->get_tile_width() / (4.0F * num_type_pins)));
		}
	}

	/* set tile_x values */
	j = 0;
	for (i = 0; i < grid_size_x-1; i++) {
		draw_coords->tile_x[i] = (i * draw_coords->get_tile_width()) + j;
		j += chan_width_y[2][2] + 1; /* N wires need N+1 units of space */
	}
	draw_coords->tile_x[grid_size_x - 1] = ((grid_size_x-1) * draw_coords->get_tile_width()) + j;

	/* set tile_y values */
	j = 0;
	for (i = 0; i < grid_size_y-1; ++i) {
		draw_coords->tile_y[i] = (i * draw_coords->get_tile_width()) + j;
		j += chan_width_x[2][2] + 1;
	}
	draw_coords->tile_y[grid_size_y-1] = ((grid_size_y-1) * draw_coords->get_tile_width()) + j;

	/* initialize easygl coordinate system */
	init_world(
		0.0, 0.0,
		draw_coords->tile_y[grid_size_y-1] + draw_coords->get_tile_width(), 
		draw_coords->tile_x[grid_size_x-1] + draw_coords->get_tile_width()
	);
}

/* updates screen */
void update_screen(Routing_Structs *routing_structs, Arch_Structs *arch_structs, User_Options *user_opts){
	if (user_opts->nodisp == false){
		f_arch_structs_ptr = arch_structs;
		f_routing_structs_ptr = routing_structs;
		f_user_opts_ptr = user_opts;

		drawscreen();
		event_loop(handle_button_press, NULL, NULL, drawscreen);
	}
}

/* handles button presses by user */
static void handle_button_press(float x, float y, t_event_buttonPressed event_buttonPressed){

	int grid_size_x, grid_size_y;
	f_arch_structs_ptr->get_grid_size(&grid_size_x, &grid_size_y);
	t_grid &grid = f_arch_structs_ptr->grid;

	t_draw_coords* draw_coords = &f_draw_coords;


	/* want to display demand of nodes that are clicked on */

	/* first need to find coordinates of corresponding tile */
	float bottom_y, left_x;
	left_x = draw_coords->tile_x[0];
	bottom_y = draw_coords->tile_y[0];
	float tile_size = draw_coords->tile_x[1] - draw_coords->tile_x[0];
	float tile_width = draw_coords->get_tile_width();

	int tile_x, tile_y;
	tile_x = floor((x - left_x) / tile_size);
	tile_y = floor((y - bottom_y) / tile_size);

	/* now we need to determine if a routing wire was clicked, and if so, which one */
	float relative_x, relative_y;
	relative_x = x - draw_coords->tile_x[ tile_x ];	//relative to tile starting point
	relative_y = y - draw_coords->tile_y[ tile_y ];

	int chan_width = f_arch_structs_ptr->chanwidth_x[tile_x][tile_y];	/* assuming channel width is the same everywhere */
	float gap_size = tile_size - tile_width;
	float wire_region_size = gap_size / (float)(chan_width+1);


	if (relative_x <= tile_width && relative_y <= tile_width){
		/* click was inside the CLB */
		stringstream ss;
		ss << "CLB at coordinate (" << tile_x << "," << tile_y << ")";
		update_message(ss.str().c_str());
	} else if (relative_x <= tile_width && relative_y > tile_width){
		/* click is in horizontal channel */
		int wire_num = floor((relative_y - tile_width) / wire_region_size);
		if (wire_num < chan_width){
			int node_ind = f_routing_structs_ptr->rr_node_index[CHANX][tile_x][tile_y][wire_num];
			double node_demand = f_routing_structs_ptr->rr_node[node_ind].get_demand(f_user_opts_ptr);

			//cout << "x: " << tile_x << "  y: " << tile_y << "  wire num: " << wire_num << "  rr node: " << node_ind << endl; 

			/* display demand */
			stringstream ss;
			ss << "CHANX node: " << node_ind << "  demand: " << node_demand;
			update_message(ss.str().c_str());
		}
	} else if (relative_x > tile_width && relative_y <= tile_width){
		/* click is in vertical channel */
		int wire_num = floor((relative_x - tile_width) / wire_region_size);
		if (wire_num < chan_width){
			int node_ind = f_routing_structs_ptr->rr_node_index[CHANY][tile_x][tile_y][wire_num];
			double node_demand = f_routing_structs_ptr->rr_node[node_ind].get_demand(f_user_opts_ptr);

			/* display demand */
			stringstream ss;
			ss << "CHANY node: " << node_ind << "  demand: " << node_demand;
			update_message(ss.str().c_str());
		}
	} else {
		/* click is in the switch block region */
		stringstream ss;
		ss << "Switch block at coordinate (" << tile_x << "," << tile_y << ")";
		update_message(ss.str().c_str());
	}

}

/* the main drawing function */
static void drawscreen(){
	clearscreen();

	setfontsize(14);

	drawplace();
	draw_rr();
}

/* draws blocks */
static void drawplace(){
	
	/* get some variables */
	int grid_size_x, grid_size_y;
	f_arch_structs_ptr->get_grid_size(&grid_size_x, &grid_size_y);
	t_grid &grid = f_arch_structs_ptr->grid;

	t_draw_coords* draw_coords = &f_draw_coords;


	/* now draw */
	int i, j;

	setlinewidth(0);

	for (i = 1; i < grid_size_x-1; i++) {
		for (j = 1; j < grid_size_y-1; j++) {
			/* Only the first block of a group should control drawing */
			if (grid[i][j].get_width_offset() > 0 || grid[i][j].get_height_offset() > 0) 
				continue;

			int type_index = grid[i][j].get_type_index();


			/* Get coords of block */
			//TODO: this will return the dimensions of a clb, not the size of a multi-height/width block
			t_bound_box abs_clb_bbox = draw_coords->get_absolute_clb_bbox(i,j);

			/* Fill background for the block */
			setcolor(LIGHTGREY);
			fillrect(abs_clb_bbox);

			setcolor(BLACK);
			drawrect(abs_clb_bbox);

			/* Draw text for block type so that user knows what block */
			if (grid[i][j].get_width_offset() == 0 && grid[i][j].get_height_offset() == 0) {
				if (i > 0 && i < grid_size_x-1 && j > 0 && j < grid_size_y-1) {
					drawtext(abs_clb_bbox.get_center() - t_point(0, abs_clb_bbox.get_width()/4),
							f_arch_structs_ptr->block_type[type_index].get_name().c_str(), abs_clb_bbox);
				}
			}
		}
	}
}



/* draws the routing resources of the FPGA */
static void draw_rr(void) {

	/* get some variables */
	t_rr_node &rr_node = f_routing_structs_ptr->rr_node;
	int num_rr_nodes = f_routing_structs_ptr->get_num_rr_nodes();

	setlinestyle(SOLID);

	/* draw nodes and their edges */
	for (int inode = 0; inode < num_rr_nodes; inode++) {
		int itrack;

		t_color node_color = get_node_color(inode, rr_node);

		/* Now call drawing routines to draw the node. */
		switch (rr_node[inode].get_rr_type()) {
			case SOURCE:
			case SINK:
				break; /* Don't draw. */

			case CHANX:
				itrack = rr_node[inode].get_ptc_num();
				draw_rr_chan(inode, itrack, node_color, CHANX);
				//draw_rr_edges(inode);
				break;

			case CHANY:
				itrack = rr_node[inode].get_ptc_num();
				draw_rr_chan(inode, itrack, node_color, CHANY);
				//draw_rr_edges(inode);
				break;

			case IPIN:
				//draw_rr_pin(inode, draw_state->draw_rr_node[inode].color);
				break;

			case OPIN:
				//draw_rr_pin(inode, draw_state->draw_rr_node[inode].color);
				//draw_rr_edges(inode);
				break;

			default:
				WTHROW(EX_OTHER, "Unexpected rr_node type: " << rr_node[inode].get_rr_type_string());
		}
	}
}

/* returns color of the node based on its demand */
static t_color get_node_color(int node_ind, t_rr_node &rr_node){
	t_color color(BLACK);	//TODO. provisional

	if (rr_node[node_ind].highlight){
		color = t_color(RED);
		//setlinewidth(3);
		
	} else {
		double demand = rr_node[node_ind].get_demand(f_user_opts_ptr);

		if (demand <= 0){
			color = t_color(BLACK);
		} else if (demand < 0.10){
			color = t_color(0, 255, 230);	//teal
		} else if (demand < 0.20){
			color = t_color(0, 255, 180);
		} else if (demand < 0.30){
			color = t_color(0, 255, 119);
		} else if (demand < 0.40){
			color = t_color(26, 255, 0);
		} else if (demand < 0.50){
			color = t_color(80, 255, 0);	//green
		} else if (demand < 0.60){
			color = t_color(137, 255, 0);
		} else if (demand < 0.70){
			color = t_color(179, 255, 0);
		} else if (demand < 0.80){
			color = t_color(200, 255, 0);
		} else if (demand < 0.90){
			color = t_color(255, 255, 0);	//yellow
		} else if (demand < 1.0){
			color = t_color(255, 205, 0);	//darkyellow
		} else if (demand < 1.1){
			color = t_color(255, 190, 0);
		} else if (demand < 1.2){
			color = t_color(255, 170, 0);
		} else if (demand < 1.3){
			color = t_color(255, 150, 0);
		} else if (demand < 1.4){
			color = t_color(255, 125, 0);
		} else if (demand < 1.5){
			color = t_color(255, 100, 0);
		} else if (demand < 1.6){
			color = t_color(255, 75, 0);
		} else if (demand < 1.7){
			color = t_color(255, 50, 0);
		} else if (demand < 1.8){
			color = t_color(255, 25, 0);
		} else {
			color = t_color(255, 0, 0);	//red
		}
	}

	return color;
}

/* draws the wire for the specified chanx/chany node */
static void draw_rr_chan(int node_ind, int track_index, t_color node_color, e_rr_type node_type){
	
	//TODO: this is a provisional function. a placeholder until i get a chance to keep porting VPR's draw routines

	t_draw_coords *draw_coords = &f_draw_coords;
	t_rr_node &rr_node = f_routing_structs_ptr->rr_node;
	
	/* get xlow/xhigh/ylow/high */
	int xlow, xhigh, ylow, yhigh;
	xlow = rr_node[node_ind].get_xlow();
	xhigh = rr_node[node_ind].get_xhigh();
	ylow = rr_node[node_ind].get_ylow();
	yhigh = rr_node[node_ind].get_yhigh();

	/* assume tiles are the same dimension in x and y directions and channel widths are the same everywhere*/
	float clb_width = draw_coords->get_tile_width();
	int wires_in_chan = f_arch_structs_ptr->chanwidth_x[2][2];
	float tile_span = draw_coords->tile_x[2] - draw_coords->tile_x[1];
	float channel_span = tile_span - clb_width;
	float track_width = channel_span / (float)(wires_in_chan+1);

	/* get coordinates of wire */
	float x1, x2, y1, y2;
	if (node_type == CHANX){
		y1 = y2 = track_width * track_index + draw_coords->tile_y[ylow] + clb_width + 1;
		x1 = draw_coords->tile_x[xlow];
		x2 = draw_coords->tile_x[xhigh] + clb_width;
	} else if (node_type == CHANY){
		x1 = x2 = track_width * track_index + draw_coords->tile_x[xlow] + clb_width + 1;
		y1 = draw_coords->tile_y[ylow];
		y2 = draw_coords->tile_y[yhigh] + clb_width;
	} else {
		WTHROW(EX_OTHER, "Node not CHANX or CHANY");
	}

	setlinewidth(3);
	setcolor(node_color);
	drawline(x1, y1, x2, y2);	
}
