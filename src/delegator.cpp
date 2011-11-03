#include <stdio.h>
#include <omp.h>
#include <list>



class Callable {
public :
  virtual void operator()() = 0;
};

template<typename T> 
Callable * new_Closure( int _sc, T * _lambda ) {
  struct Closure : Callable {
	int sc;
	T* lambda;

	Closure( T * _lambda ) :
		lambda(_lambda ), sc(1) {}

	Closure( int _sc, T* _lambda) :
		lambda(_lambda), sc(_sc) {}

	
	void tdec() {
	  sc --;
	  if ( sc == 0 ) {
		(*this)();
	  }
	}

	virtual void operator() () {
	  (*lambda)();
	  delete lambda;
	  delete this;
	}
  };

  return new Closure( _sc, _lambda );

}

#define new_Closure( counter, block ) new_Closure( counter,  new auto([=]() block ))
#define DELEGATE( block ) (DELEGATOR)->delegate( new_Closure( 1, block ) )


class Delegator {
  std::list<Callable*> delegations;
  int occupied;

public :
  Delegator() : occupied(0) {}

  void delegate( Callable *c ) {
	#pragma omp critical
	{
	  delegations.push_back( c );
	}
	
	if ( __sync_bool_compare_and_swap( &occupied, 0, 1 ) ) {
	  do_delegations();
	  __sync_bool_compare_and_swap( &occupied, 1, 0 );
	}

	
  }


private:

  void do_delegations() {
	while ( ! delegations.empty() ) {
	  Callable * c = 0;
	  #pragma omp critical
	  {
		c = delegations.front();
		delegations.pop_front();
	  }

	  // Execute the delegation.
	  (*c)();

	}
  }
  


};


int counter;

void incr() {
  counter ++;
}

#define DELEGATOR (&delegator)
int main () {
  Delegator delegator;

#pragma omp parallel
{
	for ( int i = 0; i < 10000; ++ i ) {
	  DELEGATE( { incr(); } );
	}
}

  printf( "Counter : %d\n", counter);

}

