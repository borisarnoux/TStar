
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


inline int get_thread_num() {

    return omp_get_thread_num();

}


static int _threads_tmp = -1;
inline int get_num_threads() {

    // TODO : work on this...
    // Because this function is used outside omp parallel blocks,
    // we need to give the right result... ( 2 here ).
    if ( _threads_tmp == -1  ) {
        char * fromomp = getenv("OMP_NUM_THREADS");

        _threads_tmp = fromomp==NULL?2:atoi(fromomp);
        CFATAL( _threads_tmp != 2, " Probably something wrong with OMP NUM THREADS ( != 2 here )");
    }
    return _threads_tmp;
}

#ifdef __cplusplus
}
#endif

#endif


