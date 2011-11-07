#include <frame.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct fat_pointer_buffer * fat_pointer_h;


struct fat_pointer_elt {
  int type;
  int perms;
  void * page;
}


struct fat_pointer_buffer {
  int smart_count;
  int elt_count;
  int use_count;
  struct fat_pointer_elt elements[];
}

inline fat_pointer_h create_fatp( size_t nelts ) {
  struct fat_pointer_buffer * p =  owm_malloc( 
        sizeof( struct fat_pointer_buffer ) +
        nelts * sizeof( struct fat_pointer_elt ) );
  p->smart_count = 1;
  p->elt_count = nelts;
  p->use_count = 0;
  memset( p->elements, 0, nelts * sizeof( struct fat_pointer_elt ) );

  return p;
}


void append_page_to_fatp( void * page, int perms, fat_pointer_h fatp ) {
  CFATAL( !PAGE_IS_RESP( fatp ), "Should be resp to create fatp structure") ;


}
void append_fatp_to_fatp( fat_pointer_h , int perms, fat_pointer_h fatp );

void notify_fatp_transfer( fat_pointer_h fatp ) {
  // Special message or increase depending on RESP.
  // Effect : smart_count ++;
}

void fatp_recursive_cleanup( fat_pointer_h fatp ) {
  // Executes actual dec and  cleanup if 0 locally.
  // Or sends a message for that.

}



#ifdef __cplusplus
}
#endif


