
#include <identifiers.h>
#include <mappable_event>


// This is a task mapper for arriving reads.
TaskMapper write_arrived_mapper;
TaskMapper data_arrived_mapper;

// This is a task mapper for arriving write commits: 
struct commit_key_struct {
  serial_t serial;
  PageType page;  
} __attribute__((packed));

TaskMapper write_commit_mapper;




void signal_write_arrived( PageType page ) {
    write_arrived_mapper.activate( page ); 
}


void signal_write_commited( serial_t serial, PageType page) {
     struct commit_key_struct key = {serial,page};
     write_commit_mapper.activate( key );
}


void signal_data_arrived(PageType page ) {
    data_arrived_mapper.activate( page );
}


// To be called onto complex objects only.
// This local task typically performs a tdec.
void acquire_rec( complex_obj_t ptr, LocalTask * t ) {
  // ptr is a recursive object description pointer.
  
  // The strategy we follow is simple : 
  //   - if available, proceed.
  //   - else : ask for ressources, proceed when available.
  //
  //
  //  A continuation is created
  if ( IS_VALID_PAGE( ptr ) ) {
    // When ressources are available
    // We read the pointers inside.
    // For leaves we pass if fresh/local
    //  when transient/invalid, we send a datareq msg.
    //  When access is read/write, if resp we increment count atomically.
    //  We add entries to the maps corresponding to perms.
    //
    //
    // For branches, we recurse.
  } else { // ressources need prior acquision.
    // Self schedule on data arrival.

  }
}

