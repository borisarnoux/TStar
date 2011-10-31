#ifndef FRAME_H
#define FRAME_H




#define VALID 10
#define INVALID 11
#define TRANSIENT 12
#define RESP 13

#define  PROTO_TO_STATE( __fheader, newstate ) do { __fheader->proto_status = newstate; __sync_synchronize(); } while (0)
#define  INVALIDATE( fh ) PROTO_TO_STATE( (fh), INVALID )
#define  TRANSIENTIZE( fh ) PROTO_TO_STATE( (fh), TRANSIENT )
#define  RESPONSIBILIZE( fh ) PROTO_TO_STATE( (fh), RESP )
#define  VALIDATE( fh ) PROTO_TO_STATE( (fh), VALID )

#define IS_VALID_OR_RESP( fh ) ((fh)->proto_status == VALID || (fh)->proto_status == RESP )
#define IS_RESP( fh ) ((fh)->proto_status == RESP )



struct owm_frame_layout {
	size_t data_size; // Size of
	int proto_status; // Fresh, Invalid, TransientWriter, Responsible.
	int version; // In case of Invalid, version number of the last request.
	int pagevt_dir;

	char data[];
}





void * owm_malloc( size_t size ) {
	// Allocates in private segment for data + header.

}


void own_free( size_t size ) {

}

#endif
