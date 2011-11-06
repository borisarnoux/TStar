#include  <boost/unordered_map.hpp>

#include <vector>

#include <hashint.h>
#include <frame.h>
#include <delegator>






template<typename KeyType>
class TaskMapper {
public: 
   typedef boost::unordered_multimap<KeyType,Closure*> MapType;
   typedef boost::recursive_mutex RecMutex;
   typedef boost::interprocess::scoped_lock ScopedLock;

   int mapsn;
   Vector<MapType> maps;
   Vector<RecMutex> mutexes;

private:
   TaskMapper( int _mapsn, ):
      mapsn(_mapsn), maps(_mapsn), mutexes(_mapsn) {
          
        
        

   }


   void register_evt( KeyType k, Closure * t ) {
      int idx = hashzone(&k,sizeof(k));
      ScopedLock scpl( mutexes[ idx ] );


      maps[idx].emplace( MapType::value_type(k,t) );

   }

   void signal_evt( KeyType k ) {

      // TODO : Generic but only works when packed....
      int lock_index = hashzone(&k,sizeof(k));
      ScopedLock scpl( mutexes[ hashint(k) ] );
      

      typedef std::pair<MapType::iterator> Range;
      
      // Find all occurences of Key K.
      Range all_occurences = maps[idx].equal_range( k );

      for ( MapType::iterator i = all_occurences.first();
            i != all_occurences.second;
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


