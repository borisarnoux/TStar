

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

struct DataMessage {
  void * page;
  size_t size;
  char data[];
};

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
};

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
};

struct GoTransient {
  void * page;
};

struct AckInvalidate {
  void * page;
};

struct DoInvalidate {
  void * page;
};

struct AskInvalidate {
  node_id_t orig;
  void * page;
};

struct TDec {
  int orig;
  int val;
  void * page;
};

struct TransPtr {
    void * ptr;
};



void NetworkLowLevel::init( int* argc, char *** argv ) {
      MPI_Init(argc, argv );
      MPI_Comm_rank(MPI_COMM_WORLD, &node_num );
      MPI_Comm_size(MPI_COMM_WORLD, &num_nodes ); 
}

void NetworkLowLevel::finalize() {
    MPI_Finalize();

}

bool NetworkLowLevel::receive( MessageHdr &m ) {
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




void * NetworkInterface::dbg_ptr_holder;
int NetworkInterface::dbg_ptr_signal;

NetworkInterface::NetworkInterface() : NetworkLowLevel() {
// This marco registers the handlers (following naming convention "onX")
// with the types enum (convention XType), into a table for multiplexing
// messages.
#define BIND_MT( id ) message_type_table[id##Type] = &on##id
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
      /* For debugging purposes */
      BIND_MT( TransPtr );

#undef BIND_MT
}

void NetworkInterface::process_messages() {
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
    DEBUG( "Forwarding message");
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
      DEBUG( "Received data message : %p, size : %d", drm.page,(int)drm.size);

        fheader->size = drm.size;

        // Error if not invalid 
        if ( ! IS_INVALID( fheader ) ) {
          FATAL( "Violated hypothesis : no transient writers receive data" );
        } 

        // Finally copy :
        memcpy( drm.page, drm.data, drm.size );
        // Make it valid :
        VALIDATE( fheader );
        DEBUG( "Data copied");
        // Signal data arrived :
        signal_data_arrival( drm.page );
        DEBUG( "Data arrived and signaled : %p, size %d", drm.page,(int)drm.size);
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
          forward( m, fheader->next_resp );
          return;
        }

        DEBUG( "DataReqMessage Received : page %p from %d", drm.page, drm.orig );

        // Build an answer.
        DataMessage & resp = *((DataMessage*) new char[sizeof(DataMessage) +fheader->size] );
        DEBUG( "Answer size for data for page %p is %lu", drm.page, (long unsigned int )(sizeof(DataMessage) + fheader->size) );
        resp.size = fheader->size;
        resp.page = drm.page;


        DEBUG( "Responding to message ");
        memcpy( resp.data, drm.page, fheader->size );

        // Send it :
        MessageHdr resphdr;
        resphdr.from = get_node_num();
        resphdr.to = drm.orig;
        resphdr.type = DataMessageType;
        resphdr.data = &resp;
        resphdr.data_size = sizeof(DataMessage) + fheader->size;


        send( resphdr );
        DEBUG( "Answer sent.");
        // TODO : replace with smart pointer...
        delete &resp;
        // Then add the recipient to the shared list of nodes :
        // see invalidation.cpp
        register_copy_distribution( drm.page, drm.orig );

}

void NetworkInterface::onRWrite( MessageHdr &m ) {
        // Check state or forward :
        RWrite & rwm = *(RWrite *) m.data;
        owm_frame_layout * fheader = GET_FHEADER( rwm.page );
        if ( ! IS_RESP( fheader ) ) {
          forward( m, fheader->next_resp );

          return;
        }

        // Case of a resp node :
        // Integrate the write :
        memcpy( (char*)rwm.page + rwm.offset, rwm.data, rwm.size );

        // Then send ack :
        RWriteAck rwa;
        rwa.page = rwm.page;

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

        // Not much else to do but to signal.
        signal_write_commited( rwa.serial, rwa.page );

}

void NetworkInterface::onRWReq( MessageHdr &m ) {
        // Check the state and forward if necessary :
        RWReq & rwrm = *(RWReq*) m.data;
        owm_frame_layout * fheader = GET_FHEADER( rwrm.page );

        // Check or forward.
        if ( ! IS_RESP( fheader ) ) {
          forward( m, fheader->next_resp );
          return;
        }

        // If this page has no current writers :
        if ( HAS_ZERO_COUNT( fheader ) ) {
            doRespTransfer(rwrm.page,rwrm.orig);
        } else {

          // We have to solve a RW race :
          if ( fheader->reserved ) {
              // Answers go transitive : means to retry
              // later, and might mean actually transient
              Closure * gotransientClosure = new_Closure(1,
              {GoTransient gtresp;
              gtresp.page = rwrm.page;

              MessageHdr resp;
              resp.from = get_node_num();
              resp.to = rwrm.orig;
              resp.type = GoTransientType;
              resp.data_size = sizeof( GoTransient);
              resp.data = &gtresp;

              send( resp );} );
              register_for_usecount_zero(rwrm.page, gotransientClosure);
              return;
          } else {
            // We will transfer responsibility ASAP.
            Closure * do_transfer = new_Closure (1,
            {doRespTransfer(rwrm.page, rwrm.orig);});
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
        // Set state to valid.
        VALIDATE( fheader );


        // Transfers the responsibility :
        fheader->next_resp = target;
        RespTransfer & resp_transfer = *( RespTransfer * ) new char[sizeof(RespTransfer)+fheader->size];
        resp_transfer.datamsg.size = fheader->size;
        resp_transfer.shared_nodes_bitmap = export_and_clear_shared_set(page);
        // Because we consider ourselves as valid, bitmap contains local node :
        resp_transfer.shared_nodes_bitmap &= 1<<get_node_num();

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
        // Usecount is initialized to zero, it will grow with :
        //  - Immediate increases at "acquire" phases
        //  - Closure-triggered increases when write accessed is signaled.
        fheader->usecount = 0;
        fheader->reserved = false;

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

        if ( !IS_RESP( fheader )  ) {
          forward( m, fheader->next_resp );
          return;
        } 

        planify_invalidation( ai.page, ai.orig );

}

void NetworkInterface::onDoInvalidate( MessageHdr &m ) {
        DoInvalidate &di = *(DoInvalidate*) m.data;
        owm_frame_layout * fheader = GET_FHEADER( di.page );

        DEBUG( "Received DoInvalidate(%p)", di.page);
        CFATAL( IS_RESP( fheader), "Cannot invalidate RESP" );
        CFATAL( IS_TRANSIENT( fheader ), "No transitive message should be received");

        INVALIDATE( fheader );


        // Respond do invalidate.
        AckInvalidate ackinvalidate;
        ackinvalidate.page = di.page;

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
          fheader->next_resp = m.from;
        }

        signal_invalidation_ack(  acki.page );
}


void NetworkInterface::onTDec( MessageHdr &m ) {
        TDec & tdec = *(TDec *) m.data;

        owm_frame_layout * fheader = GET_FHEADER( tdec.page );

        if ( ! IS_RESP( fheader ) ) {
          forward( m, fheader->next_resp );
        }

        // Do Tdec :
        struct frame_struct * frame_struct_p =
                (struct frame_struct*) fheader->data;



}

/*------------ Functions for sending messages---------------------- */
void NetworkInterface::send_invalidate_ack( node_id_t target, PageType page ) {
        if ( target == get_node_num() ) {
          // Just trigger the invalidation as done locally.
          DEBUG("Sending a message locally : this might be a bug");
          signal_invalidation_ack(  page );
          return;
	  

        }


        //  Or actually send the message.
        AckInvalidate ai;
        ai.page = page;

        MessageHdr m;
        m.from = get_node_num();
        m.to = target;
        m.type = AckInvalidateType;
        m.data_size = sizeof(AckInvalidate);
        m.data = &ai;

        send( m );

} 


void NetworkInterface::send_do_invalidate( node_id_t target,  PageType page ) {
        DoInvalidate di;
        di.page = page;

        MessageHdr m;
        m.from = get_node_num();
        m.to = target;
        m.type = DoInvalidateType;
        m.data_size = sizeof(DoInvalidate);
        m.data = &di;

        send( m );
        
}

void NetworkInterface::send_ask_invalidate( node_id_t target, PageType page ) {
        AskInvalidate ai;
        ai.page = page;
        ai.orig = get_node_num();
        
        MessageHdr m;
        m.from = get_node_num();
        m.to = target;
        m.type = AskInvalidateType;
        m.data_size = sizeof( AskInvalidate );
        m.data = &ai;

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





