
#ifndef NGPSV_FUTEX_H
#define NGPSV_FUTEX_H


#include <linux/futex.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <stdbool.h>

#define FENCE __sync_synchronize()
#define ATOM_INCR( val ) while ( ! BOOL_CAS( val, *(val),*(val)+1) )
#define futex(...) syscall(SYS_futex, ##__VA_ARGS__)
#define futex_wait(x,val) futex(x,FUTEX_WAIT,val, NULL,NULL,0)
#define futex_wake(x,val) futex(x,FUTEX_WAKE,val, NULL,NULL,0)

static inline bool futex_waitif( int *ft, bool(*cond) (void*), void * arg) {
	int a = ft[0];
	if ( cond(arg) ) {
		futex_wait(ft ,a);
		return true;
	}
	return false;
}

#endif
