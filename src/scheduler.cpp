



/* This small scheduler class actually keeps track of the tasks when it is needed only */




class Scheduler {
  list<struct frame_struct *> external_tasks;
  int global_local_threshold  = 0;
  int task_count;


public :

  Scheduler() : global_local_threshold(get_num_threads() * 5 ), task_count(0) {
	
  }

  // This is the entry point, should be attained when SC reaches 0.
  void schedule_global( struct frame_struct * page )  {
	if ( task_count > global_local_threshold ) {
	  schedule_external( page ); 
	} else {
	  prepare_ressources( page );
	}
  }

  // This function adds the task to an external queue.
  // This queue can be eventually published or returned for local execution.
  void schedule_external( struct frame_struct * page ) { 
	external_tasks.push_front( page );
  }

  // This function won't keep track of the task, will instead spawn local tasks to handle the ressources it needs (and thus hide latency)
  void prepare_ressources( struct frame_struct * page ) {
    // As a first step we need to analyze how this task accesses other pages.
    // Then we need to plan recursively the ressource acquisition.
    // Finallly, the total number of sub-tasks we created is used as trigger level for the next step.

	int work_count = 0;
	if ( !PAGE_IS_AVAILABLE( page ) ) {
		// Then schedule itself for when its ready :p
		auto todo = new_Closure( 1,
		  { prepare_ressources( page ); } );

		int serial = request_page (page);
		register_for_data_arrival( page, todo );

		return;
	}

	for ( int i = 0; i < page->static_data->nfields; ++ i ) {
		if ( page->static_data.fields[i] == ACQUIRE_SIMPLE_R
			 || page->static_data.fields[i] == ACQUIRE_SIMPLE_RW
			 ||  page->static_data.fields[i] == ACQUIRE_COMPLEX ) {
		  work_count += 1;
		}

		// Will be mapped once more for writing :
		if (  page->static_data.fields[i] == ACQUIRE_SIMPLE_RW ) {
		  work_count += 1;
		}
	}


	if ( work count == 0 ) {
		schedule_inner( page );
		return; 
	} 

	auto gotoinnersched  = new_ClosureLocalTask( work_count, 
		  {
			schedule_inner( page );
		  } );


	for ( int i = 0; i < page->static_data->nfields; ++ i ) {
		if ( page->static_data.fields[i] == ACQUIRE_SIMPLE_R ) {
		  register_for_data_arrival( page->fields[i], gotoinnersched );
		}

		if (  page->static_data.fields[i] == ACQUIRE_SIMPLE_RW ) {
		  register_for_data_arrival( page->fields[i], gotoinnersched);
		  register_for_write_arrival( page->fields[i], gotoinnersched );
		}

		if (   page->static_data.fields[i] == ACQUIRE_COMPLEX ) {
		  prepare_data_rec( page->fields[i], gotoinnersched );
		}
	}
  
  }


  // This yields to the inner scheduler.
  static void schedule_inner( struct frame_struct * page ) {
	#pragma omp task	
	executor( page );
  }


  // This method returns an array of stolen tasks.
  // buffer is an output buffer, supposed big enough.
  int steal_tasks( struct frame_struct * buffer, int amount ) {
	delegate_only();

	for ( int i = 0; i < amount; ++ i ) {
	  if ( external_tasks.empty() ) {
		// Only i tasks were stolen.
		return i;
	  }
	  buffer[i] = external_tasks.back();
	  external_tasks.pop_back();
	}

	// `Amount` tasks were stolen
	return amount;
  }

  int get_refund( int amount ) {
	delegate_only();

	for ( int i = 0; i < amount; ++ i ) {
	  if ( external_tasks.empty() ) {
		return i;
	  }

	  // This will eventually get the tasks to execute.
	  prepare_ressources( external_tasks.front() ) ;
	  external_tasks.pop_front();
	}

	return amount;
  }





  
}

struct TDec {
	 struct frame_struct * target;
	 void * related;
};

struct DWrite {
	void * object;
	void * frame;
	void * pos;
	size_t len;
};

class ExecutionUnit {


  struct frame_struct * current_cfp;
  // Ressource -> writes.
  map<PtrType, DWrite> writes_by_ressource;
  map<PtrType, TDec> tdecs_by_ressource;  


  void executor( struct frame_struct * page ) {
	current_cfp = page;
	if ( ! PAGE_VALID( page ) ) {
	   FATAL( "Execution of invalid page" );
	}

	before_code();
	page->static_data->code();
	after_code();
	

  }

  void tdec( struct frame_struct * page, void * ref ) {
	struct TDec mytdec = {page,ref};
	tdecs_list.push_front(mytdec);
  }


  void register_write( void * object, void * frame, void * pos, size_t len ) {
	

  }

  static void process_commits() {
	// For each registered ressource 
	for_each( ressource_desc d, current_cfp ) {
	  // Check type :
	  // If fat pointer, process : 
	  //    * Writes per frame, accessed in map by object.
	  //    * Allocate a closure that triggers invalidation and is registered in rwrite ack map.
	  //    * Allocation of a buffer for holding relevant tdecs.
	  //    * After recursive invalidation of complex object, tdecs are executed and the array freed.

	  // If normal frame :
	  //   * process writes per frame, accessed in map per frame.
	  //   * Allocate a closure that triggers invalidation and is registered in rwrite ack map.
	  //   * Allocation of a buffer for all the tdecs to do.
	  //   * after simple invalidation, do the tdecs and free the buffer.
	}
	
  }


  static void before_code() {

  }  

  static void after_code() {

  }

}
