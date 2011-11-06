
#ifndef MAPPER_H
#define MAPPER_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void mapper_initialize_address_space ( void * base, size_t len_pernode, int _nnodes ) ;

extern int mapper_who_owns( void * ptr );

// Allocates in node local segment.
extern void * mapper_malloc(size_t len);

// Allocates in node local segment.
extern void mapper_free( void * zone );

#ifdef __cplusplus
}
#endif

#endif

