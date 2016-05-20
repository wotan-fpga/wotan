#include "globals.h"
#include <pthread.h>

using namespace std;

/* Contains user options for the tool */

pthread_mutex_t g_mutex;

float g_conns_enumerated = 0;
float g_enum_nodes_popped = 0;
float g_prob_nodes_popped = 0;
