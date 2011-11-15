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


TaskMapper<PageType> invalidation_tm(2);

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
  ASSERT( sizeof(bitmap) == 64);
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



// This function sends an invalidation ack to a target node.
// This refers to a specific write.
// For making it easier to deal with special cases, whenever the target
// to answer to is local it doesn't send anything but does what the
// arrival of the message would have done.
//  TODO : think about how not to write too much code here.


void ask_or_do_invalidation_rec_then( fat_pointer_p p, Closure * c ) {
  for ( int i = 0; i < p->use_count; ++ i  ) {
	struct ressource_desc & r = p->elements[i];
	if ( r.perms == WO_PERM  || r.perms == RW_PERM ) {
				
	}
  }
  // TODO


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
        register_forinvalidateack(page, c);
        NetworkInterface::send_ask_invalidate( mapper_who_owns(page), page);
  }

  
}



void planify_invalidation( void *page, node_id_t client ) {

  // Then we add a closure which will respond to the client :
  auto continuer = new_Closure( 1,
      {
        NetworkInterface::send_invalidate_ack( client, page );
      } );

  
  invalidate_and_do( page, continuer );
}


void invalidate_and_do ( void * page, Closure * c ) {
  
  // Check if page is RESP mode :
  //
  CFATAL ( ! PAGE_IS_RESP( page ), "Wrong use of invalidate and do : go for ask version" );
  
  
  RespSharedMapRange range = resp_shared_map.equal_range(page);


  int total_to_wait = 0;
  for ( auto i = range.first; i != range.second; ++i ) {

    // This loops onto the holders of a valid copy.
    node_id_t target = i->second;

    // We need to notify them and wait for their answers ( INVALIDATE_ACK )
    total_to_wait += 1;

    NetworkInterface::send_do_invalidate( target, page );
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


