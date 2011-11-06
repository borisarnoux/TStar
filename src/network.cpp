





// Routable messages have orig and are forwarded.
enum MessageTypes {

  DataReqMessageType, // Routable. 
  DataMessageType, // From Resp

  RWReqType, // Routable.
  RespTransferType, 
  GoTransitiveType,  // Not implemented RW races yet (triggers error, TODO)

  RWriteType,
  RWriteAckType, // From Resp


  AskInvalidateType, // Routable
  DoInvalidateType, // From Resp.
  AckInvalidateType,

  TDecType // Routable


}


struct MessageHdr {
  int from;
  int to;
  MessageTypes type;
  void * data;
  size_t data_size;
}

struct DataMessage {
  int serial;
  size_t size;
  char[] data;
}

struct DataReqMessage{
  int serial;
  int orig;
  void * page;
}

struct RWrite {
  int orig;
  int serial;
  void * page;
  size_t offset;
  size_t size
    char data[];
}

struct RWriteAck {
  void *page;
  int serial;
}

struct RWReq {
  int orig;
  void * page;
}

struct RespTransfer {
  void *page;
  long long shared_nodes_bitmap;
  DataMessage datamsg;
}

struct GoTransitive {
  void * page;
}

struct AckInvalidate {
  int serial;
  void * page;
}

struct DoInvalidate {
  int serial;
  void * page;
}

struct AskInvalidate {
  node_id_t orig;
  int serial;
  void * page;
}

struct TDec {
  int orig;
  int val;
  void * page;
}


class NetworkLowLevel {
public:
    bool initialized;
    int num_nodes;
    int node_num;

    NetworkLowLevel():
      initialized(0),num_nodes(0),node_num(0) {

    }

    void init( int* argc, char *** argv ) {
      MPI_Init(argc, argv );
      MPI_Comm_rank(MPI_COMM_WORLD, &node_num );
      MPI_Comm_size(MPI_COMM_WORLD, &num_nodes ); 
    }

    bool receive( MessageHdr &m ) {
      MPI_Status s;
      int flag = 0;
      MPI_Iprove( MPI_ANY_SOURCE,
                  MPI_ANY_TAG,
                  MPI_COMM_WORLD,
                  &flag, &s );
      if ( !flag ) {
         return false;
      }

      m.from = s.MPI_SOURCE;
      m.to   = get_node_num();
      m.type = s.MPI_TAG;
      // Get size :
      MPI_Get_Count( &s, MPI_CHAR, &m.data_size );
      m.data = new char[m.data_size];

      MPI_Recv( m.data, m.data_size,
                 MPI_CHAR, s.MPI_SOURCE,
                 s.MPI_TAG, MPI_COMM_WORLD, NULL );
      return true;
    }

    void send( MessageHdr &m ) {
      MPI_Send( m.data, m.data_size, MPI_CHAR, m.to, m.type, MPI_COMM_WORLD);

    }
}

class NetworkInterface : protected NetworkLowLevel {

  private:
    void (* message_type_table[100] )(MessageHdr &);

  public:

    NetworkInterface() : NetworkLowLevel() {
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

    void process_messages() {
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
    //
    void forward( MessageHdr &m, node_id_t target ) {
      m.to = target;
      m.from = get_node_num();
      send( m );
    }

    // Actual handlers

    static void onDataMessage( MessageHdr &m ) {
      // No transitive writers :
      DataMessage & drm = *(DataMessage*)m.data;

      struct owm_frame_layout *fheader = 
        (struct owm_frame_layout*) drm.page;

      // Check serial :
      if ( drm.serial != fheader->serial ) {
        return;
          return;
        }

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

      static void onDataReqMessage( MessageHdr &m ) {
        DataReqMessage & drm = (DataReqMessage *) m.data;


        owm_frame_layout *fheader = GET_FHEADER( drm.page );
        if ( !IS_RESP( drm.page )) {
          forward( m, fheader->next_resp );
          return;
        }


        // Build an answer.
        DataMessage & resp = *( new char[sizeof(DataMessage) +fheader->size] );

        resp.serial = drm.serial;
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

      static void onRWrite( MessageHdr &m ) {
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
        rwa.serial = rwm.serial;
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

      static void onRwriteAck( MessageHdr &m ) {
        RWriteAck &rwa = *(RWriteAck*) m.data;

        // Not much else to do but to signal.
        signal_rwrite_ack( rwa.page, rwa.serial );

      }

      static void onRWReq( MessageHdr &m ) {
        // Check the state and forward if necessary :
        RWReq & rwrm = *(RWReq*) m.data;
        owm_frame_layout * fheader = GET_HEADER( rwrm.page );

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

      static void onRespTransfer( MessageHdr &m ) {

        RespTransfer &rt = *(RespTransfer*) m.data;
        owm_frame_layout fheader = GET_FHEADER( rt.page );

        CFATAL ( IS_RESP( fheader ), "Already resp, an error occured" );
        CFATAL( IS_TRANSITIVE( fheader ), "Transitive state not allowed to receive resp" );

        // Erases the data :
        memcpy( fheader->data, rt.data, rt.size );
        // Set resp :
        RESPONSIBILIZE( fheader );

        // Signal read and write data arrival :
        signal_data_arrival( page, 0 ); // special serial
        signal_write_arrival( page );

      }

      static void onGoTransitive( MessageHdr &m ) {
        FATAL( "Not yet supported" );
      }

      static void onAskInvalidate( MessageHdr &m ){
        AskInvalidate &ai = *(AskInvalidate*) m.data;
        owm_frame_layout * fheader = GET_FHEADER( ai.page );

        if ( !IS_RESP( fheader )  ) {
          forward( m, fheader->next_resp );
          return;
        } 

        planify_invalidation( ai.serial, ai.page, ai.orig );

      }

      static void onDoInvalidate( MessageHdr &m ) {
        DoInvalidate &di = *(DoInvalidate*) m.data;
        owm_frame_layout * fheader = GET_FHEADER( di.page );

        CFATAL( IS_RESP( fheader), "Cannot invalidate RESP" );
        CFATAL( IS_TRANSITIVE( fheader ), "No transitive message should be received");

        INVALIDATE( fheader );

        // Respond do invalidate.
        AckInvalidate ackinvalidate;
        ackinvalidate.serial = di.serial;
        ackinvalidate.page = di.page;

        MessageHdr resp;
        resp.from = get_node_num();
        resp.to = m.from;
        resp.type = AckInvalidateType;
        resp.data_size = sizeof( AckInvalidate);
        resp.data = & ackinvalidate;

        send( resp );


      }

      static void onAckInvalidate( MessageHdr &m ) {
        AckInvalidate &acki = *(AckInvalidate*) m.data;
        owm_frame_layout * fheader = GET_HEADER( acki.page );

        if ( !IS_RESP( fheader ) ) {
          // Then resp is the sender :
          CFATAL( ! fheader->initialized, "Unitinialized data shouldn't receive InvalidateAck");
          fheader->next_resp = m.from;
        }

        signal_ack_invalidate( acki.serial, acki.page ); 
      }


      static void onTDec( MessageHdr &m ) {
        TDec & tdec = *(TDec *) m.data;

        owm_frame_layout * fheader = GET_FHEADER( tdec.page );

        if ( ! IS_RESP( fheader ) ) {
          forward( m, fheader->next_resp );
        }



      }

      /*------------ Functions for sending messages---------------------- */
      static void send_invalidate_ack( node_id_t target, serial_t serial, PageType page ) {
        if ( target == get_node_num() ) {
          // Just trigger the invalidation as done locally.
          DEBUG("Sending a message locally : this might be a bug");
          signal_ack_invalidate( serial, page );
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

      static void send_do_invalidate( node_id_t target, serial_t serial, PageType page ) {
        DoInvalidate di;
        di.serial = serial;
        di.page = page;

        MessageHdr m;
        m.from = get_node_num();
        m.to = target;
        m.type = DoInvalidateType;
        m.data_size = sizeof(DoInvalidate);
        m.data = &di;

        send( m );
        
      }

      static void send_ask_invalidate( node_id_t target, serial_t serial, PageType page ) {
        AskInvalidate ai;
        ai.serial = serial;
        ai.page = page;
        
        MessageHdr m;
        m.from = get_node_num();
        m.to = target;
        m.type = AskInvalidateType;
        m.data_size = sizeof( AskInvalidate );
        m.data = &ai;

        send( m );
      }

      static void send_data_req( node_id_t target, serial_t serial, void * page ) {
        DataReqMessage drm;
        drm.page = page;
        drm.orig = get_node_num();
        drm.serial = serial;

        MessageHdr m;
        m.from = get_node_num();
        m.to = target;
        m.type = DataReqMessageType;
        m.data_size = sizeof( DataReqMessage );
        m.data = &drm;

        send( m );
      }

      static void send_resp_req( node_id_t target, void * page ) {
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

      static void send_tdec( node_id_t target, void * page, int val ) {
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

    }

