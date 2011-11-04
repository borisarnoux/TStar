#ifndef DFRT_MISC_H
#define DFRT_MISC_H


#include <config.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <syscall.h>
#include <unistd.h>
#include <omp.h>
#include <node.h>

#include <futex.h>
#ifdef __cplusplus
extern "C" {
#endif

#define exit_group(a) syscall(SYS_exit_group, a)

#define CONCAT(A,B) A##B
#define CONCAT2(A,B) A##B
#define CONCAT3(A,B,C) A##B##C


#define __packed __attribute__((packed))
#define hidden_attr __attribute__((visibility("hidden")))
#define CFATAL( C, msg, ...  ) \
    do {if ( C ) { fprintf(stderr, "%s:%d, from (%d:%d) " msg "\n",__FILE__, __LINE__, get_node_num(), omp_get_thread_num(),##__VA_ARGS__); puts((char*)1); exit(EXIT_FAILURE);} } while (0)
#define ASSERT( C ) CFATAL(!(C), "Assertion failed.")
#define FATAL( msg, ... ) CFATAL( 1, "FATAL::" msg, ##__VA_ARGS__)

#ifndef MUTE
#define DEBUG( msg, ...  ) \
    do { fprintf(stderr, "%s:%d, from (%d:%d) " msg " \n",__FILE__, __LINE__, get_node_num(), omp_get_thread_num(), ##__VA_ARGS__);} while (0)
#else
#define DEBUG(msg, ...)
#endif


#ifdef VERY_VERBOSE
#define __DEBUG(msg, ... ) DEBUG(msg, ##__VA_ARGS__)
#else
#define __DEBUG(msg, ... )
#endif

#if defined(VERBOSE) || defined(VERY_VERBOSE)
#define _DEBUG(msg, ... ) DEBUG(msg, ##__VA_ARGS__)
#else
#define _DEBUG(msg, ... )
#endif

#define max(a,b) ( (a)>(b)?(a):(b))


#define _loc_dbg(x,...) fprintf( stderr, x "\n", ##__VA_ARGS__)
#define SET_CANARIS(h) do { h->canari1 = 0x42; h->canari2 = 0x43; } while (0)
#define CHECK_CANARIS(h) do { \
    if ( h->canari1 != 0x42 || h->canari2 != 0x43 ) {\
        _loc_dbg( "Canari 1 : %hx", h->canari1);\
        _loc_dbg( "to_notice : %lx", h->field_nodes_to_notice);\
        _loc_dbg( "Canari 2 : %hx", h->canari2);\
        _loc_dbg( "counter : %d", h->counter);\
        _loc_dbg( "perm : %hx", h->perm);\
        _loc_dbg( "owners_field : %lx", h->owners_field);\
        _loc_dbg( "size : %lu", (unsigned long)h->size );\
        struct frame_struct * z = (struct frame_struct * ) ((intptr_t)h + sizeof(struct page_hdr));\
        _loc_dbg("Frame infos name : %s", z->infos->misc!= NULL ? z->infos->misc->fname:NULL);\
        _loc_dbg("Frame sc : %d", z->sc );\
        _loc_dbg("Frame ret field : %p", z->ret[0]);\
        FATAL(" Invalid canaries.");\
    }} while (0)


#ifdef __cplusplus
}
#endif


#endif /*Guard*/
