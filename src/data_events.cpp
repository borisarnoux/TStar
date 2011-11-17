#include <data_events.hpp>
#include <identifiers.h>
#include <mappable_event.hpp>
#include <network.hpp>
#include <mapper.h>

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

// The data-related task mappers                   ( Map#, Name )
TaskMapper<PageType>           write_arrived_mapper(2, "Write Arrived Mapper");
TaskMapper<PageType>           data_arrived_mapper (2, "Data Arrived Mapper");
TaskMapper<PageType>           usecount_zero_mapper(2, "Usecount zero Mapper");
TaskMapper<commit_key_struct>  write_commit_mapper (2, "Write Commit");






void signal_usecount_zero( PageType page ) {
    usecount_zero_mapper.signal_evt(page);
}

void register_for_usecount_zero( PageType page, Closure * c ) {
    CFATAL( c == NULL, "Closure is NULL, invalid argument.");
    usecount_zero_mapper.register_evt(page, c);
}

void signal_write_arrival( PageType page ) {
    write_arrived_mapper.signal_evt( page ); 
}


void signal_write_commited( serial_t serial, PageType page) {
     struct commit_key_struct key = {serial,page};
     write_commit_mapper.signal_evt( key );
}


void signal_data_arrival(PageType page ) {
    data_arrived_mapper.signal_evt( page );
}

void register_for_data_arrival( PageType page, Closure * c ) {
    data_arrived_mapper.register_evt(page, c);
}

void register_for_write_arrival( PageType page, Closure * c ) {
    write_arrived_mapper.register_evt(page, c);
}


void request_page_data( PageType page ) {
    FATAL("Not implemented."); // TODO
}

void request_page_resp( PageType page ) {
    // Send a resp message.
    // On write arrival, increment use count.
    if ( PAGE_IS_RESP(page)) {
        FATAL( "Error : requesting an already responsible page.");
    }

    // We need to increment use count here because a call to request_page_resp
    // corresponds to a user of the page in write mode.
    owm_frame_layout * fheader = GET_FHEADER(page);
    Closure * incrementor = new_Closure( 1, {
         fheader->usecount+=1;
     }
    );

    // TODO : make a page only requested once.
    NetworkInterface::send_resp_req( mapper_who_owns(page),page);

}



// To be called onto complex objects only.
// This local task typically performs a tdec.
void acquire_rec( fat_pointer_p ptr, Closure * t ) {
  // As we are doing a two fold counting -> requesting procedure,
  // We need to make sure these two yield coherent results.

  delegator_only();

  // Auto reschedules itself for data arrival (if needed)
  if ( !PAGE_IS_AVAILABLE(ptr)) {
     request_page_data(ptr);
     Closure * retry_c = new_Closure( 1,
     {acquire_rec(ptr,t);});
     register_for_data_arrival(ptr, retry_c);
     return;
  }

  int todo_count = 0;
  // Examination of the contents : counting.
  for ( int i = 0; i < ptr->use_count; ++ i  ) {
        struct ressource_desc & r = ptr->elements[i];
        if ( r.perms == W_FRAME_TYPE )  {
            // Nothing to do ? ( TODO : rethink carefully )

        } else if (r.perms == RW_FRAME_TYPE ) {
            // Awaiting RESP
            if ( !PAGE_IS_RESP(r.page) ) {
                todo_count += 1;
            }
        } else if ( r.perms == FATP_TYPE ) {
            fat_pointer_p rfat = (fat_pointer_p) r.page;
            // Recurse (always)
            todo_count += 1;
        } else if ( r.perms == R_FRAME_TYPE ) {
            // Wait for the data
            if ( ! PAGE_IS_AVAILABLE(r.page) ) {
                todo_count += 1;
            }

        } else {
            FATAL( "Unknown ressources perm in fat pointer %p ", ptr);
        }
  }
  // Special case where closure is not needed :
  if ( todo_count == 0 ) {
      t->tdec();
  }

  Closure * waiter = new_Closure(todo_count,
  { t->tdec();} );

  // We recount for cautiousness.
  int todo_recount = 0;

  // Examination of the contents : linking with closure.
  for ( int i = 0; i < ptr->use_count; ++ i  ) {
        struct ressource_desc & r = ptr->elements[i];
        if ( r.perms == W_FRAME_TYPE )  {
            // We will adjust the transient state
            // as well as the use count as late as possible.
            // However, if the page is available for RW now :
            if ( PAGE_IS_RESP (r.page)) {
                owm_frame_layout * fheader = GET_FHEADER( r.page );
                fheader->usecount += 1;
            }

        } else if (r.perms == RW_FRAME_TYPE ) {
            // Awaiting RESP
            if ( !PAGE_IS_RESP(r.page) ) {
                todo_recount += 1;
                request_page_resp(r.page);
                register_for_write_arrival(r.page, waiter);
            } else {
                // We need to increase the use count :
                owm_frame_layout * fheader = GET_FHEADER( r.page );
                fheader->usecount += 1;
            }
        } else if ( r.perms == FATP_TYPE ) {
            fat_pointer_p rfat = (fat_pointer_p) r.page;
            // Recurse (always)
            todo_recount += 1;
            acquire_rec(rfat, waiter);

        } else if ( r.perms == R_FRAME_TYPE ) {
            // Wait for the data
            if ( ! PAGE_IS_AVAILABLE(r.page) ) {
                todo_recount += 1;
                request_page_data(r.page);
                register_for_data_arrival(r.page, waiter);
            }

        } else {
            FATAL( "Unknown ressources perm in fat pointer %p ", ptr);
        }
  }

  CFATAL( todo_count != todo_recount, "While executing in a delegator,"
          " acquire rec has suffered from a conccurent modififation of"
          "the frames states.");


}

void release_rec( fat_pointer_p ptr ) {
  // This function takes care of modifying the use counts for the RESP and WO regions.
  // It also processes fat pointers garbage collection.
  for ( int i = 0; i < ptr->use_count; ++ i  ) {
          struct ressource_desc & r = ptr->elements[i];
          if ( r.perms == W_FRAME_TYPE || r.perms == RW_FRAME_TYPE ) {
            // TODO : check and rethink carefully.
            owm_frame_layout * fheader = GET_FHEADER(r.page);

            fheader->usecount -= 1;
            CFATAL( fheader->usecount < 0, "Invalid use count for page %p, < 0", r.page);

            // When it reaches 0, trigger an event.
            if ( fheader->usecount == 0 ) {
                // Note, this usecount is a frame use usecount.
                // Not a fatp use count, which measures occupancy.
                signal_usecount_zero(r.page);
            }


          } else if ( r.perms == FATP_TYPE ) {
              fat_pointer_p rfat = (fat_pointer_p) r.page;
              // We recurse,
              release_rec( rfat );
              // Then mark the fat pointer as having one less user :
              rfat->smart_count -= 1;
              CFATAL( rfat->smart_count < 0, "Invalid smart count for fat pointer %p", rfat );

              if ( rfat->smart_count == 0 ) {
                  FATAL( "TODO : not implemented.");
                  // Free the smart pointer.
              }

          } else if ( r.perms == R_FRAME_TYPE ) {
              // Nothing to do.
          } else {
              FATAL( "Unknown ressources perm in fat pointer %p ", ptr);
          }
    }

}

void ask_or_do_tdec( void * page ) {
    FATAL("Not implemented");
}
