#include <data_events.hpp>
#include <identifiers.h>
#include <mappable_event.hpp>
#include <network.hpp>
#include <mapper.h>
#include <scheduler.hpp>

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
TaskMapper<PageType>           write_arrived_mapper(10, "Write Arrived Mapper");
TaskMapper<PageType>           data_arrived_mapper (10, "Data Arrived Mapper");
TaskMapper<PageType>           usecount_zero_mapper(10, "Usecount zero Mapper");
TaskMapper<commit_key_struct>  write_commit_mapper (10, "Write Commit");






void signal_usecount_zero( PageType page ) {
    usecount_zero_mapper.signal_evt(page);
}

void register_for_usecount_zero( PageType page, Closure * c ) {
    CFATAL( c == NULL, "Closure is NULL, invalid argument.");
    usecount_zero_mapper.register_evt(page, c);
}

void signal_write_arrival( PageType page ) {
    DEBUG( "DATAEVENTS -- Write arrival on : %p",page);

    write_arrived_mapper.signal_evt( page ); 
}


void signal_write_commited( serial_t serial, PageType page) {
     DEBUG( "DATAEVENTS -- Write commited on : %p, serial %d",page,serial);

     struct commit_key_struct key = {serial,page};
     write_commit_mapper.signal_evt( key );
}


void signal_data_arrival(PageType page ) {
    DEBUG( "DATAEVENTS -- Data arrival : %p",page);
    data_arrived_mapper.signal_evt( page );
}

void register_for_data_arrival( PageType page, Closure * c ) {
    data_arrived_mapper.register_evt(page, c);
}

void register_for_write_arrival( PageType page, Closure * c ) {
    write_arrived_mapper.register_evt(page, c);
}

void register_for_commitack(PageType page, serial_t serial, Closure * c ) {
    commit_key_struct cks = {serial, page};
    write_commit_mapper.register_evt(cks, c);
}

void signal_commit(PageType page, serial_t serial ) {
    commit_key_struct cks = {serial,page};
    write_commit_mapper.signal_evt(cks);
}

void request_page_data( PageType page ) {
    NetworkInterface::send_data_req( PAGE_GET_NEXT_RESP(page), page);
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
    Closure * incrementor = new_Closure( 1,
         fheader->usecount+=1;

    );
    register_for_write_arrival(page, incrementor);

    // How can we make a page only requested once ?
    // It is difficult because a page need not be valid to be used...
    // TODO : discuss, gaunge or address this weakness
    NetworkInterface::send_resp_req( mapper_node_who_owns(page),page);

}




void ask_or_do_tdec( void * page ) {
    if ( PAGE_IS_RESP(page) ) {
        DEBUG("Ask or do TDEC %p : done locally.", page);
        // Do TDec locally.
        struct frame_struct * fp = (frame_struct *) page;
        int sfres =  __sync_sub_and_fetch( &fp->sc, 1 );
        DEBUG( "TDec comes to : %d", sfres);
        if ( sfres == 0 ) {
            Scheduler::global_scheduler->schedule_global(fp);
        } else if ( sfres < 0 ) {
            FATAL( "Negative TDec.");
        }
    } else {
        // Send a message for TDec :
        DEBUG("Ask or do TDEC %p : sent remotely to %d.", page, PAGE_GET_NEXT_RESP(page));

        NetworkInterface::send_tdec( PAGE_GET_NEXT_RESP(page), page, 1);
    }




}


static __thread serial_t serial_source = 0;
void ask_or_do_rwrite_then( PageType page, size_t offset, void *input_buffer, size_t len, Closure * c) {

    // For now just consider doingthis on a RESP is an error :
    CFATAL( PAGE_IS_RESP(page), "Asking for RWRite on locally available page.");
    node_id_t nextresp = PAGE_GET_NEXT_RESP(page);

    // Get a serial :
    if ( serial_source == 0 ) {
        serial_source = get_thread_num();
    }

    serial_t chosen_serial = serial_source;
    // This way each thread counts with different serials.
    serial_source += get_num_threads();

    // Register the closure :
    register_for_commitack( page, chosen_serial, c);
    NetworkInterface::send_rwrite( nextresp, chosen_serial, page, offset,input_buffer, len );

}
