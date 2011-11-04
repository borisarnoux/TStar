#include <stdio.h>
#include <omp.h>
#include <list>

#include <misc.h>
#include <frame.h>


__thread int delegator_flag = 0;

void delegator_only() {
  CFATAL( delegator_flag == 0, "Delegator only function called outside delegator");
}



class Callable {
public :
  virtual void operator()() = 0;
};

template<typename T> 
Callable * new_Closure( int _sc, T * _lambda ) {
  struct Closure : Callable {
	T* lambda;
	int sc;

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
#define DELEGATE( delegator, block ) (delegator).delegate( new_Closure( 1, block ) )


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
  	  CFATAL(delegator_flag == 1, "Nested delegators" );
	  delegator_flag = 1;
	  (*c)();
	  delegator_flag = 0;
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
  omp_set_num_threads( 5 );
#pragma omp parallel
{
	for ( int i = 0; i < 10000; ++ i ) {
	  DELEGATE( delegator, { incr(); } );
	}
}

  
  printf( "Counter : %d\n", counter);

}

