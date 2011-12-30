
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


typedef THANDLE( _input(),
                 _output(vdef(FibOut), int)) fibentry_task;

typedef THANDLE( _input( vdef(A), int,
                         vdef(B),int ),
                 _output( vdef(AddOut), int)
                 ) adder_task;
typedef THANDLE( _input( vdef(ToPrint),int),
                 _output() ) printer_task;

// Takes care of creating initial value and binding to print.
typedef THANDLE( _input(), _output()) fibclient_task;

adder_task * adder(fibentry_task *a, fibentry_task *b) {
    adder_task * addert = TASK(3, adder_task,
                _code(
                    DEBUG( "In Adder(%d,%d) -> %d", get_arg(A,int), get_arg(B,int),
                           get_arg(A,int)+get_arg(B,int));
                    provide( AddOut, int, get_arg(A,int) + get_arg(B,int));
                    )
                );
    bind(a, FibOut, addert, A, int);
    bind(b, FibOut, addert, B, int);

    tstar_tdec(addert, NULL);

    return addert;

}

fibentry_task * fibentry( int n ) {
    fibentry_task * fet =  TASK(1, fibentry_task,
                _code(
                    CFATAL( n < 0, "Invalid use of fibentry, n >= 0 required (%d found )", n);
                    DEBUG( "FIBENTRY %d", n);
                    if ( n == 0 || n == 1 ) {
                        provide( FibOut, int, n);
                        return;
                    }

                    // or spawn fib1 and fib2
                    adder_task * ad = adder(fibentry(n-1),fibentry(n-2));

                    bind_outout( ad, AddOut, FibOut);


                    ) );
        tstar_tdec(fet, NULL);
        return fet;
}

printer_task * printer() {
    return TASK(1, printer_task,
                _code (
                    fprintf( stderr, "Result : %d\n", get_arg(ToPrint,int));

                    int nid = get_node_num();

                    NetworkInterface::bcast_exit(0);

                    )
            );
}


fibclient_task * fibclient(int n) {
    return TASK( 1, fibclient_task,
                 _code(
                     DEBUG( "In FibClient (%d)", n);
                      fibentry_task * fib0 = fibentry(n);
                      printer_task * p0 = printer();

                      bind(fib0, FibOut, p0, ToPrint, int );
#if DEBUG_ADDITIONAL_CODE
                      mapper_valid_address_check(fib0->get_framep(vname(FibOut)) );
#endif
                     )
                 );

}


int arg = 11;
struct frame_struct * factory0() {
    return (frame_struct*)fibclient(arg);
}

int tstar_main_test6(int argc, char ** argv, struct frame_struct* first_task) {

    tstar_setup(argc, argv);
    if ( get_node_num() == 0) {
        int fibval = atoi( argv[argc-1]);
        fibval = fibval==0?10:fibval;
        arg = fibval;


        tstar_main(factory0);
    } else {
        tstar_main(NULL);
    }




    DEBUG( "Shouldn't display...");
    return 0;
}


int main( int argc, char ** argv ) {
           tstar_main_test6(argc, argv, NULL );

}

