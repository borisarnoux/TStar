


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



void register_forinvalidateack( serial_t serial, void * page, LocalTask * c ) {

  struct invalidation_id invid;
  invid.serial = serial;
  invid.page   = page;

  invalidateack_tm.register(  invid, c );
}



// This function sends an invalidation ack to a target node.
// This refers to a specific write.
// For making it easier to deal with special cases, whenever the target
// to answer to is local it doesn't send anything but does what the
// arrival of the message would have done.
//  TODO : think about how not to write too much code here.
void send_invalidate_ack( node_id_t target, serial_t serial, PageType page ) {
    if ( target == get_node_num() ) {
      // Just trigger the invalidation as done locally.
      
    }

    // TODO : Or actually send the message.
    
}

void planify_invalidation ( serial_t serial, void * page, node_id_t client ) {
  // If the Resp is local :
  // if shared == {}
  //    then trigger signal_invalidateack and exit.
  // else 
  //    choose a serial for doinvalidates
  //    send do_invalidate to all members of shared.
  //    register all of them into the multimap. ( double entries )
 
  RespSharedMapRange range = resp_shared_map.equal_range(page);


  int total_to_wait = 0;
  for ( auto i = range.first; i != range.second; ++i ) {

    // This loops onto the holders of a valid copy.
    node_id_t target = i->second;

    // We need to notify them and wait for their answers ( INVALIDATE_ACK )
    total_to_wait += 1;

    send_do_invalidate( target, serial, page );
  }

  // Interesting special case :
  if ( total_to_wait == 0 ) {
    send_invalidate_ack( client, serial, page );
    // Here we finish immediately.
    return;
  }

  // Then we add a closure which will respond to the client :
  auto continuer = new_ClosureLocalTask( total_to_wait,
      {
        send_invalidate_ack( client, serial, page );
      } );    
  // And we register it into the invalidate_ack map :
  for ( int i = 0; i < total_to_wait; ++i ) {
    register_forinvalidateack( serial, page, continuer );
  }
}
