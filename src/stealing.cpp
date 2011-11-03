



/* This small scheduler class actually keeps track of the tasks when it is needed only */


class OuterScheduler {

  // This is the entry point, should be attained when SC reaches 0.
  void schedule_global( struct frame_struct * page )  {

  }

  // This function adds the task to an external queue.
  // This queue can be eventually published or returned for local execution.
  void schedule_external( struct frame_struct * page ) { 

  }

  // This function won't keep track of the task, will instead spawn local tasks to handle the ressources it needs (and thus hide latency)
  void prepare_ressources( struct frame_struct * page ) {
    // As a first step we need to analyze how page accesses other page.
    // Then we need to plan recursively the ressource acquisition.
    // Finallly, the total number of sub-tasks we created is used as trigger level for the next step.
  }


  // This yields to the inner scheduler.
  void schedule_inner( struct frame_struct * page ) {
    
  }


  // This method returns an array of stolen tasks.
  // buffer is an output buffer, supposed big enough.
  void steal_tasks( struct frame_struct * buffer, int amount ) {

  }

}
