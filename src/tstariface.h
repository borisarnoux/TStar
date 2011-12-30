#ifndef TSTARIFACE_H
#define TSTARIFACE_H

#include <stdlib.h>
#include <stdint.h>
#include <misc.h>

#ifdef __cplusplus
extern "C" {
#endif


struct frame_struct {
#if DEBUG_ADDITIONAL_CODE
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

typedef frame_struct *(*task_factory)();
void tstar_setup(int argc, char ** argv);
void tstar_tdec( void * target, void * reference );
struct frame_struct * tstar_tcreate( int sc );
struct frame_struct * tstar_getcfp();
void tstar_tend();
void tstar_twrite( void * object, void * page, size_t offset, void * buffer, size_t size );

int tstar_main( task_factory first_factory );

#ifdef __cplusplus
}
#endif




#endif // TSTARIFACE_H
