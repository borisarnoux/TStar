#include <omp.h>
#include <list>

#include <scheduler.hpp>
#include <fat_pointers.hpp>
#include <identifiers.h>
#include <data_events.hpp>
/* This small scheduler class actually keeps track of the tasks when it is needed only */




class Scheduler {
    std::list<struct frame_struct *> external_tasks;
    int global_local_threshold;
    int task_count;


public :

    Scheduler() : global_local_threshold(omp_get_num_threads() * 5 ), task_count(0) {
	
    }

    // This is the entry point, should be attained when SC reaches 0.
    void schedule_global( struct frame_struct * page )  {
	if ( task_count > global_local_threshold ) {
            schedule_external( page );
	} else {
            prepare_ressources( page );
	}
    }

    // This function adds the task to an external queue.
    // This queue can be eventually published or returned for local execution.
    void schedule_external( struct frame_struct * page ) {
	external_tasks.push_front( page );
    }

    // This function won't keep track of the task, will instead spawn local tasks to handle the ressources it needs (and thus hide latency)
    void prepare_ressources( struct frame_struct * page ) {
        // As a first step we need to analyze how this task accesses other pages.
        // Then we need to plan recursively the ressource acquisition.
        // Finallly, the total number of sub-tasks we created is used as trigger level for the next step.

	int work_count = 0;
	if ( !PAGE_IS_AVAILABLE( page ) ) {
            // Then schedule itself for when its ready :p
            auto todo = new_Closure( 1,
            { prepare_ressources( page ); } );

            register_for_data_arrival( page, todo );

            return;
        }

        for ( int i = 0; i < page->static_data->nargs; ++ i ) {
            if ( page->static_data->fields[i] == R_FRAME_TYPE
              || page->static_data->fields[i] == RW_FRAME_TYPE
              ||  page->static_data->fields[i] == FATP_TYPE ) {
                work_count += 1;
            }

        // Will be mapped once more for writing :
            if (  page->static_data.fields[i] == RW_FRAME_TYPE ) {
                work_count += 1;
            }
         }


    if ( work_count == 0 ) {
        schedule_inner( page );
        return;
    }

    auto gotoinnersched  = new_ClosureLocalTask( work_count,
        {schedule_inner( page );}
    );


    for ( int i = 0; i < page->static_data->nfields; ++ i ) {
        if ( page->static_data.fields[i] == ACQUIRE_SIMPLE_R ) {
            register_for_data_arrival( page->fields[i], gotoinnersched );
        }

        if (  page->static_data.fields[i] == ACQUIRE_SIMPLE_RW ) {
            register_for_data_arrival( page->fields[i], gotoinnersched);
            register_for_write_arrival( page->fields[i], gotoinnersched );
        }

        if (   page->static_data.fields[i] == ACQUIRE_COMPLEX ) {
            prepare_data_rec( page->fields[i], gotoinnersched );
        }
    }

}


// This yields to the inner scheduler.
static void schedule_inner( struct frame_struct * page ) {
#pragma omp task
    executor( page );
}


// This method returns an array of stolen tasks.
// buffer is an output buffer, supposed big enough.
int steal_tasks( struct frame_struct * buffer, int amount ) {
    delegate_only();

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

int get_refund( int amount ) {
    delegate_only();

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






};

struct TDec {
    struct frame_struct * target;
    void * related;
};

struct DWrite {
    void * object;
    void * frame;
    void * pos;
    size_t len;
};
typedef std::multimap<PtrType, DWrite> DWriteMap;
typedef std::multimap<PtrType, TDec> TDecMap;
class ExecutionUnit {


    struct frame_struct * current_cfp;
    // Ressource -> writes.
    DWriteMap writes_by_ressource;
    TDecMap tdecs_by_ressource;


    void executor( struct frame_struct * page ) {
	current_cfp = page;
	if ( ! PAGE_VALID( page ) ) {
            FATAL( "Execution of invalid page" );
	}

	before_code();
	page->static_data->code();
	after_code();
	

    }

    void tdec( struct frame_struct * page, void * ref ) {
        CFATAL( ref==NULL, "Unreferenced tdecs unsupported.");
	struct TDec mytdec = {page,ref};
	tdecs_list.push_front(mytdec);
    }


    void register_write( void * object, void * frame, void * pos, size_t len ) {
        FATAL("NOT IMPLEMENTED");

    }

    void process_commits() {
	// For each registered ressource 
        ressource_desc d;
        int nargs = current_cfp->static_data->nargs;
        for ( int i = 0; i < nargs; ++i ) {
            long & current_type = current_cfp->static_data->arg_types[i];
            if (  current_type == DATA_TYPE ) {
                continue;
            }

            void * current_value = current_cfp->args[i];

            // Same thing for all writes ( by object or by frame )
            std::pair<DWriteMap::iterator,DWriteMap::iterator>
                    dwrite_range = writes_by_ressource.equal_range(current_value);

            std::list<DWrite> frame_dwrites;
            for ( auto i = dwrite_range.first; i != dwrite_range.second; ++i) {
                frame_dwrites.insert(frame_dwrites.begin(), i->second);
            }

            writes_by_ressource.erase( dwrite_range.first, dwrite_range.second );

            // Then come the special case where there are no writes.
            if ( frame_dwrites.empty() ) {
                // Commits the tdecs immediately.
                std::pair<TDecMap::iterator,TDecMap::iterator>  tdec_range =
                        tdecs_by_ressource.equal_range(current_value);
                for ( auto i = tdec_range.first;
                      i != tdec_range.second;
                      ++i ) {

                    ask_or_do_tdec(i->second.page);
                }
                // And remove from map.
                tdecs_by_ressource.erase(tdec_range.first,tdec_range.second);

                // And we can examine the next ressource.
                continue;
            }


            // Find all tdecs :
            std::pair<TDecMap::iterator,TDecMap::iterator>
                    tdec_range = tdecs_by_ressource.equal_range(current_value);
            // Build a list of tdecs to do :
            std::list<TDec> * frame_tdecs = new std::list<TDec>();
            for ( auto i = tdec_range.first; i != tdec_range.second; ++i ) {
                frame_tdecs->insert(frame_tdecs->begin(), i->second );
            }
            // Remove them from the ressource map.
            tdecs_by_ressource.erase(tdec_range.first,tdec_range.second);

            // We create a continuation :
            Closure * dotdecs = new_Closure( 1,// Because only one frame or object
                   {
                for ( auto i = frame_tdecs->begin();
                      i != frame_tdecs->end; ++ i ) {
                        ask_or_do_tdec(i->page);
                }
                delete frame_tdecs;

                }

            );

            // And trigger it depending on frame type :

            if ( current_type == FATP_TYPE ) {
                ask_or_do_invalidation_rec( current_value, dotdecs );
            } else if ( current_type == W_FRAME_TYPE ||
                        current_type == RW_FRAME_TYPE ) {

                ask_or_do_invalidation_then(page, dotdecs );
            }
	}

        // If there are writes remaining : error.
        CFATAL( !writes_by_ressource.empty(), "A write was associated with an unknown argument (check frame/objects)");
        // If there are TDecs left, it is ok:
        for ( auto i = tdecs_by_ressource.begin(); i!= tdecs_by_ressource.end();++i) {
            if ( i->ref != NULL ) {
                FATAL( "Non null tdec reference associated with unknown frame.");
            }
            // Anonymous TDecs
            FATAL( "Anonymous tdecs unsupported");
            ask_or_do_tdec(i->page);
        }


	
    }


    static void before_code() {

    }

    static void after_code() {

    }

};
