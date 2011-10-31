#include  <boost/unordered_map.hpp>

#include <vector>

#include <hashint.h>
#include <frame.h>

class LocalTask {
private :
  int sc;

public:
  
  void tdec() {
    assert( sc > 0 );
    sc -= 1;
    
    if ( sc == 0 ) {
      (*this)()
    }  
  }
  
  virtual void operator()() = 0;

protected :
  LocalTask( int _sc ) : sc(_sc ) {}

}


template<typename T>
class ClosureLocalTask : LocalTask {
  T * lambda;

public:
  ClosureLocalTask( int _sc, T * _lambda )
  : LocalTask(_sc), lambda(_lambda) {
    
  }

  void operator() () {
    (*lambda)();
    // Task is one shot.
    delete this;

  }

  virtual ~ClosureLocalTask() {
    delete lambda;

  }
}

#define new_ClosureLocalTask( count, block ) new ClosureLocalTask( count, new auto([=]() block ) )


template<typename KeyType>
class TaskMapper {
public: 
   typedef boost::unordered_multimap<KeyType,LocalTask*> MapType;
   typedef boost::recursive_mutex RecMutex;
   typedef boost::interprocess::scoped_lock ScopedLock;

   int mapsn;
   Vector<MapType> maps;
   Vector<RecMutex> mutexes;

private:
   TaskMapper( int _mapsn, ):
      mapsn(_mapsn), maps(_mapsn), mutexes(_mapsn) {
          
        
        

   }


   void register_evt( KeyType k, LocalTask t ) {
      int idx = hashzone(&k,sizeof(k));
      ScopedLock scpl( mutexes[ idx ] );


      maps[idx].emplace( MapType::value_type(k,t) );

   }

   void activate_evt( KeyType k ) {

      // TODO : Generic but only works when packed....
      int lock_index = hashzone(&k,sizeof(k));
      ScopedLock scpl( mutexes[ hashint(k) ] );
      

      typedef std::pair<MapType::iterator> Range;
      
      // Find all occurences of Key K.
      Range all_occurences = maps[idx].equal_range( k );

      for ( MapType::iterator i = all_occurences.first();
            i != all_occurences.second();
            ++ i ) {
          // Calls Tdec ( auto cleanup )
          i->second()->tdec();
          
      }
      // Entries cleanup.
      maps[idx].erase( all_occurences );


   }

   virtual ~TaskMapper( ) {
       
   }

}


