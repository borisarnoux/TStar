#ifndef TSTARIFACE_H
#define TSTARIFACE_H

#include <stdlib.h>
#include <stdint.h>
#include <misc.h>

#ifdef __cplusplus
extern "C" {
#endif


struct frame_struct {
#ifdef DEBUG_ADDITIONAL_CODE
  // TODO : remove it.
  uint32_t canaribuf[10];
#endif
  struct static_data * static_data;
  long sc;
  void* args[];
};

struct static_data {
  void (*fn)();
  int nargs;
  long arg_types[];
};


void tstar_setup(int argc, char ** argv);
void tstar_tdec( void * target, void * reference );
struct frame_struct * tstar_tcreate( int sc );
struct frame_struct * tstar_getcfp();
void tstar_tend();
void tstar_twrite( void * object, void * page, size_t offset, void * buffer, size_t size );

int tstar_main( struct frame_struct* first_task);

#ifdef __cplusplus
}
#endif




#endif // TSTARIFACE_H
