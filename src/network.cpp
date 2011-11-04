


// Routable messages have orig and are forwarded.
enum MessageTypes {

  DataReqType, // Routable. 
  DataMsgType, 

  RWReqType, // Routable.
  RespTransferType, 
  GoTransitiveType,  // Not supported yet (todo)


  AskInvalidateType, // Routable
  DoInvalidateType,
  AckInvalidateType,

  TDecType


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
	

class NetworkHandlers {

  // Forwarding :
  //
  void forward( MessageHdr &m, node_id_t target ) {
    // TODO
    // If Owner.
    // If not owner...

  }

  // Actual handlers

  void onDataMessage( MessageHdr &m ) {
    // No transitive writers :
    DataMessage & drm = *(DataMessage*)m.data;
    
    struct owm_frame_layout *fheader = 
      (struct owm_frame_layout*) drm.page;

    // Check serial :
    if ( drm.serial != fheader->serial ) {
      return;
    }

    // Check resp :
    if ( IS_RESP( fhreader ) ) {
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
  
  void onDataReqMessage( MessageHdr &m ) {
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

  void onRWriteMessage( MessageHdr &m ) {
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

  void onRwriteAckMessage( MessageHdr &m ) {
	RWriteAck &rwa = *(RWriteAck*) m.data;
	
	// Not much else to do but to signal.
	signal_rwrite_ack( rwa.page, rwa.serial );

  }

  void onRWReqMessage( MessageHdr &m ) {
	// Check the state and forward if necessary :
	RWReq & rwrm = *(RWReq*) m.data;
	owm_frame_layout * fheader = GET_HEADER( rwrm.page );
	
	// Check or forward.
	if ( ! IS_RESP( fheader ) ) {
	  forward( m, fheader->next_resp );
	}

	// If this page has no current writers :
	if ( HAS_ZERO_COUNT( fheader ) ) {
	  VALIDIZE( fheader );
	} else {
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
  }

  void onRespTransfer( MessageHdr &m ) {
	
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

  void onGoTransitive( MessageHdr &m ) {
	  FATAL( "Not yet supported" );
  }

  void onAskInvalidate( MessageHdr &m ){
	  AskInvalidate &ai = *(AskInvalidate*) m.data;
	  owm_frame_layout * fheader = GET_FHEADER( ai.page );
	  
	  if ( !IS_RESP( fheader )  ) {
		forward( m, fheader->next_resp );
		return;
	  } 

	  planify_invalidation( ai.serial, ai.page, ai.orig );
	  	  
  }

  void onDoInvalidate( MessageHdr &m ) {
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

  void onAckInvalidate( MessageHdr &m ) {
	AckInvalidate &acki = *(AckInvalidate*) m.data;
	owm_frame_layout * fheader = GET_HEADER( acki.page );
	
    signal_ack_invalidate( acki.serial, acki.page ); 
  }
}

