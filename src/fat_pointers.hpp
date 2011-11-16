#ifndef FAT_POINTER_H
#define FAT_POINTER_H

#include <frame.hpp>
#include <string.h>



struct ressource_desc {
  int perms;
  void * page;
};

struct fat_pointer_buffer {
  int smart_count; // GC count
  int elt_count; // Size of container.
  int use_count; // Used slots
  struct ressource_desc elements[];
};


typedef struct fat_pointer_buffer * fat_pointer_p;



fat_pointer_p create_fatp( size_t nelts );

void append_fatp_to_fatp( fat_pointer_p , int perms, fat_pointer_p fatp );

void notify_fatp_transfer( fat_pointer_p fatp );




#endif


