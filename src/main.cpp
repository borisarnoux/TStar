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


#include <typetuple.hpp>

#ifdef __x86_64
#define ZONE_START 0x70ef55b44000
#else
#define ZONE_START 0x70000000
#endif

int total_nodes = 0;
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
       Scheduler s(ni);

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
       mapper_initialize_address_space((void*)ZONE_START, 0x4000, total_nodes);

       // Creates a scheduler :
       Scheduler s(ni);

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
               // Signal termination.
                ni.dbg_send_ptr(0, NULL);
                printf( "Reacting to invalidateAck");
                *finishedp = 1;
             );

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

int tstar_main_test3(int argc, char ** argv, struct frame_struct* first_task) {
       // Create a network interface, embedding parameters ( here MPI )
       NetworkInterface ni;
       ni.init(&argc, &argv);
       // Get the external infos.
       my_node = ni.node_num;
       total_nodes = ni.num_nodes;

       // Setup address space.
       mapper_initialize_address_space((void*)ZONE_START, 0x4000, total_nodes);

       // Creates a scheduler :
       Scheduler s(ni);

       if ( get_node_num() == 0 ) {
           char * s = (char*) owm_malloc(40);
           strcpy( s, "Toto");

           CHECK_CANARIES(s);
           owm_frame_layout * fheader = GET_FHEADER(s);
           // Prepares for resp transfer.
           fheader->usecount = 0;

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
               // Signal termination.
                printf( "Reacting to invalidateAck");
                *finishedp = 1;
             );

           register_forinvalidateack(s, c);

           ask_or_do_invalidation_then(s,NULL);

           while ( PAGE_IS_VALID(s) ) {
               ni.process_messages();
           }


           while ( !finished ) {
               ni.process_messages();
           }

           bool is_resp = false;
           bool *is_resp_p = &is_resp;
           Closure * become_resp = new_Closure(1,
                  *is_resp_p = true; );

           request_page_resp(s);
           register_for_write_arrival(s,become_resp);
           while (!is_resp) {
               ni.process_messages();
           }
           ni.dbg_send_ptr(0, NULL);


           printf( "Finished, success.\n");

       }

       ni.finalize();

       return 0;

}

int tstar_main_test4(int argc, char ** argv, struct frame_struct* first_task) {
    // Create a network interface, embedding parameters ( here MPI )
    NetworkInterface ni;
    ni.init(&argc, &argv);


    // Setup address space.
    mapper_initialize_address_space((void*)ZONE_START, 0x4000, get_num_nodes());

    // Creates a scheduler :
    Scheduler s(ni);

    if ( get_node_num() == 0 ) {
        DEBUG("Starting (%d)/%d",get_node_num(),get_num_nodes());
        char * s1 = (char*) owm_malloc(40);
        char * s2 = (char*) owm_malloc(40);

        owm_frame_layout * fheader1 = GET_FHEADER(s1);
        owm_frame_layout * fheader2 = GET_FHEADER(s2);

        fheader1->usecount = 0;
        fheader2->usecount = 0;

        strcpy( s1, "Toto");
        strcpy( s2, "Tata");

        fat_pointer_p fp = create_fatp(2);
        DEBUG("FP1 : %p", fp);
        fat_pointer_p fp2 = create_fatp(2);
        DEBUG("FP2 : %p",fp2);
        append_page_to_fatp(s1, R_FRAME_TYPE, fp);
        append_page_to_fatp(fp2, FATP_TYPE, fp);
        append_page_to_fatp(s2, RW_FRAME_TYPE, fp2);

        notify_fatp_transfer(fp);
        //release_rec( fp );

        for (int i = 1; i < get_num_nodes(); ++i) {
            DEBUG("Sending pointer to %d", i);
            ni.dbg_send_ptr(i, fp);
        }


        for (int i = 1; i < get_num_nodes(); ++i) {
            DEBUG( "Finished : %d/%d", i+1, get_num_nodes());
            ni.dbg_get_ptr(NULL);
        }
        for (int i = 1; i < get_num_nodes(); ++i) {
            ni.dbg_send_ptr(i, NULL);
        }

    } else {
        DEBUG("Starting (%d)/%d",get_node_num(), get_num_nodes());
        Delegator d;
        fat_pointer_buffer* s;
        // Get the pointer
        ni.dbg_get_ptr((void **)&s);
        DEBUG( "Got pointer %p", s);

        bool done = false;
        bool * donep = &done;
        Closure * c = new_Closure( 1, *donep=true; );
        DELEGATE( d, DEBUG( "Delegated acquire rec:");acquire_rec(s, c); );
        while ( !done ) {
            DELEGATE(d,ni.process_messages(););

        }


        if (get_node_num()==4) {
            done = false;
            c = new_Closure( 1, *donep=true; );
            ask_or_do_invalidation_rec_then(s, c);
            while ( !done ) {
                DELEGATE(d,ni.process_messages(););
            }
        }


        release_rec(s);
        ni.dbg_send_ptr(0,NULL);
        // For terminating while handling messages.
        ni.dbg_get_ptr(NULL);
     }

     ni.finalize();

     return 0;

}



typedef THANDLE( _input( vdef(X), int, vdef(Buf),struct wo_frame * ), _output(vdef(BufO), struct ro_frame *) ) whello_task;
typedef THANDLE( _input( vdef(X), int, vdef(Buf),struct ro_frame * ), _output() ) rhello_task;

int tstar_main_test5(int argc, char ** argv, struct frame_struct* first_task) {


    // Create a network interface, embedding parameters ( here MPI )
    NetworkInterface ni;
    ni.init(&argc, &argv);


    // Setup address space.
    mapper_initialize_address_space((void*)ZONE_START, 0x4000, get_num_nodes());

    // Creates a scheduler :
    Scheduler *s = new Scheduler(ni);
    Delegator d;



    // Creates a memory zone :
    if ( get_node_num() == 0 ) {
        struct wo_frame * owm_pointer = (wo_frame*) owm_malloc(40);



          whello_task * htp = TASK( 1, whello_task,
                             _code (
                                       sprintf( (char*)get_arg(Buf,wo_frame*), "Hello %d!!", get_arg(X,int) );
                                       provide(BufO, ro_frame*, (ro_frame*)get_arg( Buf,wo_frame*));
                                 )
                             );
          rhello_task * rht = TASK( 1, rhello_task,
                                    _code(
                                        printf( "%s\n", (char*)get_arg(Buf,ro_frame *));
                                        ));

          handle_get_arg<wo_frame*>( htp, vname(Buf)) = owm_pointer;
          bind(htp, BufO, rht, Buf, ro_frame *);

#pragma omp parallel
{
            s->tls_init();
#pragma omp single
            {
            DELEGATE(d, s->schedule_global( (struct frame_struct *)htp);
                        s->steal_and_process();
                );
            }
}
    } else {
#pragma omp parallel
{
         s->tls_init();
 #pragma omp single
{
        DELEGATE( d, s->steal_and_process(); );
}
}
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

#ifdef BUILD_TEST3
           tstar_main_test3(argc, argv, NULL );
#endif
#ifdef BUILD_TEST4
           tstar_main_test4(argc, argv, NULL );
#endif

#ifdef BUILD_TEST5
           tstar_main_test5(argc, argv, NULL );
#endif
       }
