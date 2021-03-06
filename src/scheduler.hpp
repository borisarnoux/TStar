#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP


#include <omp.h>
#include <list>
#include <map>
#include <scheduler.hpp>
#include <identifiers.h>
#include <data_events.hpp>
#include <invalidation.hpp>
#include <delegator.hpp>
#include <network.hpp>
#include <taskstealing.h>
#include <memory>
#include <globalcounter.h>
/* This  scheduler class  keeps track of the tasks when it is needed */

#define GLOBAL_LOCAL_THRESHOLD 20

//struct TDec {
//    struct frame_struct * page;
//    void * ref;
//};

struct DWrite {
    void * obj;
    void * frame;
    size_t offset;
    size_t len;
    char * buffer;
    DWrite(void * _o, void * _f, size_t _offset, void * _buf, size_t _l) :
        obj(_o),frame(_f),offset(_offset),len(_l),buffer(new char[_l]) {
        CFATAL( _l <= 0, "Invalid write buffer size." );
        memcpy( buffer, _buf, _l);
    }

    ~DWrite() {
        delete[] buffer;
    }

};





typedef std::multimap<PageType, DWrite*> DWriteMap;
typedef std::multimap<PageType, PageType> TDecMap;
typedef std::list<struct frame_struct*> CreatedFramesList;

class ExecutionUnit {
private:

    // Ressource -> writes.
    DWriteMap writes_by_ressource;
    TDecMap tdecs_by_ressource;
    CreatedFramesList created_frames_list;
    NetworkInterface &ni;


public:

    ExecutionUnit(NetworkInterface &_ni) : ni(_ni) {

    }

    struct frame_struct * current_cfp;

    static __thread ExecutionUnit *local_execution_unit;
    void executor( struct frame_struct * page );
    static void executor_static( struct frame_struct * page );
    void tdec( struct frame_struct * page, void * ref );
    void register_write( void * object, void * frame, size_t offset, void * buffer, size_t len );
    void process_commits();
    bool check_ressources();
    void before_code();
    void after_code();
};


class Scheduler {
    friend class ExecutionUnit;
    TaskScheduler ts;

    int global_local_threshold;
    NetworkInterface &ni;
    std::list<struct frame_struct *> external_tasks;

    static const int lower_bound_for_work = 10;

    GlobalCounter<get_thread_num> prep_task_count;
    GlobalCounter<get_thread_num> omp_task_count;

public :
    static int busy_processing_messages;
    static int processing_messages_lock;


    static Scheduler * global_scheduler;

    Scheduler(NetworkInterface &_ni) :
        ts(get_num_threads(), ExecutionUnit::executor_static),
        global_local_threshold(GLOBAL_LOCAL_THRESHOLD),
        ni(_ni),
        prep_task_count(get_num_threads()),
        omp_task_count(get_num_threads())
    {
        CFATAL( global_scheduler != NULL, "Cannot run two schedulers.");
        global_scheduler = this;
    }



    // This is the entry point, should be attained when SC reaches 0.
    void schedule_global( struct frame_struct * page );

    // This function adds the task to an external queue.
    // This queue can be eventually published or returned for local execution.
    void schedule_external( struct frame_struct * page );

    // This function won't keep track of the task, will instead spawn local tasks to handle the ressources it needs (and thus hide latency)
    void prepare_ressources( struct frame_struct * page );


    // This yields to the inner scheduler.
    void schedule_inner( struct frame_struct * page );
    void steal_and_process(int amount);

    // This method returns an array of stolen tasks.
    // buffer is an output buffer, supposed big enough.
    int steal_tasks( struct frame_struct ** buffer, int amount );

    int get_refund( int amount );
    // Start with a factory to build the first task.
    void start ( frame_struct*(*tgen)());

};


#endif // SCHEDULER_HPP
