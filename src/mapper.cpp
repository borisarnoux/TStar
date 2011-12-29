//#define _GNU_SOURCE

#include <boost/interprocess/managed_external_buffer.hpp>

#include <node.h>

#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <misc.h>
#include <mpsc.h>
#include <set>

#include "mapper.h"

#define PAGE_LEN 4096

#if DEBUG_ADDITIONAL_CODE
std::set<void*> valid_pages_set;

void mapper_valid_address_add( void * ptr ) {
#pragma omp critical (valcheck)
    {
    valid_pages_set.insert(ptr);
    }
}
void mapper_valid_address_check( void * ptr ) {
#pragma omp critical (valcheck)
    {
    if ( valid_pages_set.find(ptr) == valid_pages_set.end() ) {
        current_color = 33;
        DEBUG( "WARNING ! Pointer %p was not in valid set.", ptr);
        current_color = 0;
    }
    }
}
void mapper_valid_address_rm( void * ptr)  {

    mapper_valid_address_check(ptr);
#pragma omp critical (valcheck)
    {
    valid_pages_set.erase(ptr);
}

}

__attribute__((destructor))
void mapper_exit_check() {
    if ( ! valid_pages_set.empty() ) {
        DEBUG("Warning !! Not all locally allocated pages were freed.");
    }
}


#endif




using namespace boost::interprocess;


static managed_external_buffer ** local_heap;
static MPSC<void*> *queues;
bool initialized = false;

size_t zone_len = 0;
char * start_addr = NULL;
char * node_start = NULL;
int nthreads = 0;
int nnodes = 0;


static void check_legal_address( void * zone ) {
    intptr_t v = (intptr_t)zone;
    CFATAL( (v-(intptr_t)node_start) < 0 ||  (v-(intptr_t)node_start), "Pointer is from different node." );
}


static void mapper_map_private( void * addr, size_t len ) {
    DEBUG( "Mapping at : %p to %p", addr, (void*)((uintptr_t) addr + len));
    if ( mmap( addr, len, PROT_READ|PROT_WRITE|PROT_EXEC,
               MAP_ANONYMOUS|MAP_PRIVATE, -1, 0 ) != addr ) {
        perror( "mmap");
        FATAL( "Couldn't map at address %p", addr );
    }

}


void mapper_initialize_address_space ( void * base, size_t len_pernode, int _nnodes, int threads_pernode ) {
    CFATAL( len_pernode%threads_pernode != 0, "Threads per node must divide len...");
    CFATAL( local_heap != NULL, "local heap already initialized." );
    CFATAL( threads_pernode <= 0 || _nnodes <= 0, "Negative or null thread or node number.");
    nthreads = threads_pernode;
    local_heap = new managed_external_buffer*[threads_pernode];
    queues = new MPSC<void*>[threads_pernode];

    if ( initialized ) {
        FATAL( "Double initialization of address space.");
    } else {

    }

    nnodes = _nnodes;
    zone_len = (~(PAGE_LEN-1))& len_pernode;

    // Aligns the base...
    start_addr = (char *) ( (~(uintptr_t) (PAGE_LEN-1))& (uintptr_t) base );

    // Does the Mapping :

    for ( int i = 0; i < nnodes; ++ i )  {
        char *local_start = start_addr + i * zone_len;


        if ( i == get_node_num() ) {
            node_start = local_start;
            for ( int j = 0; j < threads_pernode; ++j ) {
                mapper_map_private( start_addr + i * zone_len + j*(zone_len/threads_pernode), zone_len/threads_pernode );
                local_heap[j] = new managed_external_buffer(create_only, local_start + j*(zone_len/threads_pernode), zone_len/threads_pernode);

            }

        } else {
            // Or we map at bigger grain...
            mapper_map_private( local_start, zone_len );
        }

        DEBUG( "Mapped at : %p", start_addr + i * zone_len );
    }


}


int mapper_node_who_owns( void * ptr ) {
    CFATAL( start_addr == NULL, "Uninitialized mapper");

    uintptr_t val = (uintptr_t) ptr - (uintptr_t) start_addr;

    int retval = val/zone_len;

    //DEBUG( "%d owns %p", retval, ptr);
    return retval;

}

int mapper_thread_who_owns( void * ptr ) {
    CFATAL( node_start == NULL, "Uninitialized mapper");

    uintptr_t val = (uintptr_t) ptr - (uintptr_t) node_start;

    int retval = val/(zone_len/nthreads);

    return retval;

}

void * mapper_malloc(size_t len) {
    void * retval = NULL;

#if DEBUG_ADDITIONAL_CODE
    CFATAL( !local_heap[get_thread_num()]->check_sanity(), "Corrupted heap.");
#endif

    retval = local_heap[get_thread_num()]->allocate( len );

    CFATAL( retval == 0, "No memory left.");

    return retval;
}

void mapper_free( void * zone ) {
    CFATAL( mapper_node_who_owns(zone) != get_node_num(), "Cannot free pointer out of zone( %p )", zone );

    void * additional_to_free;
    while( (additional_to_free = queues[get_thread_num()].pop())!= NULL) {
        local_heap[get_thread_num()]->deallocate(additional_to_free);
    }

#if DEBUG_ADDITIONAL_CODE
    CFATAL( !local_heap[get_thread_num()]->check_sanity(), "Corrupted heap.");
#endif
    if ( get_thread_num() == mapper_thread_who_owns(zone) ) {
        local_heap[get_thread_num()]->deallocate( zone );
    } else {
        DEBUG( "Pushing %p for freeing to : %d", zone, mapper_thread_who_owns(zone));
        queues[mapper_thread_who_owns(zone)].push(zone);
    }
}


void mapper_check_sanity() {
        CFATAL( !local_heap[get_thread_num()]->check_sanity(), "Corrupted heap.");
}
