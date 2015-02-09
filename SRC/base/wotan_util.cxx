
#include <cmath>
#include <utility>
#include <functional>
#include "wotan_util.h"
#include "exception.h"
#include "wotan_types.h"

using namespace std;


/**** Function Definitions ****/
/* specifies whether the string contains the given substring */
bool contains_substring(std::string str, std::string substr){
	bool result = false;
	
	if ( str.find(substr) != string::npos ){
		result = true;
	}

	return result;
}

/* ORs two independent probability numbers */
template <typename T> T or_two_probs(T p1, T p2){
	T result;

	result = p1 + p2 - p1*p2;

	return result;
}
/* only defined for float/double types */
template float or_two_probs(float, float);
template double or_two_probs(double, double);


/**** Class Function Definitions ****/

/*==== Coordinate Class====*/
/* Sets coordinate */
void Coordinate::set(int s_x, int s_y){
	x=s_x; y=s_y;
}
/* gets hypotenuse size between this and another coordinate */
double Coordinate::get_dist(const Coordinate &obj){
	double dist = std::sqrt( std::pow((double)(x-obj.x),2.0) + std::pow((double)(y-obj.y),2.0) );
	return dist;
}
/* returns the absolute difference between x coordinates plus the absolute difference between the y coordinates i.e. dx+dy */
int Coordinate::get_dx_plus_dy(const Coordinate &obj){
	int dx_plus_dy = abs(x - obj.x) + abs(y - obj.y);
	return dx_plus_dy;
}
int Coordinate::get_dx_plus_dy(int x2, int y2){
	int dx_plus_dy = abs(x - x2) + abs(y - y2);
	return dx_plus_dy;
}
/* defined for purposes of indexing into an std::map */
bool Coordinate::operator < (const Coordinate &obj) const{
	bool result;

	if (x < obj.x){
		result = true;
	} else {
		if (x == obj.x){
			if (y < obj.y){
				result = true;
			} else {
				result = false;
			}
		} else {
			result = false;
		}
	}
	return result;
}
/* compares this and another coordinate */
bool Coordinate::operator== (const Coordinate &obj) const{
	bool result;
	if (x == obj.x && y == obj.y){
		result = true;
	} else {
		result = false;
	}
	return result;
}
/* For printing Coordinate class */
std::ostream& operator<<(std::ostream &os, const Coordinate &coord){
	os << "(" << coord.x << "," << coord.y << ")";
	return os;
}
/*=== End Coordinate Class ===*/


/*=== My_Bounded_Priority_Queue Class ===*/

template <typename T> My_Bounded_Priority_Queue<T>::My_Bounded_Priority_Queue(){
	this->max_weight = UNDEFINED;
	this->current_lowest_weight = UNDEFINED;
	this->num_objects = 0;
}

template <typename T> My_Bounded_Priority_Queue<T>::My_Bounded_Priority_Queue(int max_w){
	this->max_weight = UNDEFINED;
	this->current_lowest_weight = UNDEFINED;
	this->num_objects = 0;

	this->set_max_weight(max_w);
}

/* sets maximum weight */
template <typename T> void My_Bounded_Priority_Queue<T>::set_max_weight(int max_w){
	if (max_weight == UNDEFINED){
		this->max_weight = max_w;
	
		/* size the priority queue.
		   priority queue will have weight 0..max_w */
		this->my_pq.assign(max_w + 1, queue<T>());
	} else {
		WTHROW(EX_OTHER, "Not allowing re-sizing of bounded-height priority queue. May be implemented later.");
	}
}

/* push object of specified weight to queue */
template <typename T> void My_Bounded_Priority_Queue<T>::push(T object, int weight){
	if (weight > this->max_weight || weight < 0){
		WTHROW(EX_OTHER, "Object pushed into bounded-height priority queue has weight outside 0..max_weight");
	}

	this->my_pq[weight].push( object );
	this->num_objects++;

	/* update current lowest weight */
	if (this->current_lowest_weight == UNDEFINED || weight < this->current_lowest_weight){
		this->current_lowest_weight = weight;
	}
}

/* pop lowest-weight object from queue */
template <typename T> void My_Bounded_Priority_Queue<T>::pop(){
	if (this->current_lowest_weight != UNDEFINED){
		this->my_pq[this->current_lowest_weight].pop();
		this->num_objects--;

		int objects_at_current_lowest = (int)this->my_pq[this->current_lowest_weight].size();

		if (objects_at_current_lowest == 0){
			if (this->num_objects == 0){
				this->current_lowest_weight = UNDEFINED;
			} else {
				/* must search for the next current lowest weight */
				for (int iweight = this->current_lowest_weight; iweight <= this->max_weight; iweight++){
					int num_objs = (int)this->my_pq[iweight].size();

					if (num_objs > 0){
						this->current_lowest_weight = iweight;
						break;
					}
				}
			}
		}
	}
}

/* gets the top (lowest-weight) element */
template <typename T> const T& My_Bounded_Priority_Queue<T>::top() const{

	if (this->current_lowest_weight == UNDEFINED){
		WTHROW(EX_OTHER, "Called top on empty bounded-height priority queue");
	}

	const T &obj = this->my_pq[this->current_lowest_weight].front();

	return obj;
}

/* returns weight of top (lowest-weight) node */
template <typename T> int My_Bounded_Priority_Queue<T>::top_weight() const{
	return this->current_lowest_weight;
}

/* # of entries in priority queue */
template <typename T> int My_Bounded_Priority_Queue<T>::size() const{
	return this->num_objects;
}

/* clears entire priority queue */
template <typename T> void My_Bounded_Priority_Queue<T>::clear(){
	this->max_weight = UNDEFINED;
	this->current_lowest_weight = UNDEFINED;	
	this->num_objects = UNDEFINED;
}

/* IMPORTANT: the bounded-height priority queue will only work for types explicitely specified in below templates */
template class My_Bounded_Priority_Queue<int>;
/*=== END My_Bounded_Priority_Queue Class ===*/



/*=== My_Fixed_Size_PQ Class ===*/

template <typename T, typename S> My_Fixed_Size_PQ<T,S>::My_Fixed_Size_PQ(){
	this->set_properties(UNDEFINED);
}

template <typename T, typename S> My_Fixed_Size_PQ<T,S>::My_Fixed_Size_PQ(int set_max_objs){
	this->set_properties(set_max_objs);
}

/* sets maximum number of objects, as well as whether to sort in ascending or descending order */
template <typename T, typename S> void My_Fixed_Size_PQ<T,S>::set_properties(int set_max_objs){
	this->max_objects = set_max_objs;
}

/* pushes specified object onto priority queue. if priority queue exceeds its maximum size limit, 
   get rid of object at the back of the queue */
template <typename T, typename S> void My_Fixed_Size_PQ<T,S>::push( T object ){
	this->pq.push( object );

	int pq_size = this->size();
	if (pq_size > this->max_objects){
		/* kick off object at the TOP of the queue */
		this->pq.pop();
	}
}

/* returns reference to object at the front of the queue */
template <typename T, typename S> const T& My_Fixed_Size_PQ<T,S>::top() const{
	const T &result = this->pq.top();
	return result;
}

/* pops object at front of the queue */
template <typename T, typename S> void My_Fixed_Size_PQ<T,S>::pop(){
	this->pq.pop();
}

/* returns number of elements in this fixed-size priority queue */
template <typename T, typename S> int My_Fixed_Size_PQ<T,S>::size() const{
	int result = (int)this->pq.size();
	return result;
}

/* clears all contents */
template <typename T, typename S> void My_Fixed_Size_PQ<T,S>::clear(){
	this->pq = priority_queue<T, vector<T>, S>();	//reset the priority queue
}

/* IMPORTANT: My_Fixed_Size_PQ will only work for types explicitely specified in below templates */
template class My_Fixed_Size_PQ<float, less<float> >;
template class My_Fixed_Size_PQ<float, greater<float> >;
/*=== END My_Fixed_Size_PQ Class ===*/


/**** END Class Function Definitions ****/

