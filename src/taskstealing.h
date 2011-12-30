#ifndef TASKSTEALING_H
#define TASKSTEALING_H


#include <stdlib.h>
#include <misc.h>
#include <list>
#include <algorithm>
#include <pthread.h>
#include <delegator.hpp>


#include <node.h>

template <typename T>
class TaskQueue {
    typedef T* TPtr;
    typedef std::list<TPtr*> FreeList;

    static const int initial_size = 16;


    struct item_t {
        item_t() :
            size(16),
            ap(new TPtr[16]) {}

        long size;
        TPtr *ap;
    };

    item_t * wsq;
    long top;
    long bottom;
    FreeList free_list;


public:
    static T* const EMPTY;
    static T* const ABORT;

    TaskQueue() :
        wsq(new item_t),
        top(0), bottom(0)
        {


    }

    virtual ~TaskQueue() {
        for ( auto z = free_list.begin();
              z != free_list.end();
              ++z ) {
            delete[] *z;

        }
    }

    int size() {
        return bottom-top;
    }

    item_t * expand() {
      int newsize = wsq->size*2;
      TPtr * newitems = new TPtr[newsize];
      item_t *newq = new item_t;
      for ( long i = top; i < bottom; ++i ) {
          newitems[i%newsize] = wsq->ap[i%wsq->size];
      }
      newq->size = newsize;
      newq->ap = newitems;
      //fence("store-store");
      __sync_synchronize();

      wsq = newq;
      free_list.push_back(wsq->ap);

      while ( free_list.size() > 4 ) {
          // TODO Fix me : non safe memory freeing...
          // Leave a (bounded) memleak instead ?
          delete[] free_list.front();
          free_list.pop_front();
      }
      return newq;
    }


    TPtr take() {
      long b = bottom-1;
      item_t *q = wsq;
      bottom = b;
      // fence("store-load");
      __sync_synchronize();
      long t = top;
      if ( b < t ) {
       bottom = t;
       return EMPTY;
      }
      TPtr task = q->ap[b % q->size ];
      if (b > t) {
        return task;
      }
      if ( !__sync_bool_compare_and_swap(&top, t, t+1 ) ) {
        return EMPTY;
      }
      bottom = t+1;
      return task;
    }


    void push( TPtr task ) {
      long b = bottom;
      long t = top;
      item_t * q = wsq;
      if ( b-t >= q->size - 1 ) {
        q = expand();
      }

      q->ap[b% q->size] = task;
      //fence("store-store");
      __sync_synchronize();

      bottom = b+1;
    }




    TPtr steal() {
      long t = top;
      //fence("load-load");
      __sync_synchronize();

      long b = bottom;
      //fence("load-load");
      __sync_synchronize();

      item_t * q = wsq;
      if ( t >= b) {
        return EMPTY;
      }
      TPtr task=q->ap[t % q->size];
      //fence("load-store");
      __sync_synchronize();

      if ( !__sync_bool_compare_and_swap(&top, t, t+1 ) ) {
        return ABORT;
      }

      return task;
    }


};

template <typename T>
T*const TaskQueue<T>::EMPTY = (T*const)0;
template <typename T>
T*const TaskQueue<T>::ABORT = (T*const)-1;

// A task scheduler with OMP tasks compatible interface.
class TaskScheduler {
    int nthreads;
    void (*executor)(frame_struct*);
    typedef frame_struct * Task;
    typedef TaskQueue<frame_struct> FSTaskQueue;
    FSTaskQueue *queues;
    volatile bool *finished;
    volatile bool *stealing;
    pthread_t *threads;

    struct entry_struct {
        entry_struct(TaskScheduler*_ts, int _id, Task (*_tgen)() ):
        ts(_ts),
        id(_id),
        tgen(_tgen)
        {}
        TaskScheduler *ts;
        int id;
        Task (*tgen)();
    };

    void thread_mainloop() {
        // Requires finished initialized to 0s
        // And one task in one of the queues.
        bool in_stealing = false;
        bool in_finished = false;

        while ( true ) {
            if ( in_stealing) {
                if ( std::count(stealing, stealing+nthreads, true) == nthreads ) {
                    // We are finished.
                    finished[get_thread_num()] = true;
                    in_finished = true;
                } else {
                    finished[get_thread_num()] = false;
                    in_finished = false;
                }
            }

            if ( in_finished ) {
                if ( std::count(finished, finished+nthreads, true) == nthreads ) {
                       break;

                }
            }


            Task to_exec = queues[get_thread_num()].take();
            if ( to_exec != FSTaskQueue::EMPTY ) {
                executor(to_exec);
            } else {
                // Signal finished :
                stealing[get_thread_num()] = true;
                in_stealing = true;
                // Try to steal work x times
                for ( int i = 0; i < 5; ++ i ) {
                    int r = 0;
                    r = rand()%get_num_threads();

                    if  ( r == get_thread_num() || finished[r] ) {
                        continue;
                    }

                    Task t = queues[r].steal();
                    if ( t != FSTaskQueue::EMPTY && t!= FSTaskQueue::ABORT ) {
                        stealing[get_thread_num()] = false;
                        in_stealing = false;
                        queues[get_thread_num()].push(t);
                        break;
                    }
                }
            }
        }

    }




public :
    TaskScheduler( int _nthreads, void(*_executor)(frame_struct*))
        : nthreads(_nthreads),
          executor(_executor),
        queues( new FSTaskQueue[_nthreads]),
        finished(new bool[_nthreads]),
        stealing(new bool[_nthreads]),
        threads(new pthread_t[_nthreads]) {}

    virtual ~TaskScheduler() {
        delete queues;
        delete finished;
        delete stealing;
        delete threads;

    }

    // To call from within a thread only...
    void schedule( Task t ) {
        CFATAL( get_thread_num()==-1, "Invalid use of schedule.");
        queues[get_thread_num()].push(t);
    }


    static void entry_func( struct entry_struct *e ) {
        set_thread_num(e->id);
        if ( get_thread_num() == 0 ) {
            e->ts->queues[0].push( e->tgen() );
        }
        e->ts->thread_mainloop();
        delete e;
    }


    void start( Task (*tgen)()) {

        for ( int i = 0; i < nthreads; ++ i ) {
            pthread_create( &threads[i],
                            NULL,
                            (void *(*)(void*))entry_func,
                            new entry_struct(this,i, tgen));
        }

        for ( int i = 0; i < nthreads; ++i ) {
            pthread_join( threads[i], NULL);

        }

    }

    int tasks_inside() {
        int retval = 0;

        for ( int i = 0; i < nthreads; ++i  ) {
            retval += queues[i].size();
        }

        return retval;

    }
};

#endif // TASKSTEALING_H
