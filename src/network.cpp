

#include "mpi.h"

#include <misc.h>
#include <omp.h>
#include <frame.hpp>
#include <node.h>
#include <identifiers.h>
#include <invalidation.hpp>
#include <network.hpp>
#include <data_events.hpp>
#include <mapper.h>
#include <owm_mem.hpp>
#include <scheduler.hpp>


struct DataMessage {
  void * page;
  size_t size;
  char data[];
} __packed;

struct DataReqMessage{
  int orig;
  void * page;
};

struct RWrite {
  int orig;
  int serial;
  void * page;
  size_t offset;
  size_t size;
    char data[];
} __packed;

struct RWriteAck {
  void *page;
  int serial;
};

struct RWReq {
  int orig;
  void * page;
};

struct RespTransfer {
  void *page;
  long long shared_nodes_bitmap;
  DataMessage datamsg;
} __packed;

struct GoTransient {
  void * page;
};

struct AckInvalidate {
  void * page;
  serial_t serial;
};

struct DoInvalidate {
  void * page;
  serial_t serial;
};

struct AskInvalidate {
  node_id_t orig;
  void * page;
  serial_t serial;
};

struct TDec {
  int orig;
  int val;
  void * page;
};

struct TransPtr {
    void * ptr;
};

struct ExitMessage {
    int code;
};

struct StealMessage {
    int amount;
};

struct StolenMessage {
    int amount;
    struct frame_struct * stolen[];
}__packed;

struct FreeMessage {
    void * page;
};

void NetworkLowLevel::init( int* argc, char *** argv ) {
      MPI_Init(argc, argv );
      MPI_Comm_rank(MPI_COMM_WORLD, &node_num );
      MPI_Comm_size(MPI_COMM_WORLD, &num_nodes ); 
      set_node_num(node_num);
      set_num_nodes(num_nodes);
}

void NetworkLowLevel::finalize() {
    MPI_Finalize();

}

bool NetworkLowLevel::receive( MessageHdr &m ) const {
      MPI_Status s;
      int flag = 0;
      MPI_Iprobe( MPI_ANY_SOURCE,
                  MPI_ANY_TAG,
                  MPI_COMM_WORLD,
                  &flag, &s );
      if ( !flag ) {
         return false;
      }

      m.from = s.MPI_SOURCE;
      m.to   = get_node_num();
      m.type = (MessageTypes) s.MPI_TAG;
      // Get size :
      int tmp_count = 0;
      MPI_Get_count( &s, MPI_CHAR, &tmp_count );
      m.data_size = tmp_count;
      CFATAL(tmp_count<=0, "Negative or null message size");
      m.data = new char[m.data_size];
      //DEBUG( "RCV MessageHdr Data buffer at : %p", m.data);
      MPI_Recv( m.data, m.data_size,
                 MPI_CHAR, s.MPI_SOURCE,
                 s.MPI_TAG, MPI_COMM_WORLD, NULL );
      return true;
}




void * NetworkInterface::dbg_ptr_holder = NULL;
int NetworkInterface::dbg_ptr_signal = 0;
int NetworkInterface::stolen_message_onflight = 0;
NetworkInterface * NetworkInterface::global_network_interface = NULL;

NetworkInterface::NetworkInterface() : NetworkLowLevel() {
// This marco registers the handlers (following naming convention "onX")
// with the types enum (convention XType), into a table for multiplexing
// messages.
CFATAL( global_network_interface != NULL, "Double network interface ");
global_network_interface = this;
      bool type_handle_checker[LastMessageType];
      for ( int i = 0; i < LastMessageType; ++i ) {
          type_handle_checker[i] = 0;
      }
#define BIND_MT( id ) message_type_table[id##Type] = &on##id; type_handle_checker[id##Type] = 1
      BIND_MT( DataMessage );
      BIND_MT( DataReqMessage );
      BIND_MT( DoInvalidate );
      BIND_MT( AskInvalidate );
      BIND_MT( AckInvalidate );
      BIND_MT( TDec );
      BIND_MT( RWrite );
      BIND_MT( RWriteAck );
      BIND_MT( RWReq );
      BIND_MT( GoTransient );
      BIND_MT( RespTransfer );
      BIND_MT( ExitMessage );
      BIND_MT( StealMessage);
      BIND_MT( StolenMessage);
      BIND_MT( FreeMessage );
      /* For debugging purposes */
      BIND_MT( TransPtr );

#undef BIND_MT
      for ( int i = 0; i < LastMessageType; ++i ) {
          if (type_handle_checker[i] == 0)  {
              FATAL( "Undefined handler : %d, BIND_MT it !", i);
          }
      }
}

void NetworkInterface::process_messages() const {
      MessageHdr m;
      while ( true ) {
        if ( ! receive( m ) ) {
          return;
        }

        message_type_table[ m.type ] ( m );
        delete[] (char *)m.data; //Frees the message after processing.
      } 
} 



// Forwarding :

void NetworkInterface::forward( MessageHdr &m, node_id_t target ) {
      DEBUG( "NETWORK -- Forwarding message.");
      CFATAL( target == get_node_num(), "Forwarding loop.");

      m.to = target;
      m.from = get_node_num();
      send( m );
}

// Actual handlers
void NetworkInterface::onDataMessage( MessageHdr &m ) {
      // No transitive writers :

      DataMessage & drm = *(DataMessage*)m.data;

      struct owm_frame_layout *fheader = 
                  GET_FHEADER(drm.page);
      DEBUG( "NETWORK : Received DataMessage : page %p, size : %d", drm.page,(int)drm.size);
      CFATAL( PAGE_IS_RESP(drm.page), "Receiving and processing data message while resp...");
      CFATAL( PAGE_IS_VALID(drm.page), "Overwriting a valid copy on %p", drm.page);

      fheader->size = drm.size;


       // Finally copy :
       memcpy( drm.page, drm.data, drm.size );
       // Make it valid :
       VALIDATE( fheader );
       fheader->canari = CANARI;
       fheader->canari2 = CANARI;
       fheader->next_resp = m.from;

       // Signal data arrived :
       CFATAL ( !PAGE_IS_AVAILABLE(drm.page), "Inconsistent validation");
       signal_data_arrival( drm.page );
}

void NetworkInterface::onDataReqMessage( MessageHdr &m ) {
        DataReqMessage & drm = *(DataReqMessage *) m.data;


        owm_frame_layout *fheader = GET_FHEADER( drm.page );

        if ( mapper_who_owns(drm.page) == get_node_num() ) {
            // Owner must have correct canari values :
            CFATAL( fheader->canari != CANARI || fheader->canari2 != CANARI,
                    "Invalid canari values for page %p.",drm.page);

        }
        if ( !IS_RESP( fheader )) {
          DEBUG( "Fheader fof req message : size :%d, next_resp : %d", (int)fheader->size, fheader->next_resp);
          CFATAL(drm.orig == get_node_num(), "Forwarding loop.");
          forward( m, PAGE_GET_NEXT_RESP(drm.page) );
          return;
        }

        DEBUG( " NETWORK -- DataReqMessage Received : page %p from %d (sizeofpage=%d)", drm.page, drm.orig, (int)fheader->size );

        // Build an answer.
        DataMessage & resp = *((DataMessage*) new char[sizeof(DataMessage) +fheader->size] );
        resp.size = fheader->size;
        resp.page = drm.page;


        memcpy( resp.data, drm.page, fheader->size );

        // Send it :
        MessageHdr resphdr;
        resphdr.from = get_node_num();
        resphdr.to = drm.orig;
        resphdr.type = DataMessageType;
        resphdr.data = &resp;
        resphdr.data_size = sizeof(DataMessage) + fheader->size;


        send( resphdr );
        // TODO : replace with smart pointer...
        delete[] (char*)&resp;
        // Then add the recipient to the shared list of nodes :
        // see invalidation.cpp
        register_copy_distribution( drm.page, drm.orig );

}

void NetworkInterface::onRWrite( MessageHdr &m ) {
        // Check state or forward :
        RWrite & rwm = *(RWrite *) m.data;
        owm_frame_layout * fheader = GET_FHEADER( rwm.page );
        if ( ! IS_RESP( fheader ) ) {
          CFATAL(rwm.orig == get_node_num(), "Forwarding loop.");
          forward( m, PAGE_GET_NEXT_RESP(rwm.page) );
          return;
        }

        DEBUG( "NETWORK -- Received RWrite on (%p) : overwriting [%p .. %p] (size=%d)", rwm.page,
                                                                                        (char*)rwm.page+rwm.offset,
               (char *)rwm.page + rwm.offset+rwm.size, (int) rwm.size         );
        // Case of a resp node :
        // Integrate the write :
        CHECK_CANARIES(rwm.page);
        mapper_check_sanity();
        memcpy( (char*)rwm.page + rwm.offset, rwm.data, rwm.size );
        mapper_check_sanity();
        CHECK_CANARIES(rwm.page);

        // Then send ack :
        RWriteAck rwa;
        rwa.page = rwm.page;
        rwa.serial = rwm.serial;


        MessageHdr resphdr;
        resphdr.from = get_node_num();
        resphdr.to = rwm.orig;
        resphdr.type = RWriteAckType;
        resphdr.data = &rwa;
        resphdr.data_size = sizeof( RWriteAck );

        // Send the message.
        send( resphdr );

}

void NetworkInterface::onRWriteAck( MessageHdr &m ) {
        RWriteAck &rwa = *(RWriteAck*) m.data;

        DEBUG( "NETWORK -- Received RWriteAck for page : (%p), serial : %d", rwa.page, rwa.serial);
        // Not much else to do but to signal.

        // We additionaly note that the sender is the resp for this page.
        struct owm_frame_layout * fheader = GET_FHEADER(rwa.page);
        fheader->next_resp = m.from;

        signal_write_commited( rwa.serial, rwa.page );

}

void NetworkInterface::onRWReq( MessageHdr &m ) {
        // Check the state and forward if necessary :
        RWReq & rwrm = *(RWReq*) m.data;
        owm_frame_layout * fheader = GET_FHEADER( rwrm.page );

        // Check or forward.
        if ( ! IS_RESP( fheader ) ) {
          CFATAL(rwrm.orig == get_node_num(), "Forwarding loop.");
          forward( m, PAGE_GET_NEXT_RESP(rwrm.page) );
          return;
        }
        CHECK_CANARIES(rwrm.page);
        if ( get_node_num() == mapper_who_owns(rwrm.page) && fheader->freed) {
            FATAL( "Attempting to transfer resp of a freed zone.");
        }
        // If this page has no current writers :
        if ( HAS_ZERO_COUNT( fheader ) ) {
            DEBUG( "NETWORK -- RWReq received on %p, transferring.", rwrm.page);

            doRespTransfer(rwrm.page,rwrm.orig);
        } else {
            // TODO : Here we have one request to process before
            // others. If its not the case, it should be ok anyway
            // but would generate a lot of transfers.
            // It would be nice if task mappers respected FIFO order.

            DEBUG( "NETWORK -- RWReq received on %p, but busy.",rwrm.page);
            // We have to solve a RW race, we will
            // simply artificially delay the message.

            if ( fheader->reserved ) {
              // Answers go transitive : means to retry
              // later, and might mean actually transient


              Closure * transfer_message = new_Closure(1,
                RWReq new_req;
                new_req.page = rwrm.page;
                new_req.orig = rwrm.orig;

                MessageHdr resp;
                resp.from = get_node_num();
                resp.to = PAGE_GET_NEXT_RESP(rwrm.page);
                resp.type = RWReqType;
                resp.data_size = sizeof(RWReq);
                resp.data = &new_req;

                send( resp );

              );
              register_for_usecount_zero(rwrm.page, transfer_message);
              return;
          } else {
            // We will transfer responsibility ASAP.
            DEBUG( "Page %p reserved for %d", rwrm.page, rwrm.orig);

            Closure * do_transfer = new_Closure (1,
            doRespTransfer(rwrm.page, rwrm.orig););
            register_for_usecount_zero(rwrm.page,do_transfer);
            fheader->reserved = true;
            return;

          }
        }
}

void  NetworkInterface::doRespTransfer(PageType page,  node_id_t target ) {
        owm_frame_layout * fheader = GET_FHEADER(page);

        // Check status :
        CFATAL( !PAGE_IS_RESP(page), "Attempting to transfer RESP of non RESP !!! ");
        // Check for usecount :
        CFATAL( fheader->usecount != 0, "Transferring non zero usecount page.");
        CFATAL( fheader->size <= 0, "Transferring invalid sized zone");
        // Set state to valid.
        VALIDATE( fheader );

        CHECK_CANARIES(page);

        // Transfers the responsibility :
        fheader->next_resp = target;
        RespTransfer & resp_transfer = *( RespTransfer * ) new char[sizeof(RespTransfer)+fheader->size];
        resp_transfer.datamsg.size = fheader->size;
        resp_transfer.shared_nodes_bitmap = export_and_clear_shared_set(page);
        // Because we consider ourselves as valid, bitmap contains local node :
        resp_transfer.shared_nodes_bitmap &= 1<<get_node_num();
        resp_transfer.page = page;
        resp_transfer.datamsg.page = page;
        memcpy( resp_transfer.datamsg.data, fheader->data, fheader->size );


        MessageHdr resp_msg;
        resp_msg.from = get_node_num();
        resp_msg.to = target;
        resp_msg.type = RespTransferType;
        resp_msg.data = &resp_transfer;
        resp_msg.data_size = sizeof(RespTransfer) + fheader->size;


        send( resp_msg );    

        delete[] (char*)&resp_transfer;
}

void NetworkInterface::onRespTransfer( MessageHdr &m ) {

        RespTransfer &rt = *(RespTransfer*) m.data;
        owm_frame_layout *fheader = GET_FHEADER( rt.page );

        CFATAL ( IS_RESP( fheader ), "Already resp, an error occured" );
        CFATAL( IS_TRANSIENT( fheader ), "Transitive state not allowed to receive resp" );


        // Erases the data :
        memcpy( fheader->data, rt.datamsg.data, rt.datamsg.size );

        // Set resp :
        RESPONSIBILIZE( fheader );
        SET_CANARIES(fheader);
        shared_set_load_bit_map(rt.page,rt.shared_nodes_bitmap);
        // Usecount is initialized to zero, it will grow with :
        //  - Immediate increases at "acquire" phases
        //  - Closure-triggered increases when write accessed is signaled.
        fheader->usecount = 0;
        fheader->reserved = false;
        fheader->size = rt.datamsg.size;

        // Signal read and write data arrival :
        signal_data_arrival( rt.datamsg.page);
        signal_write_arrival( rt.datamsg.page );

}

void NetworkInterface::onGoTransient( MessageHdr &m ) {
        FATAL( "Not yet supported" );
}

void NetworkInterface::onAskInvalidate( MessageHdr &m ){
        AskInvalidate &ai = *(AskInvalidate*) m.data;
        owm_frame_layout * fheader = GET_FHEADER( ai.page );
        DEBUG( "Receiving AskInvalidate(on %p), orig=%d ", ai.page, ai.orig );
        if ( !IS_RESP( fheader )  ) {
          CFATAL(ai.orig == get_node_num(), "Forwarding loop.");
          forward( m, PAGE_GET_NEXT_RESP(ai.page) );
          return;
        } 
        CHECK_CANARIES(ai.page);

        if ( get_node_num() == mapper_who_owns(ai.page) && GET_FHEADER(ai.page)->freed ) {
            FATAL( "Asking invalidation of freed page (passed canari test).");
        }
        planify_invalidation( ai.page, ai.serial, ai.orig );

}

void NetworkInterface::onDoInvalidate( MessageHdr &m ) {
        DoInvalidate &di = *(DoInvalidate*) m.data;
        owm_frame_layout * fheader = GET_FHEADER( di.page );

        CHECK_CANARIES(di.page);

        DEBUG( "Received DoInvalidate(%p)", di.page);
        CFATAL( IS_RESP( fheader), "Cannot invalidate RESP" );
        CFATAL( IS_INVALID(fheader), "Cannot invalidate Invalid : double invalidation.");
        INVALIDATE( fheader );


        // Respond do invalidate.
        AckInvalidate ackinvalidate;
        ackinvalidate.page = di.page;
        ackinvalidate.serial = di.serial;

        MessageHdr resp;
        resp.from = get_node_num();
        resp.to = m.from;
        resp.type = AckInvalidateType;
        resp.data_size = sizeof( AckInvalidate);
        resp.data = & ackinvalidate;

        send( resp );

}

void NetworkInterface::onAckInvalidate( MessageHdr &m ) {
        AckInvalidate &acki = *(AckInvalidate*) m.data;
        owm_frame_layout * fheader = GET_FHEADER( acki.page );
        DEBUG( "Receiving AckInvalidate(%p)", acki.page);
        if ( !IS_RESP( fheader ) ) {
          // Then resp is the sender :
          //CFATAL( ! fheader->initialized, "Unitinialized data shouldn't receive InvalidateAck");
          // This line forbids freeing before invalidation : it would corrupt memory on an
          // owner who would have transferred his rights.
          fheader->next_resp = m.from;
        }

        signal_invalidation_ack(  acki.page, acki.serial );
}


void NetworkInterface::onTDec( MessageHdr &m ) {
        TDec & tdec = *(TDec *) m.data;

        owm_frame_layout * fheader = GET_FHEADER( tdec.page );

        if ( ! IS_RESP( fheader ) ) {
            forward( m, PAGE_GET_NEXT_RESP(tdec.page) );
          return;
        }

        // Do Tdec :
        struct frame_struct * fp =
                (struct frame_struct*) fheader->data;

        int sfres =  __sync_sub_and_fetch( &fp->sc, 1 );
        DEBUG( "TDec comes to : %d", sfres);
        if ( sfres == 0 ) {
            Scheduler::global_scheduler->schedule_global(fp);
        } else if ( sfres < 0 ) {
            FATAL( "Negative TDec.");
        }

}

void NetworkInterface::onStealMessage( MessageHdr &m ) {
    StealMessage &sm = *(StealMessage *) m.data;
    DEBUG( "Networkinterface : Steal Message received(amount=%d), responding...", sm.amount);
    int amount = sm.amount;

    frame_struct * buffer [ amount ];
    amount = Scheduler::global_scheduler->steal_tasks(buffer, amount );

    send_stolen_message( m.from, amount, buffer );

}

// This handler processes the "Stolen" Messages, that is,
// the messages containing pointers to the tasks that were stolen
// and are transfered locally.
void NetworkInterface::onStolenMessage( MessageHdr &m ) {
    StolenMessage &sm = *(StolenMessage*) m.data;
    DEBUG( "Networkinterface : Stolen Message received.");

    int amount = sm.amount;
    for ( int i = 0; i < amount; ++ i ) {
        DEBUG( "Importing Task.");
        Scheduler::global_scheduler->schedule_global(sm.stolen[i]);
    }
    stolen_message_onflight = 0;
}

void NetworkInterface::onExitMessage( MessageHdr &m ) {
    DEBUG( "Exit Received");
    int code = ((ExitMessage*)m.data)->code;
    MPI_Finalize();
    exit(code);
}

void NetworkInterface::onFreeMessage(MessageHdr &m) {


    FreeMessage &fm = *(FreeMessage*)m.data;
    if ( mapper_who_owns(fm.page) != get_node_num()) {
        FATAL( "Free message not addressed to owner...");
    }

    DEBUG( "FreeMessage Received for %p : freeing locally.", fm.page);

    owm_free_local(fm.page);


}


/*------------ Functions for sending messages---------------------- */
void NetworkInterface::send_invalidate_ack( node_id_t target, PageType page, serial_t serial ) {
        if ( target == get_node_num() ) {
          // Just trigger the invalidation as done locally.
          FATAL("Sending a message locally : this must be a bug");
          return;
        }


        //  Or actually send the message.
        AckInvalidate ai;
        ai.page = page;
        ai.serial = serial;

        MessageHdr m;
        m.from = get_node_num();
        m.to = target;
        m.type = AckInvalidateType;
        m.data_size = sizeof(AckInvalidate);
        m.data = &ai;

        send( m );

} 


void NetworkInterface::send_do_invalidate( node_id_t target,  PageType page, serial_t serial ) {
        DoInvalidate di;
        di.page = page;
        di.serial = serial;

        MessageHdr m;
        m.from = get_node_num();
        m.to = target;
        m.type = DoInvalidateType;
        m.data_size = sizeof(DoInvalidate);
        m.data = &di;

        send( m );
        
}

void NetworkInterface::send_ask_invalidate( node_id_t target, PageType page, serial_t serial ) {
        CFATAL( target==get_node_num(), "Attempting to invalidate locally by sending a message." );
        AskInvalidate ai;
        ai.page = page;
        ai.orig = get_node_num();
        ai.serial = serial;
        
        MessageHdr m;
        m.from = get_node_num();
        m.to = target;
        m.type = AskInvalidateType;
        m.data_size = sizeof( AskInvalidate );
        m.data = &ai;

        DEBUG( "Sending AckInvalidate (%p), orig=%d", ai.page, ai.orig );
        send( m );
}

void NetworkInterface::send_data_req( node_id_t target, void * page ) {
        DataReqMessage drm;
        drm.page = page;
        drm.orig = get_node_num();

        MessageHdr m;
        m.from = get_node_num();
        m.to = target;
        m.type = DataReqMessageType;
        m.data_size = sizeof( DataReqMessage );
        m.data = &drm;

        send( m );
}

void NetworkInterface::send_resp_req( node_id_t target, void * page ) {
        RWReq rw;
        rw.orig = get_node_num();
        rw.page = page;

        MessageHdr m;
        m.from = get_node_num();
        m.to = target;
        m.type = RWReqType;
        m.data_size = sizeof( RWReq );
        m.data = &rw;

        send( m );
}

void NetworkInterface::send_tdec( node_id_t target, void * page, int val ) {
        TDec tdec;
        tdec.orig = get_node_num();
        tdec.val = val;
        tdec.page = page;

        MessageHdr m;
        m.from = get_node_num();
        m.to = target;
        m.type = TDecType;
        m.data_size = sizeof( TDec);
        m.data = &tdec;

        send( m );

}

void NetworkInterface::send_rwrite( node_id_t target,
                                    serial_t serial,
                                    void * page,
                                    size_t offset,
                                    void * buffer,
                                    size_t len) {
    RWrite &rw = *(RWrite*)new char[sizeof(RWrite)+ len];
    memcpy( rw.data, (char*)buffer, len );
    rw.offset = offset;
    rw.orig = get_node_num();
    rw.serial = serial;
    rw.page = page;
    rw.size = len;

    MessageHdr m;
    m.from = get_node_num();
    m.to = target;
    m.data = &rw;
    m.data_size = sizeof(RWrite) + len;
    m.type = RWriteType;

    send(m);
    delete[] (char*)&rw;
}

void NetworkInterface::bcast_exit( int code ) {
    for ( int i = 0; i < get_num_nodes(); ++i ) {
        ExitMessage em;
        em.code = code;
        MessageHdr m;
        m.data = &em;
        m.data_size = sizeof(ExitMessage);
        m.to = i;
        m.from = get_node_num();
        m.type = ExitMessageType;

        send(m);
    }

}

void NetworkInterface::onTransPtr(MessageHdr &msg) {
    dbg_ptr_holder = ((TransPtr*)msg.data)->ptr;
    dbg_ptr_signal = 1;
}

void NetworkInterface::dbg_get_ptr( void ** ptrp ) {
    while ( !dbg_ptr_signal ) {
        process_messages();
    }
    dbg_ptr_signal = 0;
    if ( ptrp != NULL ) *ptrp = dbg_ptr_holder;

}

void NetworkInterface::dbg_send_ptr( node_id_t target, void * ptr ) {
    TransPtr tp;
    tp.ptr = ptr;
    MessageHdr msg;
    msg.data = &tp;
    msg.from = get_node_num();
    msg.to = target;
    msg.type = TransPtrType;
    msg.data_size = sizeof( TransPtr);

    send( msg );
}
void NetworkInterface::send_steal_message(node_id_t target, int amount ) {
    CFATAL( target < 0 || target >= get_num_nodes(), "Invalid target : %d", target);
    CFATAL( amount <= 0 || amount > 1000, "Invalid amount of tasks to steal (%d) ", amount);
    CFATAL( target == get_node_num(), "Cannot steal self : absurd, check code.");

    DEBUG( "NetworkInterface : Sending Steal message for %d : %d tasks wanted. ", target, amount );
    StealMessage s;
    s.amount = amount;

    MessageHdr m;
    m.data = &s;
    m.data_size = sizeof( StealMessage );

    m.to = target;
    m.from = get_node_num();
    m.type = StealMessageType;

    stolen_message_onflight=1;
    send( m );
    DEBUG( "Steal message sent.");

}

void NetworkInterface::send_stolen_message( int target ,  int amount, struct frame_struct ** buffer ) {
    CFATAL( target < 0 || target >= get_num_nodes(), "Invalid target : %d", target);
    CFATAL( target == get_node_num(), "Cannot steal self : absurd, check code.");
    DEBUG( "NetworkInterface : Sending Stolen message for %d : %d tasks packed. ", target, amount );

    // Allocate a message :
    StolenMessage * sm = (StolenMessage*) new char[sizeof(StolenMessage) + amount * sizeof(frame_struct*)];
    sm->amount = amount;
    memcpy( sm->stolen, buffer, amount * sizeof(frame_struct*));

    MessageHdr m;
    m.from = get_node_num();
    m.to = target;
    m.data_size = sizeof(StolenMessage) + amount * sizeof(frame_struct *);
    m.data = sm;
    m.type = StolenMessageType;

    send( m );
    delete[] (char*)sm;
}

void NetworkInterface::send_free_message(node_id_t target, PageType page) {
    FreeMessage fm;
    fm.page = page;

    MessageHdr m;
    m.to = target;
    m.from = get_node_num();
    m.type = FreeMessageType;
    m.data = &fm;
    m.data_size = sizeof( FreeMessage);

    send(m);
}

// Wait for reply.
void NetworkInterface::wait_for_stolen_task() {
    // This function is used when it is needed to busy wait for results from a steal.
    // Due to its coslty nature, it should be the last line of defence and only
    // should be activated when the whole node has no work to work on.

    // Besides, it will be done in a delegator, and as a lengthy function it will
    // Deny access to the delegator as long as it is waiting there.
    // The node itself will still be responsive and process messages as usual.

    while (stolen_message_onflight) {
        // Make sure we are not spamming the delegator : ( TODO : do a sync_delegate... )
        bool waiter = false;
        bool *wp = &waiter;
        DELEGATE( Delegator::default_delegator, process_messages(); *wp=true ;);
        while ( !waiter );
    }

}



