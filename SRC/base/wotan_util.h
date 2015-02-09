#ifndef WOTAN_UTIL_H
#define WOTAN_UTIL_H

#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <set>

/**** Classes ****/

/* represents a coordinate on a grid */
//FIXME: do I need this? Probably not.
class Coordinate{
public:
	int x;
	int y;
	Coordinate(){
		x=y=0;
	}
	Coordinate(int s_x, int s_y){
		x=s_x; y=s_y;
	}
	/* Sets coordinate */
	void set(int s_x, int s_y);
	/* gets hypotenuse size between this and another coordinate */
	double get_dist(const Coordinate &obj);
	/* returns the absolute difference between x coordinates plus the absolute difference between the y coordinates i.e. dx+dy */
	int get_dx_plus_dy(const Coordinate &obj);
	int get_dx_plus_dy(int x2, int y2);
	/* defined for purposes of indexing into an std::map */
	bool operator < (const Coordinate &obj) const;
	/* compares this and another coordinate */
	bool operator== (const Coordinate &obj) const;
};
std::ostream& operator<<(std::ostream &os, const Coordinate &coord);


/* A fixed-weight (aka bounded-height) priority queue. Only accepts elements that have a weight up to (and including)
   the maximum, as set in the constructor / the set_max_weight function.
   The weight of an object being pushed-in is given to the 'push' function alongside the object.
   Having a queue of a fixed weight allows push/top operation to have complexity of O(1) and the
   pop operation to have a complexity of O(max_weight) */
template <typename T> class My_Bounded_Priority_Queue{
private:
	std::vector< std::queue<T> > my_pq;	/* will implement fixed-size priority queue */
	int max_weight;				/* the fixed size of the priority queue */
	int current_lowest_weight;		/* lowest-weight at which an object exists*/
	int num_objects;			/* number of objects in priority queue */
public:

	My_Bounded_Priority_Queue();
	My_Bounded_Priority_Queue(int max_w);

	/* sets maximum weight */
	void set_max_weight(int max_w);

	/* push, pop, top */
	void push(T object, int weight);
	void pop();
	const T& top() const;

	/* returns weight of top (lowest-weight) node */
	int top_weight() const;

	/* # of entries in priority queue */
	int size() const;

	/* clears entire priority queue */
	void clear();
};

/* A fixed size priority queue. Can contain up to the number of objects
   as specified to the constructor. If the priority queue
   exceeds the maximum number of objects, the object at the TOP of the queue is kicked out
   (for instance, if queue is sorted in ascending order, then this is the LARGEST object) */
template <typename T, typename S> class My_Fixed_Size_PQ{
private:
	int max_objects;		/* maximum number of objects that can be stored in the queue */
	std::priority_queue<T, std::vector<T>, S> pq;
	bool ascending;			/* if true, objects are sorted in ascending order, descending otherwise */
public:

	My_Fixed_Size_PQ();
	My_Fixed_Size_PQ(int set_max_objs);
	void set_properties(int set_max_objs);

	/* some standard functions for this type of container */
	void push(T object);
	void pop();
	const T& top() const;
	int size() const;
	void clear();
};

/**** Function Declarations ****/
/* specifies whether the string contains the given substring */
bool contains_substring(std::string str, std::string substr);

/* ORs two independent probability numbers */
template <typename T> T or_two_probs(T p1, T p2);

#endif
