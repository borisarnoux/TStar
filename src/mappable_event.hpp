#ifndef TASK_MAPPER_H
#define TASK_MAPPER_H


#include  <boost/thread/recursive_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <vector>
#include <map>

#include <hash_int.h>
#include <frame.hpp>
#include <delegator.hpp>






template<typename KeyType>
class TaskMapper {
private:
   typedef std::multimap<KeyType,Closure*> MapType;
   typedef std::pair<KeyType,Closure*> MapVal;
   typedef boost::recursive_mutex RecMutex;
   typedef boost::interprocess::scoped_lock<RecMutex> ScopedLock;
   typedef std::pair<typename MapType::iterator, typename MapType::iterator> Range;


   int mapsn;
   std::vector<MapType> maps;
   std::vector<RecMutex> mutexes;

public: 
   TaskMapper( int _mapsn ):
      mapsn(_mapsn), maps(_mapsn), mutexes(_mapsn) {
          
        
        

   }


   void register_evt( KeyType k, Closure * t ) {
      int idx = hashzone(&k,sizeof(k));
      ScopedLock scpl( mutexes[ idx ] );


      maps[idx].insert( MapVal(k,t) );

   }

   void signal_evt( KeyType k ) {
      int idx = hashzone(&k,sizeof(k));

      // TODO : Generic but only works when packed....
      int lock_index = hashzone(&k,sizeof(k));
      ScopedLock scpl( mutexes[ hashzone(&k, sizeof(KeyType)) ] );
      

      
      // Find all occurences of Key K.
      Range all_occurences = maps[idx].equal_range( k );

      for ( auto i = all_occurences.first;
            i != all_occurences.second;
            ++ i ) {
          // Calls Tdec ( auto cleanup )
          i->second->tdec();
          
      }
      // Entries cleanup.
      maps[idx].erase( all_occurences.first, all_occurences.second );


   }

   virtual ~TaskMapper( ) {
       
   }

};



#endif