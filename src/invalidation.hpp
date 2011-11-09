#ifndef INVALIDATION_H
#define INVALIDATION_H

#include <identifiers.h>
#include <misc.h>
#include <frame.hpp>
#include <delegator.hpp>
#include <fat_pointers.hpp>




void ask_or_do_invalidation_rec_then( fat_pointer_p p, Closure * c );
void ask_or_do_invalidation_then( void * page, Closure * c );
void planify_invalidation( void * page, node_id_t client ) ;
void invalidate_and_do ( void * page, Closure * c ) ;



#endif

