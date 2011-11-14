#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP


#include <omp.h>
#include <list>
#include <map>
#include <scheduler.hpp>
#include <fat_pointers.hpp>
#include <identifiers.h>
#include <data_events.hpp>
#include <invalidation.hpp>
#include <delegator.hpp>

/* This  scheduler class  keeps track of the tasks when it is needed */

struct TDec {
    struct frame_struct * page;
    void * ref;
};

struct DWrite {
    void * obj;
    void * frame;
    void * pos;
    size_t len;
    DWrite(void * _o, void * _f, void * _p, size_t _l) :
        obj(_o),frame(_f),pos(_p),len(_l) {}

};





typedef std::multimap<PageType, DWrite> DWriteMap;
typedef std::multimap<PageType, TDec> TDecMap;
typedef std::list<struct frame_struct*> CreatedFramesList;

class ExecutionUnit {
private:
    static __thread ExecutionUnit * local_execution_unit;

    struct frame_struct * current_cfp;
    // Ressource -> writes.
    DWriteMap writes_by_ressource;
    TDecMap tdecs_by_ressource;
    CreatedFramesList created_frames_list;

public:
    static void executor( struct frame_struct * page );
    void tdec( struct frame_struct * page, void * ref );
    void register_write( void * object, void * frame, void * pos, size_t len );
    void process_commits();
    bool check_ressources();
    static void before_code();
    static void after_code();
};


class Scheduler {
    std::list<struct frame_struct *> external_tasks;
    int global_local_threshold;
    int task_count;


public :

    Scheduler() : global_local_threshold(omp_get_num_threads() * 5 ), task_count(0) {}

    // This is the entry point, should be attained when SC reaches 0.
    void schedule_global( struct frame_struct * page );

    // This function adds the task to an external queue.
    // This queue can be eventually published or returned for local execution.
    void schedule_external( struct frame_struct * page );


    void get_ressources_rec( fat_pointer_p fat, Closure * c );

    // This function won't keep track of the task, will instead spawn local tasks to handle the ressources it needs (and thus hide latency)
    void prepare_ressources( struct frame_struct * page );


    // This yields to the inner scheduler.
    static void schedule_inner( struct frame_struct * page );


    // This method returns an array of stolen tasks.
    // buffer is an output buffer, supposed big enough.
    int steal_tasks( struct frame_struct ** buffer, int amount );

    int get_refund( int amount );

};


#endif // SCHEDULER_HPP
