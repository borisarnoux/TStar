

#include "hash_int.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct intholder {
  int a;

} __attribute__((packed));


int main( ) {
  for ( int i = 0; i < 100; ++ i ) {
      printf( "%d\n", hashint( i ) );

      struct intholder z;
      z.a  = i;
      printf( "%d\n", hashzone( &z, sizeof(z)) );
          
  
  }

}
