#ifndef DFRT_MISC_H
#define DFRT_MISC_H


#include <config.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
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



#ifdef __cplusplus
}
#endif


#endif /*Guard*/
