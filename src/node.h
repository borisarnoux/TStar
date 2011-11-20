
#ifndef NODE_H
#define NODE_H
#include <omp.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef  int node_id_t;

extern node_id_t my_node;
extern int num_nodes;

static void set_node_num(node_id_t nodenum) {
    my_node = nodenum;
}

static void set_num_nodes( int numnodes ) {
    num_nodes = numnodes;
}

inline int get_num_nodes() {
    return num_nodes;
}

inline node_id_t get_node_num() {
	return my_node;
}

inline int get_thread_num() {
    return omp_get_thread_num();

}

inline int get_num_threads() {
    return omp_get_num_threads();
}

#ifdef __cplusplus
}
#endif

#endif


