#include <data_events.hpp>
#include <identifiers.h>
#include <mappable_event.hpp>


// This is a task mapper for arriving reads.
TaskMapper<PageType> write_arrived_mapper(2);
TaskMapper<PageType> data_arrived_mapper(2);

// This is a task mapper for arriving write commits: 
struct commit_key_struct {
  bool operator<(const commit_key_struct &other) const {
	if ( this->page == other.page ) {
	  return this->serial < other.serial;
	}
	return this->page < other.page;
  }
  
  bool operator==(const commit_key_struct &other) const {
	return this->page == other.page && this->serial == other.serial;
  	
  }
  serial_t serial;
  PageType page;  
} __attribute__((packed));


TaskMapper<commit_key_struct> write_commit_mapper(2);




void signal_write_arrived( PageType page ) {
    write_arrived_mapper.signal_evt( page ); 
}


void signal_write_commited( serial_t serial, PageType page) {
     struct commit_key_struct key = {serial,page};
     write_commit_mapper.signal_evt( key );
}


void signal_data_arrived(PageType page ) {
    data_arrived_mapper.signal_evt( page );
}


// To be called onto complex objects only.
// This local task typically performs a tdec.
void acquire_rec( complex_obj_t ptr, Closure * t ) {
  // ptr is a recursive object description pointer.
  
  // The strategy we follow is simple : 
  //   - if available, proceed.
  //   - else : ask for ressources, proceed when available.
  
  if ( PAGE_IS_VALID( ptr ) ) {
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
    auto c = new_Closure( 1, {
      acquire_rec( ptr, t );
        });

    serial_t serial = request_data( ptr );
    register_for_data_arrival( serial, ptr, t );


  }
}

void release_rec( complex_obj_t ptr ) {
  

}

