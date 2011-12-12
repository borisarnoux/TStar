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

#define new_Closure( counter, block ) new_Closure( counter,  new auto([=]() {block} ))
#define new_Closure_R( counter, block ) new_Closure( counter,  new auto([&]() {block} ))

#define DELEGATE( delegator, block ) (delegator).delegate( new_Closure( 1, block ) )
#define DELEGATE_R( delegator, block ) (delegator).delegate( new_Closure_R( 1, block ) )


typedef boost::recursive_mutex RecMutex;
typedef boost::lock_guard<RecMutex> ScopedLock;

class Delegator {

  std::list<Closure*> delegations;
  RecMutex recmutex;
  int occupied;
  int fastl;

public :
  static Delegator default_delegator;

  Delegator() : occupied(0),fastl(0) {}

  void delegate( Closure *c ) {
      {
          ScopedLock scl(recmutex);
          delegations.push_back( c );
      }


       while(! __sync_bool_compare_and_swap(&fastl,0,1) );

        if ( occupied ==  0 ) {
          occupied = 1;

          __sync_synchronize();
          do {
                fastl = 0;
                DEBUG( "In delegate...");
                do_delegations();
            } while(! __sync_bool_compare_and_swap(&fastl,0,1));

           occupied = 0;
           __sync_synchronize();
           fastl = 0;
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
          //CFATAL(delegator_flag == 1, "Nested delegators" );
          delegator_flag += 1;
          set_current_color(33);
          DEBUG("[In delegator] : code at %p ", c);
	  (*c)();
          DEBUG( "[Out of delegator]");
          set_current_color(0);
          delegator_flag -= 1;
	}
  }
  


};



#endif
