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


TaskMapper<PageType> invalidation_tm(2, "Invalidation Mapper");

// This multimap contains the nodes considering their copy of page
// valid, by page. It should not contain any page the local node is
// not responsible for.
RespSharedMap resp_shared_map;
// This is simply a pair of iterators which will represent an
// individual set.

void register_copy_distribution( void * page, node_id_t nodeid ) {
  resp_shared_map.insert( RespSharedMapElt(page, nodeid) );   
}


void signal_invalidation_ack( PageType page ) {
    invalidation_tm.signal_evt(page);
}

long long export_and_clear_shared_set( void * page ) {
  RespSharedMapRange r = resp_shared_map.equal_range(page);

  long long retval = 0;
  
  for ( auto i = r.first; i != r.second; ++ i ) {
   
    retval |= 1<<(i->second);

  }

  resp_shared_map.erase( r.first, r.second );
  return retval;
}

void shared_set_load_bit_map( void * page,long long bitmap ) {
  ASSERT( sizeof(bitmap) >= 8);
  for ( int i = 0; i < 64; ++ i ) {
    if ( (bitmap & 1<<i)
     &&  i!=get_node_num() ) { // We don't want to add ourselves to the map.
	    resp_shared_map.insert( RespSharedMapElt( page, i) );
    }

  }
}



void register_forinvalidateack(  void * page, Closure * c ) {
  invalidation_tm.register_evt(  page, c );
}






void ask_or_do_invalidation_rec_then( fat_pointer_p p, Closure * c ) {
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
  { c->tdec(); }
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
  // Note c == null means no register closure.

  if ( PAGE_IS_RESP( page ) ) {
      // Register closure in invalidate and do.
      DEBUG( "Performing invalidation for %p", page);
      invalidate_and_do(page, c);
  } else {
      // Send message.
      // Register closure for invalidation arrival.
      DEBUG( "Sending invalidation for %p", page);

      if ( c != NULL ) {
          register_forinvalidateack(page, c);
      }

      NetworkInterface::send_ask_invalidate( mapper_who_owns(page), page);
  }

  
}


// This function answers a remote invalidation demand.
void planify_invalidation( void *page, node_id_t client ) {
  CFATAL ( ! PAGE_IS_RESP( page ), "planify invalidation is to be executed on RESP only." );

  // Then we add a closure which will respond to the client :
  auto continuer = new_Closure( 1,
      {
        NetworkInterface::send_invalidate_ack( client, page );
      } );

  
  invalidate_and_do( page, continuer );
}


// Invalidate and do operates on RESP only : it takes care of sending
// a do invalidate message to the members of the shared set.
// When
void invalidate_and_do ( void * page, Closure * c ) {
  
  // Check if page is RESP mode :
  //
  
  
  RespSharedMapRange range = resp_shared_map.equal_range(page);


  int total_to_wait = 0;
  for ( auto i = range.first; i != range.second; ++i ) {

    // This loops onto the holders of a valid copy.
    node_id_t target = i->second;

    // We need to notify them and wait for their answers ( INVALIDATE_ACK )
    total_to_wait += 1;

    NetworkInterface::send_do_invalidate( target, page );
  }

  // The rest is for triggering in case of c!= NULL
  if ( c == NULL ) {
      return;
  }

  // We can avoid setting up :
  if ( total_to_wait == 0 ) {
    c->tdec();
  }

  // Todo ; optitimize with a tincr...

  Closure * continuer = new_Closure( total_to_wait, 
	{ 
	  (*c).tdec();
	}
  );
  
  // And we register it into the invalidate_ack map :
  for ( int i = 0; i < total_to_wait; ++i ) {
    register_forinvalidateack( page, continuer );
  }
  

}


