
#ifndef MAPPER_H
#define MAPPER_H

#include <stdlib.h>
#include <misc.h>
#ifdef __cplusplus
extern "C" {
#endif

#if DEBUG_ADDITIONAL_CODE
void mapper_valid_address_rm( void * ptr);
void mapper_valid_address_check( void * ptr );
void mapper_valid_address_add( void * ptr );

#endif

extern void mapper_initialize_address_space ( void * base, size_t len_pernode, int _nnodes, int nthreads) ;

extern int mapper_node_who_owns( void * ptr );

// Allocates in node local segment.
extern void * mapper_malloc(size_t len);

// Allocates in node local segment.
extern void mapper_free( void * zone );
extern void mapper_check_sanity();

#ifdef __cplusplus
}
#endif

#endif

