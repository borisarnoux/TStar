#ifndef OWM_MEM_H
#define OWM_MEM_H

#include <frame.hpp>
#include <mapper.h>
#include <delegator.hpp>
#include <invalidation.hpp>
#include <network.hpp>


static inline void * owm_malloc( size_t size ) {
	// Allocates in private segment for data + header.
        struct owm_frame_layout * retval =
                (struct owm_frame_layout*) mapper_malloc( size +
                                                          sizeof(struct owm_frame_layout)
                                          #ifdef DEBUG_ADDITIONAL_CODE
                                                          + sizeof(int) // For the canari at the end.
                                          #endif
                                                          );
#ifdef DEBUG_ADDITIONAL_CODE
        mapper_valid_address_add( retval->data );
#endif
	retval->size = size;
        retval->proto_status = RESP;
        retval->usecount = 1;
        retval->next_resp = 0;
        retval->canari = CANARI;
        retval->canari2 = CANARI;
        retval->reserved = false;
        retval->freed = 0;

        CFATAL( (intptr_t) retval->data - (intptr_t) retval != sizeof(struct owm_frame_layout), "Layout invalid (packing issue).");
        CHECK_CANARIES( retval->data );
        DEBUG("Allocated : page %p", retval->data);

#if DEBUG_ADDITIONAL_CODE
        int * ending_canari = (int*)((intptr_t)retval->data+size);
        *ending_canari = CANARI;
#endif
	return retval->data;
}

static inline void owm_free_local( void * page ) {
        // Error if not called locally.
        CFATAL( mapper_who_owns(page) != get_node_num(), "Attempting to free pointer in a different location." );
        CHECK_CANARIES(page);
#if DEBUG_ADDITIONAL_CODE
        int * ending_canari = (int*)((intptr_t)page + (intptr_t)(GET_FHEADER(page))->size);
        CFATAL( *ending_canari != CANARI, "Modified ending canari !!");
#endif
        if ( GET_FHEADER(page)->freed == 1 ) {
            FATAL( "Double free (%p)", page);
        }
        // We could make the conservative assumption that we can free
        // Only after the page is invalidated. (TODO)

        auto dofree_closure = new_Closure( 1,
        mapper_free( GET_FHEADER(page) );
        #ifdef DEBUG_ADDITIONAL_CODE
                              mapper_valid_address_rm( page );
        #endif
         );

        DELEGATE( Delegator::default_delegator,
                  ask_or_do_invalidation_then( page, dofree_closure);
                );

}

static inline void owm_free( void * page ) {
    if (mapper_who_owns(page) == get_node_num() ) {
        // Free locally.
        DEBUG( "Freeing : %p, local pointer, done locally.",page);
        owm_free_local(page);
    } else {
        DEBUG( "Freeing : %p, global pointer, sending message to %d.",page, PAGE_GET_NEXT_RESP(page));
        NetworkInterface::send_free_message(mapper_who_owns(page), page);
    }

}



#endif
