#ifndef OWM_MEM_H
#define OWM_MEM_H

#include <frame.hpp>
#include <mapper.h>
#include <delegator.hpp>
#include <invalidation.hpp>



static inline void * owm_malloc( size_t size ) {
	// Allocates in private segment for data + header.
	struct owm_frame_layout * retval =  (struct owm_frame_layout*) mapper_malloc( size + sizeof(struct owm_frame_layout) );
	retval->size = size;
	int proto_status = RESP;
	
	return retval->data;
}

static inline void owm_free( void * page ) {
        ask_or_do_invalidation_then( page, NULL); // serial == 0, NULL continuation
	// Use continuation here ?
	mapper_free( GET_FHEADER(page) );
}



#endif
