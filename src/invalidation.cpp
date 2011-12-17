#include <fat_pointers.hpp>
#include <network.hpp>

#include <hash_int.h>
#include <delegator.hpp>
#include <misc.h>
#include <identifiers.h>
#include <mappable_event.hpp>
#include <map>

#include <omp.h>
#include <invalidation.hpp>
#include <mapper.h>


typedef void * PageType;
typedef std::multimap<PageType,node_id_t> RespSharedMap;





typedef std::pair<RespSharedMap::iterator,RespSharedMap::iterator> RespSharedMapRange;
typedef RespSharedMap::value_type RespSharedMapElt;

typedef std::pair<PageType,serial_t> InvAckPair;

static __thread int invalidation_tm_counter = 0;
static int invalidation_tm_getserial() {
    int ticket = invalidation_tm_counter;
    ticket *= get_num_threads();
    ticket += get_thread_num();

    invalidation_tm_counter++;

    return ticket;
}

TaskMapper<InvAckPair> invalidation_tm(2, "Invalidation Mapper");

// This multimap contains the nodes considering their copy of page
// valid, by page. It should not contain any page the local node is
// not responsible for.
RespSharedMap resp_shared_map;
// This is simply a pair of iterators which will represent an
// individual set.

void register_copy_distribution( void * page, node_id_t nodeid ) {
#if DEBUG_ADDITIONAL_CODE
  delegator_only();
#endif
  resp_shared_map.insert( RespSharedMapElt(page, nodeid) );   
}


void signal_invalidation_ack( PageType page, serial_t s ) {
#if DEBUG_ADDITIONAL_CODE
  delegator_only();
#endif
    invalidation_tm.signal_evt(InvAckPair(page, s));
}

void register_forinvalidateack( PageType page, serial_t s, Closure * c ) {
#if DEBUG_ADDITIONAL_CODE
  delegator_only();
#endif
    invalidation_tm.register_evt( InvAckPair(page,s), c );
}

long long export_and_clear_shared_set( void * page ) {
#if DEBUG_ADDITIONAL_CODE
  delegator_only();
#endif
  RespSharedMapRange r = resp_shared_map.equal_range(page);

  long long retval = 0;
  
  for ( auto i = r.first; i != r.second; ++ i ) {
    DEBUG("SharedSet : transferring %d as valid copy holder", i->second);
    retval |= 1<<(i->second);

  }

  resp_shared_map.erase( r.first, r.second );
  return retval;
}

void shared_set_load_bit_map( void * page,long long bitmap ) {
#if DEBUG_ADDITIONAL_CODE
  delegator_only();
#endif
  ASSERT( sizeof(bitmap) >= 8);
  for ( int i = 0; i < 64; ++ i ) {
    if ( (bitmap & 1<<i)
     &&  i!=get_node_num() ) { // We don't want to add ourselves to the map.
	    resp_shared_map.insert( RespSharedMapElt( page, i) );
            DEBUG( "shared set(%p) += %d",page, i);
    }

  }
}









void ask_or_do_invalidation_rec_then( fat_pointer_p p, Closure * c ) {
#if DEBUG_ADDITIONAL_CODE
  delegator_only();
#endif
  // Because p is a fat pointer,
  // it is supposed to stay valid until this point.
  CFATAL( !PAGE_IS_AVAILABLE(p), "Illegal invalid fat pointer.");


  // Replace this model with one actually taking care of the writes
  // To determine whether invalidation is necessary or not.
  int count_todo = 0;
  for ( int i = 0; i < p->use_count; ++ i  ) {
        struct ressource_desc & r = p->elements[i];
        if ( r.perms == W_FRAME_TYPE  || r.perms == RW_FRAME_TYPE ) {
            // RW means we need to check for RESP acquired (or this is a bug )
            CFATAL ( r.perms == RW_FRAME_TYPE && !PAGE_IS_RESP( r.page ),
                     "Page requires RW perm, but RESP was not granted. FATAL " );
            count_todo +=1;

        } else if ( r.perms == FATP_TYPE ) {
            // Recurse, we need to count it :
            count_todo +=1;
        } else if ( r.perms == R_FRAME_TYPE ) {
            // Nothing to do.
        } else {
            FATAL( "Unknown ressources perm in fat pointer %p ", p);
        }
  }
  // Before doing anything else, we must count operations to be done.
  Closure * waiter = new_Closure(count_todo,
   c->tdec();
  );


  // We now link the work with this new task.
  for ( int i = 0; i < p->use_count; ++ i  ) {
       struct ressource_desc & r = p->elements[i];
       if ( r.perms == W_FRAME_TYPE  || r.perms == RW_FRAME_TYPE ) {
           ask_or_do_invalidation_then(r.page, waiter);

       } else if ( r.perms == FATP_TYPE ) {
           ask_or_do_invalidation_rec_then((fat_pointer_p)r.page, waiter);

       } else if ( r.perms == R_FRAME_TYPE ) {
           // Nothing to do.

       } else {
           FATAL( "Unknown ressources perm in fat pointer %p ", p);
       }
  }


}


void ask_or_do_invalidation_then(  void * page, Closure * c ) {
#if DEBUG_ADDITIONAL_CODE
  delegator_only();
#endif
  // Note c == null means no register closure.

  if ( PAGE_IS_RESP( page ) ) {
      // Register closure in invalidate and do.
      DEBUG( "Performing invalidation for %p", page);
      invalidate_and_do(page, c);
  } else {
      // Send message.
      // Register closure for invalidation arrival.
      DEBUG( "Sending invalidation ask for %p to %d", page, PAGE_GET_NEXT_RESP(page));

      serial_t s = invalidation_tm_getserial();
      if ( c != NULL ) {
          register_forinvalidateack(page, s, c);
      }

      NetworkInterface::send_ask_invalidate( PAGE_GET_NEXT_RESP(page), page, s);
  }

  
}


// This function answers a remote invalidation demand.
void planify_invalidation( void *page, serial_t s, node_id_t client ) {
  CFATAL ( ! PAGE_IS_RESP( page ), "planify invalidation is to be executed on RESP only." );

  // Then we add a closure which will respond to the client :
  auto continuer = new_Closure( 1,

        NetworkInterface::send_invalidate_ack( client, page,s );
       );

  
  invalidate_and_do( page, continuer );
}


// Invalidate and do operates on RESP only : it takes care of sending
// a do invalidate message to the members of the shared set.
// When
void invalidate_and_do ( void * page, Closure * c ) {
  delegator_only();
  // Check if page is RESP mode :
  CFATAL( !PAGE_IS_RESP(page), "Invalidate_and_do must only be activated on RESP pages.");
  
  
  RespSharedMapRange range = resp_shared_map.equal_range(page);

  int total_to_wait =
          std::count_if(range.first,
                        range.second,
                        [](RespSharedMapElt e){return true;});

  if ( total_to_wait == 0 ) {
    // We cannot rely on any event then :
    if ( c != NULL ) {
        (*c).tdec();
    }
    return;
  }

  Closure * continuer = NULL;
  if ( c!= NULL ) {
  continuer = new_Closure( total_to_wait,
            CFATAL( c == NULL, "NULL Closure invalid here." );
            (*c).tdec();

  );
  }

  for ( auto i = range.first; i != range.second; ++i ) {
    // We need a serial:
    serial_t s = invalidation_tm_getserial();

    // This loops onto the holders of a valid copy.
    node_id_t target = i->second;
    if ( c != NULL ) {
        register_forinvalidateack( page, s, continuer );
    }

    NetworkInterface::send_do_invalidate( target, page, s);
  }
  resp_shared_map.erase(range.first,range.second);

}


