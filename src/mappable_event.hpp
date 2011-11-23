#ifndef TASK_MAPPER_H
#define TASK_MAPPER_H


#include  <boost/thread/recursive_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <vector>
#include <map>
#include <list>

#include <hash_int.h>
#include <frame.hpp>
#include <delegator.hpp>






template<typename KeyType>
class TaskMapper {
private:
   typedef std::multimap<KeyType,Closure*> MapType;
   typedef std::pair<KeyType,Closure*> MapVal;
   typedef boost::recursive_mutex RecMutex;
   typedef boost::lock_guard<RecMutex> ScopedLock;
   typedef std::pair<typename MapType::iterator, typename MapType::iterator> Range;


   int mapsn;
   std::vector<MapType> maps;
   std::vector<RecMutex> mutexes;

   std::list<MapVal> temp_storage;
   bool busy;

   int use_count;
   const char * name;


public: 
   TaskMapper( int _mapsn, const char * _name ):
       mapsn(_mapsn), maps(_mapsn), mutexes(_mapsn), busy(0),use_count(0), name(_name) {
        
        

   }


   void register_evt( KeyType k, Closure * t ) {
       if ( t == NULL ) {
           FATAL( "Unsupported NULL closure for mapping.");
       }
       int idx = hashzone(&k,sizeof(k))%mapsn;
       ScopedLock scpl( mutexes[ idx ] );

       // This busy  indicator serves
       // to keep ranges consistent if
       // this code is called inside a signal
       // event.
       if ( !busy ) {
           maps[idx].insert( MapVal(k,t) );
       } else {
           temp_storage.push_front(MapVal(k,t));
       }
       use_count++;

   }

   void signal_evt( KeyType k ) {
      int idx = hashzone(&k,sizeof(k))%mapsn;

      ScopedLock scpl( mutexes[ idx ] );
      maps[idx].insert(temp_storage.begin(),temp_storage.end());
      temp_storage.empty();
      
      // Find all occurences of Key K.
      busy = 1;
      Range all_occurences = maps[idx].equal_range( k );

      for ( auto i = all_occurences.first;
            i != all_occurences.second;
            ++ i ) {
          // Calls Tdec ( auto cleanup )
          i->second->tdec();
          
      }
      // Entries cleanup.
      maps[idx].erase( all_occurences.first, all_occurences.second );
      busy = 0;


   }

   virtual ~TaskMapper( ) {
       set_current_color(36);
       DEBUG( "TaskMapper %s was used %d times.", name, use_count);
       set_current_color(0);
   }

};



#endif
