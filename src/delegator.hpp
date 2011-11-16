#ifndef DELEGATOR_H
#define DELEGATOR_H

#include <stdio.h>
#include <omp.h>
#include <list>

#include <misc.h>
#include <frame.hpp>
// This is a C++ header.

#include  <boost/thread/recursive_mutex.hpp>
#include <boost/thread/locks.hpp>

extern __thread int  delegator_flag;

static inline void delegator_only() {
  CFATAL( delegator_flag == 0, "Delegator only function called outside delegator");
}



class Closure {
private:
  int sc;

public :
  virtual void operator()() = 0;
  
  virtual void tdec() {
	  sc --;
	  if ( sc == 0 ) {
		(*this)();
	  }
	}

protected:	
  Closure( int _sc ) : sc(_sc) {}

};

template<typename T> 
Closure * new_Closure( int _sc, T * _lambda ) {
  if ( _sc == 0 ) {
    //FATAL( "FATAL : Closure must be delayed, use call instead." );
  }

  struct LocalClosure : Closure {
	T* lambda;
	


	LocalClosure( int _sc, T* _lambda) :
		Closure(_sc), lambda(_lambda) {}

	

	virtual void operator() () {
	  (*lambda)();
	  delete lambda;
	  delete this;
	}
  };

  return new LocalClosure( _sc, _lambda );

}

#define new_Closure( counter, block ) new_Closure( counter,  new auto([=]() block ))
#define DELEGATE( delegator, block ) (delegator).delegate( new_Closure( 1, block ) )



typedef boost::recursive_mutex RecMutex;
typedef boost::lock_guard<RecMutex> ScopedLock;

class Delegator {
  std::list<Closure*> delegations;
  int occupied;
  RecMutex recmutex;

public :
  Delegator() : occupied(0) {}

  void delegate( Closure *c ) {
	{
          ScopedLock scl(recmutex);
	  delegations.push_back( c );
	}
	
	if ( __sync_bool_compare_and_swap( &occupied, 0, 1 ) ) {
	  do_delegations();
	  __sync_bool_compare_and_swap( &occupied, 1, 0 );
	}

	
  }


private:

  void do_delegations() {
        while ( true ) {
	  Closure * c = 0;
	  {
                ScopedLock scl( recmutex );

                if ( delegations.empty() ) {
                    break;
                }
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



#endif
