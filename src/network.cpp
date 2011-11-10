

#include "mpi.h"

#include <misc.h>
#include <omp.h>
#include <frame.hpp>
#include <node.h>
#include <identifiers.h>
#include <network.hpp>
#include <data_events.hpp>

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

struct GoTransitive {
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




void NetworkLowLevel::init( int* argc, char *** argv ) {
      MPI_Init(argc, argv );
      MPI_Comm_rank(MPI_COMM_WORLD, &node_num );
      MPI_Comm_size(MPI_COMM_WORLD, &num_nodes ); 
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
	  
      MPI_Get_count( &s, MPI_CHAR, (int*)&m.data_size );
      m.data = new char[m.data_size];

      MPI_Recv( m.data, m.data_size,
                 MPI_CHAR, s.MPI_SOURCE,
                 s.MPI_TAG, MPI_COMM_WORLD, NULL );
      return true;
}






NetworkInterface::NetworkInterface() : NetworkLowLevel() {
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
      BIND_MT( GoTransitive );
      BIND_MT( RespTransfer );

#undef BIND_MT
}

void NetworkInterface::process_messages() {
      MessageHdr m;
      while ( true ) {
        if ( ! receive( m ) ) {
          return;
        }

        message_type_table[ m.type ] ( m );
      } 

      delete m.data; //Frees the message after processing.
} 



// Forwarding :

void NetworkInterface::forward( MessageHdr &m, node_id_t target ) {
      m.to = target;
      m.from = get_node_num();
      send( m );
}

// Actual handlers
void NetworkInterface::onDataMessage( MessageHdr &m ) {
      // No transitive writers :
      DataMessage & drm = *(DataMessage*)m.data;

      struct owm_frame_layout *fheader = 
        (struct owm_frame_layout*) drm.page;


        fheader->size = drm.size;

        // Error if not invalid 
        if ( ! IS_INVALID( fheader ) ) {
          FATAL( "Violated hypothesis : no transient writers receive data" );
        } 

        // Finally copy :
        memcpy( drm.page, drm.data, drm.size );

        // Signal data arrived :
        signal_data_arrived( drm.page );
}

void NetworkInterface::onDataReqMessage( MessageHdr &m ) {
        DataReqMessage & drm = (DataReqMessage *) m.data;


        owm_frame_layout *fheader = GET_FHEADER( drm.page );
        if ( !IS_RESP( drm.page )) {
          forward( m, fheader->next_resp );
          return;
        }


        // Build an answer.
        DataMessage & resp = *( new char[sizeof(DataMessage) +fheader->size] );

        resp.size = fheader.size;
        memcpy( resp.data, drm.page, fheader->size );

        // Send it :
        MessageHdr resphdr;
        resphdr.from = get_node_num();
        resphdr.to = drm.orig;
        resphdr.type = MessageTypes.DataMessage;
        resphdr.data = &resp;
        resphdr.data_size = sizeof(DataMessage) + fheader->size;

        send( resphdr );

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
        memcpy( rwm.page + rwm.offset, rwm.data, rwm.size );

        // Then send ack :
        RWriteAck rwa;
        rwa.page = rwm.page;

        MessageHdr resphdr;
        resphdr.from = get_node_num();
        resphdr.to = rwm.orig;
        resphdr.type = MessageTypes.RWriteAckType;
        resphdr.data = &rwa;
        resphdr.data_size = sizeof( RWriteAck );

        // Send the message.
        send( resphdr );

}

void NetworkInterface::onRWriteAck( MessageHdr &m ) {
        RWriteAck &rwa = *(RWriteAck*) m.data;

        // Not much else to do but to signal.
        signal_rwrite_ack( rwa.page );

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
          VALIDIZE( fheader );
        } else {

          // This shouldn't happen : (for now ) TODO

          FATAL("RW Races forbidden");

          // Answers go transitive
          GoTransitive gtresp;
          gtresp.page = rwrm.page;		

          MessageHdr resp;
          resp.from = get_node_num();
          resp.to = rwrm.orig;
          resp.type = MessageTypes.GoTransitiveType;
          resp.data_size = sizeof( GoTransitive);
          resp.data = &gtresp;

          send( resp );
          // Nothing more to be done here.
          return;
        }


        // Transfers the responsibility :
        fheader.next_resp = rwrm.orig;
        RespTransfer & resp_transfer = *( RespTransfer * ) new char[sizeof(RespTransfer)+fheader->size];
        resp_transfer.size = fheader->size;
        resp_transfer.shared_nodes_bitmap = export_and_clear_shared_set();
        resp_transfer.page = rwrm.page;
        memcpy( resp_transfer.data, fheader->data, fheader->size );


        MessageHdr resp_msg;
        resp_msg.from = get_node_num();
        resp_msg.to = rwrm.orig;
        resp_msg.type = RespTransferType;
        resp_msg.data = &resp_transfer;
        resp_msg.data_size = sizeof(RespTransfer) + fheader->size;


        send( resp_msg );    

        delete &resp_transfer;
}

void NetworkInterface::onRespTransfer( MessageHdr &m ) {

        RespTransfer &rt = *(RespTransfer*) m.data;
        owm_frame_layout fheader = GET_FHEADER( rt.page );

        CFATAL ( IS_RESP( fheader ), "Already resp, an error occured" );
        CFATAL( IS_TRANSITIVE( fheader ), "Transitive state not allowed to receive resp" );

        // Erases the data :
        memcpy( fheader->data, rt.data, rt.size );
        // Set resp :
        RESPONSIBILIZE( fheader );

        // Signal read and write data arrival :
        signal_data_arrival( page);
        signal_write_arrival( page );

}

void NetworkInterface::onGoTransitive( MessageHdr &m ) {
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

        CFATAL( IS_RESP( fheader), "Cannot invalidate RESP" );
        CFATAL( IS_TRANSITIVE( fheader ), "No transitive message should be received");

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

        if ( !IS_RESP( fheader ) ) {
          // Then resp is the sender :
          CFATAL( ! fheader->initialized, "Unitinialized data shouldn't receive InvalidateAck");
          fheader->next_resp = m.from;
        }

        signal_ack_invalidate(  acki.page ); 
}


void NetworkInterface::onTDec( MessageHdr &m ) {
        TDec & tdec = *(TDec *) m.data;

        owm_frame_layout * fheader = GET_FHEADER( tdec.page );

        if ( ! IS_RESP( fheader ) ) {
          forward( m, fheader->next_resp );
        }



}

/*------------ Functions for sending messages---------------------- */
void NetworkInterface::send_invalidate_ack( node_id_t target, PageType page ) {
        if ( target == get_node_num() ) {
          // Just trigger the invalidation as done locally.
          DEBUG("Sending a message locally : this might be a bug");
          signal_ack_invalidate(  page );
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



