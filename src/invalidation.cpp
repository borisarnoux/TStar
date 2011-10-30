






struct invalidation_id {
    int serial;
    void *page;

} __attribute__((packed));


TaskMapper invalidateack_tm;

void signal_invalidateack( int serial, void * page ) {
  
  // Use the task mapper :

  struct invalidation_id invid;
  invid.serial = serial;
  invid.page   = page;


  invalidateack_tm.activate( invid );

}

void register_forinvalidateack( int serial, void * page, LocalTask * c ) {

  struct invalidation_id invid;
  invid.serial = serial;
  invid.page   = page;

  invalidateack_tm.register(  invid, c );
}


void demand_invalidation ( int serial, void * page ) {
  // If the Resp is local :
  // if shared == {}
  //    then trigger signal_invalidateack and exit.
  // else 
  //    choose a serial for doinvalidates
  //    send do_invalidate to all members of shared.
  //    register all of them into the multimap. ( double entries )
  //    
  //
}
