
#ifndef NODE_H
#define NODE_H
#include <omp.h>

#include <stdlib.h>

#include <misc.h>

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

inline node_id_t get_node_num() {
        if ( my_node == -1 ) {
            fprintf(stderr,"WARNING : Uninitialized node num\n");
            return 666;
        }
        return my_node;
}

static inline int get_num_nodes() {
    if ( num_nodes == -1 ) {
        fprintf(stderr,"WARNING : Uninitialized num nodes\n");
        return 666;

    }
    return num_nodes;
}


extern __thread int thread_id;
inline int get_thread_num() {
    return thread_id;

}
inline void set_thread_num( int id) {
    thread_id = id;
}


extern int _num_threads;
static const int default_num_threads = 2;
inline int get_num_threads() {

    if ( _num_threads == -1  ) {
        char * fromomp = getenv("TSTAR_NUM_THREADS");

        _num_threads = fromomp==NULL?2:atoi(fromomp);
        CFATAL( _num_threads != 2, " Probably something wrong with OMP NUM THREADS ( != 2 here )");
    }
    return _num_threads;
}

#ifdef __cplusplus
}
#endif

#endif


