#ifndef TSTARIFACE_H
#define TSTARIFACE_H



#ifdef __cplusplus
extern "C" {
#endif


struct frame_struct {
  struct static_data * static_data;
  long sc;
  void* args[];
};

struct static_data {
  void (*fn)();
  int nargs;
  long arg_types[];
};


void tstar_setup();
void tstar_tdec( void * target, void * reference );
struct frame_struct * tstar_tcreate( int sc );
struct frame_struct * tstar_getcfp();
void tstar_tend();



#ifdef __cplusplus
}
#endif




#endif // TSTARIFACE_H
