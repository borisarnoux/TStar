#ifndef DELEGATOR_H
#define DELEGATOR_H

#include <stdio.h>
#include <omp.h>
#include <list>

#include <misc.h>
#include <frame.hpp>
#include <mpsc.h>

#include  <boost/thread/recursive_mutex.hpp>
#include <boost/thread/locks.hpp>

extern __thread int  delegator_flag;

static inline void delegator_only() {
#if DEBUG_ADDITIONAL_CODE
  CFATAL( delegator_flag == 0, "Delegator only function called outside delegator");
#endif
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
Closure * new_Closure( int _sc, T _lambda ) {
  struct LocalClosure : Closure {
        T lambda;
	


        LocalClosure( int _sc, const T& _lambda) :
		Closure(_sc), lambda(_lambda) {}

	

	virtual void operator() () {
          lambda();
	  delete this;
	}
  };

  return new LocalClosure( _sc, _lambda );

}

#define new_Closure( counter, block ) new_Closure( counter,  [=]() {block} )
#define new_Closure_R( counter, block ) new_Closure( counter,  [&]() {block} )

#define DELEGATE( delegator, block ) (delegator).delegate( new_Closure( 1, block ) )
#define DELEGATE_R( delegator, block ) (delegator).delegate( new_Closure_R( 1, block ) )




/* This class is a delegation lock.
   What it does is to execute some code ( a closure here ), and
   if the lock is not available, some other thread does the
   work a bit later. */

/* The requirements for this class are :
    - To have mutual exclusion.
    - To have immediate execution of the delegation.
    (that is, delegation posted must be executed right after the code
    holding the lock )
    - Reentrancy : delegating in delegator should be equivalent
    to executing synchronously.
    WARNING : for now, only one delegator is supported at a time.
    */

class Delegator {

  MPSC<Closure*> delegations;
  int counter;


public :
  static Delegator default_delegator;

  Delegator() : counter(0) {}

  void delegate( Closure *c ) {
      CFATAL( c == NULL, "Cannot delegate null closure.");
      static __thread int in_delegator = 0;

      // Executes linearly if in delegator :
      if ( in_delegator ) {
          (*c)();
          return;
      }

      // Otherwise add to the queue first.
      delegations.push( c );



      // Then compete for a ticket
      int ticket = __sync_fetch_and_add(&counter,1);
      if ( ticket == 0 ) {
          in_delegator ++;
          do {
              do_delegation();
          } while ( __sync_sub_and_fetch(&counter,1) != 0 );
          in_delegator --;
      } else {
          // Do nothing : we are guaranteed to be taken care of.
      }



	
  }


private:

  void do_delegation() {

	  Closure * c = 0;

          while ( c == 0 )
              c = delegations.pop();
          CFATAL( c == NULL, "Invalid count..");


	  // Execute the delegation.
          //CFATAL(delegator_flag == 1, "Nested delegators" );
          delegator_flag += 1;
          set_current_color(33);
          //DEBUG("[In delegator] : code at %p ", c);
	  (*c)();
          //DEBUG( "[Out of delegator]");
          set_current_color(0);
          delegator_flag -= 1;

  }
  


};




#endif
