#ifndef NETWORK_HPP
#define NETWORK_HPP

#include "mpi.h"
#include <node.h>
#include <identifiers.h>

// Routable messages have orig and are forwarded.
enum MessageTypes {

  DataReqMessageType, // Routable. 
  DataMessageType, // From Resp

  RWReqType, // Routable.
  RespTransferType, 
  GoTransientType,  // Manages RW Races.
        //For now, used for failure : future real conccurent writers ?

  RWriteType,
  RWriteAckType, // From Resp


  AskInvalidateType, // Routable
  DoInvalidateType, // From Resp.
  AckInvalidateType,

  TDecType, // Routable
  ExitMessageType,
  StealMessageType,
  StolenMessageType,
 /* For debug */
  TransPtrType,
  FreeMessageType,
  LastMessageType

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
    void finalize();

    bool receive( MessageHdr &m ) const;

    static inline void send( MessageHdr &m ) {
        if ( m.to == get_node_num()) {
            DEBUG( "Warning : sending a message to self.");
        }
      MPI_Send( m.data, m.data_size, MPI_CHAR, m.to, m.type, MPI_COMM_WORLD);

    }
};

class NetworkInterface : public NetworkLowLevel {

  private:
    void (* message_type_table[100] )(MessageHdr &);
    static void * dbg_ptr_holder;
    static int dbg_ptr_signal;
  public:
    static int stolen_message_onflight;

    static NetworkInterface * global_network_interface;

    NetworkInterface();

    void process_messages() const ;


    // Forwarding :
    //
    static void forward( MessageHdr &m, node_id_t target );

    // Actual handlers

    static void onDataMessage( MessageHdr &m );

    static void onDataReqMessage( MessageHdr &m );


    static void onRWrite( MessageHdr &m );

    static void onRWriteAck( MessageHdr &m );

    static void onRWReq( MessageHdr &m );
    static void doRespTransfer(PageType page,  node_id_t target );


    static void onRespTransfer( MessageHdr &m ) ;


    static void onGoTransient( MessageHdr &m ) ;

    static void onAskInvalidate( MessageHdr &m );

    static void onDoInvalidate( MessageHdr &m );

    static void onAckInvalidate( MessageHdr &m );
    static void onExitMessage( MessageHdr &m );

    static void onTDec( MessageHdr &m );

    static void onStealMessage( MessageHdr &m );
    static void onStolenMessage( MessageHdr &m );
    static void onFreeMessage(MessageHdr &m);
      /*------------ Functions for sending messages---------------------- */
    static void send_invalidate_ack( node_id_t target, PageType page, serial_t serial );

    static void send_do_invalidate( node_id_t target, PageType page, serial_t serial );

    static void send_ask_invalidate( node_id_t target,  PageType page, serial_t serial );

    static void send_data_req( node_id_t target,  void * page );

    static void send_resp_req( node_id_t target, void * page );

    static void send_tdec( node_id_t target, void * page, int val );


    static void send_rwrite( node_id_t target,
                                        serial_t serial,
                                        void * page,
                                        size_t offset,
                                        void * buffer,
                                        size_t len) ;

    static void send_steal_message( node_id_t target, int amount );
    static void send_stolen_message( node_id_t target, int amont, struct frame_struct ** buffer );
    static void send_free_message( node_id_t target, PageType page );

    static void bcast_exit( int code );

    static void dbg_send_ptr( node_id_t target, void * ptr );
    void dbg_get_ptr( void ** ptrp );
    static void onTransPtr(MessageHdr &msg);
    void wait_for_stolen_task();
};

#endif
