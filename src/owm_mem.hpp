#ifndef OWM_MEM_H
#define OWM_MEM_H

#include <frame.hpp>
#include <mapper.h>
#include <delegator.hpp>
#include <invalidation.hpp>
#include <network.hpp>


static inline void * owm_malloc( size_t size ) {
	// Allocates in private segment for data + header.
	struct owm_frame_layout * retval =  (struct owm_frame_layout*) mapper_malloc( size + sizeof(struct owm_frame_layout) );
	retval->size = size;
        retval->proto_status = RESP;
        retval->usecount = 1;
        retval->next_resp = 0;
        retval->canari = CANARI;
        retval->canari2 = CANARI;
        retval->reserved = false;
        CFATAL( (intptr_t) retval->data - (intptr_t) retval != sizeof(struct owm_frame_layout), "Layout invalid.");
	return retval->data;
}

static inline void owm_free_local( void * page ) {
        // Error if not called locally.
        CFATAL( mapper_who_owns(page) != get_node_num(), "Attempting to free pointer in a different location." );
        ask_or_do_invalidation_then( page, NULL);
        mapper_free( GET_FHEADER(page) );
}

static inline void owm_free( void * page ) {
    if (mapper_who_owns(page) == get_node_num() ) {
        // Free locally.
        DEBUG( "Freeing : %p, local pointer, done locally.",page);
        owm_free_local(page);
    } else {
        DEBUG( "Freeing : %p, global pointer, sending message to %d.",page, PAGE_GET_NEXT_RESP(page));
        NetworkInterface::send_free_message(PAGE_GET_NEXT_RESP(page), page);
    }

}



#endif
