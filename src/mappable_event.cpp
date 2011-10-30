#include  <boost/unordered_map.hpp>

#include <hashint.h>


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
  virtual void cleanup();

}


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


   void register( KeyType k, LocalTask t ) {

   }

   void activate( KeyType k ) {

      // TODO : Generic but only works when packed....
      int lock_index = hashzone(&k,sizeof(k));

      ScopedLock scpl( mutexes[ hashint(k) ] );
      
      
      // Find all occurences of Key K.
      

      // Call tdec on it ( tdec autocleans )
      // Remove all keys.    
   }

   virtual ~TaskMapper( ) {
       
   }

}


