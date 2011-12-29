#ifndef HASHINT
#define HASHINT

#include <string.h>
#include<stdlib.h>
#include <stdio.h>
#include <misc.h>
#include <node.h>

static inline int hashint(int key) {
  int c2=0x27d4eb2d; // a prime or an odd constant
  key = (key ^ 61) ^ (key >> 16);
  key = key + (key << 3);
  key = key ^ (key >> 4);
  key = key * c2;
  key = key ^ (key >> 15);
  return key;
}


static inline int hashzone( void * _z, size_t size) {
  char * z = (char*) _z;
  int r = 0;

  while ( size >= sizeof(int) ) {
    r += *(int*)z;
    r = hashint( r );

    z += sizeof( int );
    size -= sizeof( int );
  }

  if ( size > 0 ) {
    int holder = 0;
    CFATAL(size>sizeof(int), "Assumption failed.");
    memcpy ( &holder, z, size );
    r += holder;
    r = hashint( r );
  }

  if ( r < 0 ) {
      fprintf( stderr,
      "Negative hash value.");
      exit( EXIT_FAILURE);
  }
  return r;
  
}


#endif
