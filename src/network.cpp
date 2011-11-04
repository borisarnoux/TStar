


// Routable messages have orig and are forwarded.
enum MessageTypes {

  DataReqType, // Routable. 
  DataMsgType, 

  RWReqType, // Routable.
  RespTransferType, 
  GoTransitiveType,  // Not supported yet (todo)


  AskInvalidate, // Routable
  DoInvalidate,
  AckInvalidate,

  TDec


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

  }

  void onRWReqMessage( MessageHdr &m ) {
  
  }

  void onRespTransfer( MessageHdr &m ) {

  }

  void onGoTransitive( MessageHdr &m ) {
      FATAL( "NotImplemented");
  }

  void onAskInvalidate( MessageHdr &m ){

  }

  void onDoInvalidate( MessageHdr &m ) {

  }

  void onAckInvalidate( MessageHdr &m ) {
    signalAckInvalidate(
  }


}

