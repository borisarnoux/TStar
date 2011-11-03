


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





TaskMapper invalidateack_tm;


// This is essentially what happens when an invalidation
// ack arrives. This triggers a tdec onto a so called local task
// This local task will eventually fire (one shot only), and 
// can : 
//   * In the case of a resp collecting acks from its invalidation :
//      -> propagate to the node demanding the invalidation in the first place.
//        ( once all invalidations are done )
//   * In the case of a non resp having called the resp to do the invalidation :
//      -> will likely trigger an actual do_tdec (local or remote)


void signal_invalidateack( int serial, void * page ) {
  
  // Use the task mapper :

  struct invalidation_id invid;
  invid.serial = serial;
  invid.page   = page;


  invalidateack_tm.activate( invid );

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
