#ifndef INVALIDATION_H
#define INVALIDATION_H

#include <identifiers.h>
#include <misc.h>
#include <frame.hpp>
#include <delegator.hpp>

void signal_invalidation_ack( PageType page, serial_t s );
void register_forinvalidateack( PageType page, serial_t s, Closure * c );

long long export_and_clear_shared_set( void * page );
void shared_set_load_bit_map( void * page,long long bitmap );
void register_copy_distribution( void * page, node_id_t nodeid );
void ask_or_do_invalidation_then( void * page, Closure * c );
void planify_invalidation( void * page, serial_t s, node_id_t client ) ;
void invalidate_and_do ( void * page, Closure * c ) ;
void ask_or_do_tdec(void * page);

#endif

