#include <fat_pointers.hpp>



void append_page_to_fatp( void * page, int perms, fat_pointer_p fatp ) {
  CFATAL( !PAGE_IS_RESP( fatp ), "Should be resp to create fatp structure") ;
}

fat_pointer_p create_fatp( size_t nelts ) {
  struct fat_pointer_buffer * p = (struct fat_pointer_buffer*) 
	owm_malloc( sizeof( struct fat_pointer_buffer ) + nelts * sizeof( struct ressource_desc ) );
  p->smart_count = 1;
  p->elt_count = nelts;
  p->use_count = 0;
  memset( p->elements, 0, nelts * sizeof( struct ressource_desc ) );

  return p;
}


void notify_fatp_transfer( fat_pointer_p fatp ) {
  // Special message or increase depending on RESP.
  // Effect : smart_count ++;
}

void fatp_recursive_cleanup( fat_pointer_p fatp ) {
  // Executes actual dec and  cleanup if 0 locally.
  // Or sends a message for that.

}
