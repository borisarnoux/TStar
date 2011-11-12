#include "mpi.h"


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


};


struct MessageHdr {
  int from;
  int to;
  MessageTypes type;
  void * data;
  size_t data_size;
};







class NetworkLowLevel {
public:
    bool initialized;
    int num_nodes;
    int node_num;

    NetworkLowLevel():
      initialized(0),num_nodes(0),node_num(0) {

    }

    void init( int* argc, char *** argv );

    bool receive( MessageHdr &m );

    static inline void send( MessageHdr &m ) {
      MPI_Send( m.data, m.data_size, MPI_CHAR, m.to, m.type, MPI_COMM_WORLD);

    }
};

class NetworkInterface : protected NetworkLowLevel {

  private:
    void (* message_type_table[100] )(MessageHdr &);

  public:

    NetworkInterface();

    void process_messages();


    // Forwarding :
    //
    static void forward( MessageHdr &m, node_id_t target );

    // Actual handlers

    static void onDataMessage( MessageHdr &m );

	static void onDataReqMessage( MessageHdr &m );


    static void onRWrite( MessageHdr &m );

    static void onRWriteAck( MessageHdr &m );

    static void onRWReq( MessageHdr &m );

	static void onRespTransfer( MessageHdr &m ) ;


    static void onGoTransitive( MessageHdr &m ) ;

    static void onAskInvalidate( MessageHdr &m );

    static void onDoInvalidate( MessageHdr &m );

    static void onAckInvalidate( MessageHdr &m );


    static void onTDec( MessageHdr &m );

      /*------------ Functions for sending messages---------------------- */
    static void send_invalidate_ack( node_id_t target, PageType page );

    static void send_do_invalidate( node_id_t target, PageType page );

    static void send_ask_invalidate( node_id_t target,  PageType page );

    static void send_data_req( node_id_t target,  void * page );

    static void send_resp_req( node_id_t target, void * page );

    static void send_tdec( node_id_t target, void * page, int val );
};

