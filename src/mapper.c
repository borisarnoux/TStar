#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "log.h"


#include "mapper.h"

#define PAGE_LEN 4096

size_t zone_len = 0;
void * start_addr = NULL;
int nnodes = 0;


static void mapper_map_private( void * addr, size_t len ) {
	if ( mmap( addr, len, PROT_READ|PROT_WRITE|PROT_EXEC,
			MAP_ANONYMOUS|MAP_PRIVATE, -1, 0 ) != addr ) {
		perror( "mmap");
		FATAL( "Couldn't map at address %p", addr ); 
	}

}


void mapper_initialize_address_space ( void * base, size_t len_pernode, int _nnodes ) {
	nnodes = _nnodes;
	zone_len = (~(PAGE_LEN-1))& len_pernode;
	
	// Aligns the base...
	start_addr = (void *) ( (~(uintptr_t) (PAGE_LEN-1))& (uintptr_t) base );

	// Does the Mapping :

	for ( int i = 0; i < nnodes; ++ i )  {
		mapper_map_private( start_addr + i * zone_len, zone_len );
		LOG( "Mapped at : %p", start_addr + i * zone_len ); 
	}
	

}


int mapper_who_owns( void * ptr ) {
	CFATAL( start_addr == NULL, "Uninitialized mapper");

	uintptr_t val = (uintptr_t) ptr - (uintptr_t) start_addr;
	
	int retval = val/zone_len;
	
	CFATAL( retval >= nnodes, "Invalid pointer"  );

	return retval;
	 
}


