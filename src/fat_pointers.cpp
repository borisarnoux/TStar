#include <fat_pointers.hpp>
#include <owm_mem.hpp>
#include <misc.h>

void append_page_to_fatp( void * page, int perms, fat_pointer_p fatp ) {
  CFATAL( !PAGE_IS_RESP( fatp ), "Should be resp to create fatp structure") ;

  ressource_desc r = {perms,page};
  fatp->elements[fatp->use_count] = r;
  fatp->use_count += 1;

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

