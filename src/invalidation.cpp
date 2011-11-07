


#include <identifiers.h>
#include <boost/unordered.hpp>

typedef unordered_multimap<PageType,node_id_t> RespSharedMap;


struct invalidation_id {
    int serial;
    void *page;

} __attribute__((packed));



// This multimap contains the nodes considering their copy of page
// valid, by page. It should not contain any page the local node is
// not responsible for.
RespSharedMap resp_shared_map;
// This is simply a pair of iterators which will represent an
// individual set.
typedef std::pair<RespSharedMap::iterator> RespSharedMapRange;


void register_copy_distribution( void * page, node_id_t nodeid ) {
  resp_shared_map.emplace( page, nodeid );   
}


long long export_and_clear_shared_set( void * page ) {
  RespSharedMapRange r = resp_shared_map.equal_range(page);

  long long retval = 0;
  
  for ( auto i = r.first; i != r.second; ++ i ) {
   
    retval |= 1<<(i->second);

  }

  resp_shared_map.erase( r );
  return retval;
}

void shared_set_load_bit_map( void * page,long long bitmap ) {
  ASSERT( sizeof(bitmap) == 64);
  for ( int i = 0; i < 64; ++ i ) {
    if ( (bitmap & 1<<i)
     &&  i!=get_node_num() ) { // We don't want to add ourselves to the map.
	    RespSharedMap.emplace( page, i );
    }

  }
}



void register_forinvalidateack( serial_t serial, void * page, Closure * c ) {

  struct invalidation_id invid;
  invid.serial = serial;
  invid.page   = page;

  invalidateack_tm.register_evt(  invid, c );
}



// This function sends an invalidation ack to a target node.
// This refers to a specific write.
// For making it easier to deal with special cases, whenever the target
// to answer to is local it doesn't send anything but does what the
// arrival of the message would have done.
//  TODO : think about how not to write too much code here.


void ask_or_do_invalidation_rec_then( serial_t serial, fat_pointer_h p, Closure * c ) {
  for ( int i = 0; i < p->use_count; ++ i  ) {
	struct ressource_desc & r = p->elements[i];
	if ( r.perm == WO_PERM  || r.perm == RW_PERM ) {
				
	}
  }


}

void ask_or_do_invalidation_then( serial_t serial, void * page, Closure * c ) {
  if ( PAGE_IS_RESP( page ) ) {
	invalidate_and_do
  
  } else {
	


  }

  
}



void planify_invalidation(serial, page, client ) {

  // Then we add a closure which will respond to the client :
  auto continuer = new_Closure( 1,
      {
        send_invalidate_ack( client, serial, page );
      } );

  
  invalidate_and_do( serial, page, continuer );
}


void invalidate_and_do ( serial_t serial, void * page, Closure * c ) {
  
  // Check if page is RESP mode :
  //
  CFATAL ( ! PAGE_IS_RESP( page ) );
  
  
  RespSharedMapRange range = resp_shared_map.equal_range(page);


  int total_to_wait = 0;
  for ( auto i = range.first; i != range.second; ++i ) {

    // This loops onto the holders of a valid copy.
    node_id_t target = i->second;

    // We need to notify them and wait for their answers ( INVALIDATE_ACK )
    total_to_wait += 1;

    send_do_invalidate( target, serial, page );
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
    register_forinvalidateack( serial, page, continuer );
  }
  

}

