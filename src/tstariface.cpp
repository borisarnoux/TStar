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
    mapper_initialize_address_space((void*)ZONE_START, 0x40000, get_num_nodes());

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


typedef THANDLE( _input(), _output() ) dummy_task;


int tstar_main( struct frame_struct* first_task) {
    if ( !initialized ) {
        FATAL( "Initialization required : tstar_setup");

    }





    if ( get_node_num() == 0 ) {


#pragma omp parallel
{
            sched->tls_init();
#pragma omp single
            {
                DEBUG( "Launching main task on node : %d ", get_node_num());
                sched->schedule_global( first_task);

            }
}
    } else {
       // Create a dummy task :
        dummy_task * dt = TASK( 1, dummy_task, _code(
                                    DEBUG( "Program start on node %d", get_node_num());
                                    ) );
#pragma omp parallel
{
         sched->tls_init();

#pragma omp single
{
             sched->schedule_global( (struct frame_struct*)dt);


}

}
    }


    ni.finalize();

    return 0;
}
