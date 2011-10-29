

#ifndef ALLOCATOR_H
#define ALLOCATOR_H


void * sharedallocator_malloc( size_t size );


int sharedallocator_free( void * addr );


#endif
