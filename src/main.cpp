#include "main.h"

#include <mapper.h>
#include <network.hpp>
#include <node.h>
#include <scheduler.h>
#include <frame.hpp>
#include <misc.h>

int total_nodes;

int tstar_main(int argc, char ** argv, struct frame_struct* first_task) {
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

#pragma omp parallel single
{
           DEBUG( "Parallel single region.");


}
       ni.finalize();

       return 0;

}
