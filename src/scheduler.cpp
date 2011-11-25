#include <unistd.h>

#include <omp.h>
#include <list>
#include <map>
#include <scheduler.hpp>
#include <fat_pointers.hpp>
#include <identifiers.h>
#include <data_events.hpp>
#include <invalidation.hpp>
#include <delegator.hpp>
#include <network.hpp>
#include <mapper.h>
/* This  scheduler class  keeps track of the tasks when it is needed */




int Scheduler::task_count = 0;



typedef std::multimap<PageType, DWrite> DWriteMap;
typedef std::multimap<PageType, PageType> TDecMap;
typedef std::list<struct frame_struct*> CreatedFramesList;

void ExecutionUnit::executor( struct frame_struct * page ) {
    local_execution_unit->current_cfp = page;
    if ( ! PAGE_IS_AVAILABLE( page ) ) {
        // This, while not being strictly speaking a mistake, shouldn't happen.
        FATAL( "Execution of invalid page" );
    }
    DEBUG( "%p %p %p", local_execution_unit, local_execution_unit, page );
    local_execution_unit->before_code();
    page->static_data->fn();
    local_execution_unit->after_code();


}

void ExecutionUnit::tdec( struct frame_struct * page, void * ref ) {
    CFATAL( ref==NULL, "Unreferenced tdecs unsupported.");
    tdecs_by_ressource.insert( TDecMap::value_type(ref,page) );
}


void ExecutionUnit::register_write( void * object, void * frame, size_t offset, size_t len ) {
    void * k = object!=NULL?object:frame;
    struct DWrite w = {object,frame,offset,len};
    writes_by_ressource.insert( DWriteMap::value_type(k,w));
}

void ExecutionUnit::process_commits() {
    // For each registered ressource

    ressource_desc d;
    int nargs = current_cfp->static_data->nargs;

    DEBUG( "ExecutionUnit : Processing Commits -- Task : %p", current_cfp);
    for ( int i = 0; i < nargs; ++i ) {
        long & current_type = current_cfp->static_data->arg_types[i];
        if (  current_type == DATA_TYPE || current_type == R_FRAME_TYPE ) {
            continue;
        }

        void * current_value = current_cfp->args[i];

        // Same thing for all writes ( by object or by frame )
        auto dwrite_range = writes_by_ressource.equal_range(current_value);

        std::list<DWrite> frame_dwrites;
        for ( auto i = dwrite_range.first; i != dwrite_range.second; ++i) {
            frame_dwrites.insert(frame_dwrites.begin(), i->second);
        }

        writes_by_ressource.erase( dwrite_range.first, dwrite_range.second );


        // We construct a list in heap of all tdecs, and assoiated closure.
        std::pair<TDecMap::iterator,TDecMap::iterator>
                tdec_range = tdecs_by_ressource.equal_range(current_value);
        // Build a list of tdecs to do :
        std::list<PageType> * frame_tdecs = new std::list<PageType>();
        for ( auto i = tdec_range.first; i != tdec_range.second; ++i ) {
            frame_tdecs->insert(frame_tdecs->begin(), i->second );
        }
        // Remove them from the ressource map.
        tdecs_by_ressource.erase(tdec_range.first,tdec_range.second);

        // We create a closure for managing the tdecs :
        Closure * dotdecs = new_Closure( 1,// Because only one frame or object.

            for ( auto i = frame_tdecs->begin();
                        i != frame_tdecs->end(); ++ i ) {
                        ask_or_do_tdec(*i);
            }
            delete frame_tdecs;
        );

        // Then come the special case where there are no writes.
        if ( frame_dwrites.empty() ) {
            DEBUG ("Ressource %p, no writes, invalidation & TDecs to do.", current_value);
            // Commits the tdecs immediately after invalidation :

            if ( current_type == FATP_TYPE ) {
                ask_or_do_invalidation_rec_then( (fat_pointer_p)current_value, dotdecs );
            } else if ( current_type == W_FRAME_TYPE ||
                current_type == RW_FRAME_TYPE ) {
                ask_or_do_invalidation_then(current_value, dotdecs );
            }


            // And we can examine the next ressource.
            continue;
        }

        // At this point we have remote writes registered,
        // and this requires a two step process :
        // * Sending RWrites.
        // * On response do all tdecs.



        // Now, we need another closure, the one
        // triggered after the writes commits arrive.

        Closure *  on_write_commits = new_Closure ( frame_dwrites.size(),
              if ( current_type == FATP_TYPE ) {
                  ask_or_do_invalidation_rec_then( (fat_pointer_p)current_value, dotdecs );
              } else if ( current_type == W_FRAME_TYPE ||
                  current_type == RW_FRAME_TYPE ) {
                  ask_or_do_invalidation_then(current_value, dotdecs );
              }

        );

        // Finally, we do the writes :
        for ( auto i = frame_dwrites.begin(); i!=frame_dwrites.end(); ++i) {
            ask_or_do_rwrite_then(current_value, i->offset, i->len, on_write_commits);

        }


    }

    // If there are writes remaining : error.
    CFATAL( !writes_by_ressource.empty(), "A write was associated with an unknown argument (check frame/objects)");
    // If there are TDecs left, it is ok:
    for ( auto i = tdecs_by_ressource.begin(); i!= tdecs_by_ressource.end();++i) {
        if ( i->first != NULL ) {
            FATAL( "Non null tdec reference associated with unknown frame.");
        }
        // Anonymous TDecs : not implemented.
        // - one option is to make them depend on nothing.
        // - another is to make them depend on everything.
        FATAL( "Anonymous tdecs unsupported");
        ask_or_do_tdec(i->second);
    }



}


bool ExecutionUnit::check_ressources() {
    // Check all ressources
    for ( int i = 0; i < current_cfp->static_data->nargs;
          ++i ) {
        switch( current_cfp->static_data->arg_types[i] ) {
        case R_FRAME_TYPE:
            // no need to check because data is new
            // enough by construction.
            break;
        case RW_FRAME_TYPE:
            // Need to check if data is in RESP mode and has
            // a good usecount
            CFATAL( ! PAGE_IS_RESP(current_cfp->args[i]), "RW frames has no resp on requested frame." );
            break;
        case W_FRAME_TYPE:
            CFATAL( ! PAGE_IS_TRANSIENT( current_cfp->args[i]), "WONLY frame didn't reserve frame for WONLY.");
            break;
        case FATP_TYPE:
            FATAL( "TODO");
            break;


        }
    }
    return true;
}


void ExecutionUnit::before_code() {
    // Check ressources availability.

}

void ExecutionUnit::after_code() {
    // Process commits.
    // and/or free zones written.
    // Process fat pointers refecence counters.
    local_execution_unit->process_commits();
    NetworkInterface *nip = &local_execution_unit->ni;
    DELEGATE( Delegator::default_delegator, nip->process_messages(); );
}


__thread ExecutionUnit * ExecutionUnit::local_execution_unit = NULL;
__thread bool Scheduler::initialized = false;


// This is the entry point, should be attained when SC reaches 0.
void Scheduler::schedule_global( struct frame_struct * page )  {
    CFATAL( ! initialized, "Uninitialized TLS");
    if ( task_count > global_local_threshold ) {
        schedule_external( page );
    } else {
        task_count ++;
        prepare_ressources( page );
    }
}

// This function adds the task to an external queue.
// This queue can be eventually published or returned for local execution.
void Scheduler::schedule_external( struct frame_struct * page ) {
    external_tasks.push_front( page );
}


// This function won't keep track of the task, will instead spawn local tasks to handle the ressources it needs (and thus hide latency)
void Scheduler::prepare_ressources( struct frame_struct * page ) {
    // As a first step we need to analyze how this task accesses other pages.
    // Then we need to plan recursively the ressource acquisition.
    // Finallly, the total number of sub-tasks we created is used as trigger level for the next step.

    delegator_only();

    int work_count = 0;
    if ( !PAGE_IS_AVAILABLE( page ) ) {
        // Then schedule itself for when its ready :p
        auto todo = new_Closure( 1,prepare_ressources( page ););

        register_for_data_arrival( page, todo );

     return;
    }
    for ( int i = 0; i < page->static_data->nargs; ++ i ) {
        if ( page->static_data->arg_types[i] == R_FRAME_TYPE ) {
            if ( !PAGE_IS_AVAILABLE(page->args[i]) ) {
                work_count += 1;
            }

        } else if ( page->static_data->arg_types[i] == RW_FRAME_TYPE ) {
            if ( !PAGE_IS_RESP(page->args[i]) ) {
                work_count += 1;
            }
        } else if ( page->static_data->arg_types[i] == FATP_TYPE ) {
            work_count += 1;
        }



    }


    if ( work_count == 0 ) {
        schedule_inner( page );
        return;
    }

    auto gotoinnersched  = new_Closure( work_count,
        schedule_inner( page );
    );


    // Recount work count for safety.
    int recount_work = 0;
    for ( int i = 0; i < page->static_data->nargs; ++ i ) {
        if ( page->static_data->arg_types[i] == R_FRAME_TYPE ) {
            if ( !PAGE_IS_AVAILABLE(page->args[i])) {
                request_page_data(page->args[i]);
                register_for_data_arrival( page->args[i], gotoinnersched );
                recount_work += 1;
            }
        }

        if (  page->static_data->arg_types[i] == RW_FRAME_TYPE ) {
            if ( !PAGE_IS_RESP(page->args[i])) {
                request_page_resp(page->args[i]);
                register_for_write_arrival( page->args[i], gotoinnersched );
                recount_work += 1;
            }
        }

        if (   page->static_data->arg_types[i] == FATP_TYPE ) {

            acquire_rec((fat_pointer_p) page->args[i], gotoinnersched );
            recount_work += 1;
        }
    }

    CFATAL( recount_work != work_count, "Memory frame state modified during prepare_ressources.");

}


// This yields to the inner scheduler.
void Scheduler::schedule_inner( struct frame_struct * page ) {

#pragma omp task
    {
    ExecutionUnit::local_execution_unit->executor( page );
    task_count --;
    }
}


// This method returns an array of stolen tasks.
// buffer is an output buffer, supposed big enough.
int Scheduler::steal_tasks( struct frame_struct ** buffer, int amount ) {
    delegator_only();

    for ( int i = 0; i < amount; ++ i ) {
        if ( external_tasks.empty() ) {
            // Only i tasks were stolen.
            return i;
        }
        buffer[i] = external_tasks.back();
        external_tasks.pop_back();
    }

    // `Amount` tasks were stolen
    return amount;
}

void Scheduler::steal_and_process() {
    int initial_target = rand()%get_num_nodes();
    int target = initial_target;
    for (;;) {

                struct frame_struct * buffer[10];
                int amount = 0;

                while ( (amount = steal_tasks(buffer, 10)) == 0 ) {
                      target = ( target + 1)%get_num_nodes();
                      if ( target == initial_target ) {
                          goto endd;
                      }
                }

                for ( int i = 0; i < amount; ++i ) {
                    schedule_global(buffer[i]);
                }
                continue;
    endd:

                usleep(100);
                ni.process_messages();
    }



}


int Scheduler::get_refund( int amount ) {
    delegator_only();

    for ( int i = 0; i < amount; ++ i ) {
        if ( external_tasks.empty() ) {
            return i;
        }

        // This will eventually get the tasks to execute.
        prepare_ressources( external_tasks.front() ) ;
        external_tasks.pop_front();
    }

    return amount;
}



