#include "tstariface.h"

#include <stdlib.h>
#include <string.h>


#include <mapper.h>
#include <network.hpp>
#include <node.h>
#include <scheduler.hpp>
#include <frame.hpp>
#include <misc.h>
#include <owm_mem.hpp>
#include <invalidation.hpp>
#include <delegator.hpp>
#include <typetuple.hpp>

#ifdef __x86_64
#define ZONE_START 0x70ef55b44000
#else
#define ZONE_START 0x70000000
#endif

NetworkInterface ni;
Scheduler *sched;

static bool initialized = false;

void tstar_setup(int argc, char ** argv) {
    if ( initialized ) {
        FATAL("Double TStar RT initialization.");
    }
    initialized = true;

    // Create a network interface, embedding parameters ( here MPI )
    ni.init(&argc, &argv);


    // Setup address space.
    DEBUG( "Inititalizing address space with %d nodes and %d threads per node", get_num_nodes(), get_num_threads() );
    mapper_initialize_address_space((void*)ZONE_START, 0x40000, get_num_nodes(), get_num_threads());

    // Creates a scheduler :
    sched = new Scheduler(ni);
}

void tstar_tdec( void * target, void * reference ) {
    ExecutionUnit::local_execution_unit->tdec((frame_struct*)target, reference);

}

struct frame_struct * tstar_tcreate( int sc, size_t size ) {
    CFATAL( !initialized, "Initialization of tstar needed before creating a task"
            " please call tstar_setup first");
    // Needs to register the frame as created
    // (done in owm)

    struct frame_struct * f =  (struct frame_struct*)owm_malloc(size);
    f->sc = sc;

    return f;

}

struct frame_struct * tstar_getcfp() {
    return ExecutionUnit::local_execution_unit->current_cfp;
}


void tstar_twrite( void * object, void * page, size_t offset, void * buffer, size_t size ) {
    ExecutionUnit::local_execution_unit->register_write(object,page,offset,buffer,size);
}

void tstar_tend() {
    // nothing to do.
}

void tstar_teardown(int code) {
    NetworkInterface::bcast_exit(code);
}



typedef THANDLE(_input(),_output()) dummy_task;

static dummy_task *dummy_task_factory() {

    return  TASK( 1, dummy_task, _code(
                                 DEBUG( "Program start on node %d", get_node_num());
                                 ) );
}

// We pass a factory method to this one because
// memory allocation on the global system is not
// possible before it is initialized.
// TODO : think about a way to get past this : )
int tstar_main( struct frame_struct* (*taskinit)()) {
    if ( !initialized ) {
        FATAL( "Initialization required : tstar_setup");

    }

    if ( get_node_num() == 0 ) {
             DEBUG( "Launching main task on node : %d ", get_node_num());
             sched->start( taskinit );
    } else {
             // Initialize with the dummy task factory.
             sched->start( (task_factory) dummy_task_factory );
    }


    ni.finalize();

    return 0;
}
