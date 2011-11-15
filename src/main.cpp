#include "main.h"

#include <mapper.h>
#include <network.hpp>
#include <node.h>
#include <scheduler.hpp>
#include <frame.hpp>
#include <misc.h>
#include <owm_mem.hpp>
#include <invalidation.hpp>

#include <string.h>

int total_nodes;

int tstar_main_test1(int argc, char ** argv, struct frame_struct* first_task) {
       // Create a network interface, embedding parameters ( here MPI )
       NetworkInterface ni;
       ni.init(&argc, &argv);
       // Get the external infos.
       my_node = ni.node_num;
       total_nodes = ni.num_nodes;

       // Setup address space.
       mapper_initialize_address_space((void*)0x07000000, 0x4000, total_nodes);

       // Creates a scheduler :
       Scheduler s;

       if ( get_node_num() == 0 ) {
           char * s = (char*) owm_malloc(40);
           strcpy( s, "Toto");

           ni.dbg_send_ptr(1, s);
           DEBUG("Pointer sent, processing messages in loop.");

           ni.dbg_get_ptr(NULL);


       } else {
           char *s = NULL;
           DEBUG( "Getting pointer...");
           ni.dbg_get_ptr((void**)&s);
           DEBUG( "Got pointer from 0.");
           ni.send_data_req(0,s);
           DEBUG( "Data request message sent.");
           while ( memcmp( s, "Toto", 4)) {
               ni.process_messages();

           }
           // Signal termination.
           ni.dbg_send_ptr(0, NULL);
           printf( "Finished, success.\n");

       }

       ni.finalize();

       return 0;

}


int tstar_main_test2(int argc, char ** argv, struct frame_struct* first_task) {
       // Create a network interface, embedding parameters ( here MPI )
       NetworkInterface ni;
       ni.init(&argc, &argv);
       // Get the external infos.
       my_node = ni.node_num;
       total_nodes = ni.num_nodes;

       // Setup address space.
       mapper_initialize_address_space((void*)0x07000000, 0x4000, total_nodes);

       // Creates a scheduler :
       Scheduler s;

       if ( get_node_num() == 0 ) {
           char * s = (char*) owm_malloc(40);
           strcpy( s, "Toto");

           ni.dbg_send_ptr(1, s);
           DEBUG("Pointer sent, processing messages in loop.");

           ni.dbg_get_ptr(NULL);


       } else {
           char *s = NULL;
           DEBUG( "Getting pointer...");
           ni.dbg_get_ptr((void**)&s);
           DEBUG( "Got pointer from 0.");
           ni.send_data_req(0,s);
           DEBUG( "Data request message sent.");
           while ( memcmp( s, "Toto", 4)) {
               ni.process_messages();

           }

           bool finished = 0;
           bool *finishedp = &finished;

           Closure * c = new_Closure( 1,
           {    // Signal termination.
                ni.dbg_send_ptr(0, NULL);
                *finishedp = 1;
             });

           register_forinvalidateack(s, c);

           ask_or_do_invalidation_then(s,NULL);

           while ( PAGE_IS_VALID(s) ) {
               ni.process_messages();
           }


           while ( !finished ) {
               ni.process_messages();
           }
           printf( "Finished, success.\n");

       }

       ni.finalize();

       return 0;

}

int main( int argc, char ** argv ) {
#ifdef BUILD_TEST1
    tstar_main_test1(argc, argv, NULL);
#endif

#ifdef BUILD_TEST2
    tstar_main_test2(argc, argv, NULL );
#endif
}
