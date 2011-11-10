#ifndef OWM_MEM_H
#define OWM_MEM_H

#include <frame.hpp>
#include <mapper.h>
#include <delegator.hpp>



// From invalidation.h 
extern void ask_or_do_invalidation( void *page );


void * owm_malloc( size_t size ) {
	// Allocates in private segment for data + header.
	struct owm_frame_layout * retval =  (struct owm_frame_layout*) mapper_malloc( size + sizeof(struct owm_frame_layout) );
	retval->size = size;
	int proto_status = RESP;
	
	return retval->data;
}

void owm_free( void * page ) {
	ask_or_do_invalidation( page); // serial == 0, NULL continuation
	// Use continuation here ?
	mapper_free( GET_FHEADER(page) );
}



#endif
