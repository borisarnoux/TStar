#include <stdlib.h>
#include <stdio.h>
#include <tstariface.h>
#include <typetuple.hpp>
#include <map>

struct example_t {
  int content;

  example_t( int _c ) : content(_c) {}
  example_t() : content( 0 ) {}

  example_t operator+( const example_t& a) const {
	return example_t(a.content + content);
  }  

  std::pair<example_t,example_t> split() const { 
	return std::pair<example_t,example_t>( example_t(content-1), example_t(content-2) );

  }

  bool is_ready() const  {
	return content==0||content==1;
  }

  void print() const {
	  printf( "%d\n", content );
  }
};





template <typename T>
struct MapReducer {

typedef THANDLE(
			_input(),
			_output( vdef(OutM), T ) ) map_task;

typedef THANDLE(
			_input( vdef(InA), T, vdef(InB), T ),
			_output( vdef(OutR), T ) ) reduce_task;

reduce_task * creduce_task() {
	return TASK( 3, reduce_task,

				_code (
					provide( OutR, T, get_arg( InA, T) + get_arg( InB, T )); 	
				  
				  ) );

}

map_task * cmap_task( T & in ) {
	  return TASK( 1, map_task,
		_code ( 
				if ( in.is_ready() ) {
				  provide( OutM, T, in );
				  return;
				}

				std::pair<T,T> splitted_pair = in.split();
				map_task * t1 = this->cmap_task( splitted_pair.first );
				map_task * t2 = this->cmap_task( splitted_pair.second );
				
				reduce_task * reducer = this->creduce_task();
				
				bind( t1, OutM, reducer, InA, T );
				bind( t2, OutM, reducer, InB, T );


				bind_outout( reducer, OutR, OutM );
				
				tstar_tdec( t1, NULL );
				tstar_tdec( t2, NULL );
				
				tstar_tdec( reducer, NULL );
				
				

			));

  }
  
  T compute( T in ) {
	map_task * main_task = cmap_task( in );
	T out;

	main_task->get_offset(vname(OutM)) = 0;
	main_task->get_framep(vname(OutM)) = (struct frame_struct *) &out ;
	
	
  
#pragma omp parallel
{
#pragma omp single
  tstar_tdec( main_task, NULL );
}
  
  
  return out;

  }
};


int main(int argc, char ** argv ) { 
	if ( argc < 2 ) {
	  fprintf( stderr, "Provide fib arg.\n");
	  exit( EXIT_FAILURE );
	}
  
	MapReducer<example_t> m;

	example_t out = m.compute( example_t( atoi(argv[1] ) ) );

	printf( "Result :\n");
	out.print();
}

