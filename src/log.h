

#include <stdio.h>
#include <stdlib.h>


#define LOG(msg, ...) fprintf( stderr, "%s:%d\t" msg "\n", __FILE__, __LINE__, ## __VA_ARGS__ )


#define FATAL(...) do { LOG("" __VA_ARGS__ ) ; exit( EXIT_FAILURE ); } while(0)

#define CFATAL( c, ... ) 
