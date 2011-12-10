#include "tstariface.h"
#include <owm_mem.hpp>
#include <scheduler.hpp>
#include <stdlib.h>

void tstar_setup() {

}

void tstar_tdec( void * target, void * reference ) {
    ExecutionUnit::local_execution_unit->tdec((frame_struct*)target, reference);

}

struct frame_struct * tstar_tcreate( int sc, size_t size ) {
    // Needs to register the frame as created
    // (done in owm)

    struct frame_struct * f =  (struct frame_struct*)owm_malloc(size);
    f->sc = sc;

    return f;

}

struct frame_struct * tstar_getcfp() {
    return ExecutionUnit::local_execution_unit->current_cfp;
}

void tstar_tend() {
    // nothing to do.
}


