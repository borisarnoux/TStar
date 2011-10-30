

#include "frame.h"

// Object descriptor.
struct range_desc {
	size_t offset;
	size_t size;
}

struct object_desc {
	int nranges;
	struct range_desc  ranges[];
}

#define MSG_TYPE_MAX 256


#define DATAMSG_TYPE 11
#define DATAREQ_TYPE 12

struct datamsg {
	void * addr;
	size_t size;
	char data[];
}


struct datareq {
	int orig;
	int serial;
	void * addr;
}



#define RWREQ_TYPE 21
struct rwreq {
	int orig;
	void * addr;
}


#define RESPTRANSFER_TYPE 22
struct resptransfer {
	void * addr;
	size_t size;
	char data[];
}



#define GOTRANSITIVE_TYPE 23
struct gotransitive {
	void * addr;
}


#define IMRESP_TYPE 24
struct imresp {
	void * addr;
}


#define INVALIDATECACHE_TYPE 31
struct invalidatecache {
	void * addr;
}


#define INVALIDATE_ACK_TYPE
struct invalidate_ack {
	void * addr;
}

// More specific messages.
#define RWRITE_TYPE 111
struct rwrite {
	// Contains the data.
    int orig;
	int serial;
	struct object_desc object_desc;
	// Followed by data.
}



#define RWRITE_ACK_TYPE  112
struct rwrite_ack {
	int serial;
	void * addr;
}


// Dataflow specific.
#define TDEC_TYPE   121
struct tdec {
	void * addr; // Target.
	int decrement;
}


// Scheduler related.
#define STEAL_TYPE  131
struct steal {
	int requested_n;
}


#define STOLEN_TYPE 132
struct stolen {
	int ntasks;
	int stolen_ids[];
}


#define GET_FRHEADER(name,addr) struct owm_frame_layout * name = (struct owm_frame_layout *) addr

// Sending is asynchronous.
extern int send ( int sender, int receiver, int type, void * data, size_t data_size );
extern void data_arrived( void  * page );
extern void w_arrived( void  * page );
extern void resp_arrived( void * page );

// For receiving, conform to one of those.
typedef int (*handler_sig)(int src,
		int dest,
		int type,
		void * data,
		size_t data_size );

// And register.
extern void register_handler( int type, handler_sig h );


int datareq_handler( int src, int dest, int type, void * data, size_t data_size ) {
	// Here the datareq is handled
	struct datareq * datareq = ( struct datareq * ) data;
	struct owm_frame_layout * fheader  = datareq->addr - sizeof( struct owm_frame_layout );

	// TODO
	// If the address corresponds to a local responsible, send the copy.
	// Otherwise, forward it to the next responsible ( see owm header ).

	if ( IAM_RESP( fheader ) ) {
		// Prepare a packet for the data.
		// TODO : avoid copies here.
		struct datamsg * buffer = malloc( sizeof( struct datamsg ) + fheader->size );
		buffer->size = fheader->size;
		buffer->addr = datareq->addr;

		memcpy( buffer->data, datareq->addr, fheader->size );
		send( get_node_num(), datareq->reqsource,
		      DATAMSG_TYPE,
		      buffer, sizeof( struct datamsg ) + fheader->size );
	} else {
		// Forward it to the next known responsible.
		send( get_node_num(), fheader->nextresp, type, data, data_size );
	}
}








int datamsg_handler( int src, int dest, int type, void * data, size_t data_size ) {
	// Check serial. If serial up to date, make the update.
	struct datamsg * datamsg = (struct datamsg * ) data;
	struct owm_frame_layout * fheader  = datareq->addr - sizeof( struct owm_frame_layout );
	if (datamsg->serial != fheader->serial)
		// Otherwise : ignore it, because its not an up to date version.
		LOG( "Out of order data msg ignored.");
		return -1;
	}

	if (  && ! IS_TRANSIENT( fheader ) ) {
		// make the update :
		memcpy( fheader->data, datamsg->data, datamsg->size);
		fheader->size = datamsg->size;
	}
	// If page is currently in transient write : call a special function for wrtiting some of the objects only.
	if ( IS_TRANSIENT( fheader ) ) {
		select_cpy( datamsg->datafheader->data );
	}

	// Then call the data arrived callback ( TODO )




}


int rwreq_handler( int src, int dest, int type, void * data, size_t data_size ) {
	// Unpacks address
	struct rwreq * rwreq = (struct rwreq *) data;
	struct owm_frame_layout * fheader  = rwreq->addr - sizeof( struct owm_frame_layout );

    // If owner, but not resp, forward message.

    // Check for availability :
    if ( is_available( rwreq->addr )) {
        transfer_resp( rwreq->orig );
    } else {
        // Send go transitive message to orig.
        struct gotransitive msg;
        msg.addr = rwreq.addr;
        send( get_node_num(), rwreq.orig, GOTRANSITIVE_TYPE, &msg, sizeof( struct gotransitive));
    }

}


int rwrite_ack_handler( int src, int dest, int type, void * data, size_t data_size ) {
    struct rwrite_ack * rwrite_ack = (struct rwrite_ack *)data;

	struct owm_frame_layout * fheader  = rwrite_ack->addr - sizeof( struct owm_frame_layout );

    signal_write_commited( fheader->addr );


}


int invalidatecache_handler( int src, int dest, int type, void * data, size_t data_size ) {
	// Unpacks header address.
	struct invalidatecache * invalidatecache = (struct invalidatecache * ) data;
	struct owm_frame_layout * fheader  = invalidatecache->addr - sizeof( struct owm_frame_layout );
	// Does invalidation locally.
	INVALIDATE( fheader);
	// Fences invalidation locally.
	// Replies with invalidation ack.
	struct invalidate_ack msg;
	msg.addr = invalidatecache->addr;

	send( get_node_num(), src, INVALIDATE_ACK_TYPE, &msg, sizeof( struct invalidate_ack ) );
}


int gotransitive_handler( int src, int dest, int type, void * data, size_t data_size ) {
    GET_FHDR(gotransitive,fheader);

	SET_TRANSITIVE( fheader );
	signal_write_arrived( fheader->data );
}



