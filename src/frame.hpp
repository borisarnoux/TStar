#ifndef FRAME_HDR
#define FRAME_HDR

#include <stdlib.h>
#include <identifiers.h>

#ifdef __cplusplus
extern "C" {
#endif


#define VALID 10
#define INVALID 0
#define TRANSIENT 12
#define RESP 13

#define  PROTO_TO_STATE( __fheader, newstate ) do { __fheader->proto_status = newstate; __sync_synchronize(); } while (0)
#define  INVALIDATE( fh ) PROTO_TO_STATE( (fh), INVALID )
#define  TRANSIENTIZE( fh ) PROTO_TO_STATE( (fh), TRANSIENT )
#define  RESPONSIBILIZE( fh ) PROTO_TO_STATE( (fh), RESP )
#define  VALIDATE( fh ) PROTO_TO_STATE( (fh), VALID )


#define IS_AVAILABLE_( fh ) IS_VALID_OR_RESP(fh)
#define IS_VALID_OR_RESP( fh ) ((fh)->proto_status == VALID || (fh)->proto_status == RESP )
#define IS_RESP( fh ) ((fh)->proto_status == RESP )

#define PAGE_IS_AVAILABLE( page ) IS_AVAILABLE( GET_FHEADER( page ) )
#define PAGE_IS_VALID( page ) IS_VALID( GET_FHEADER(page) )
#define PAGE_IS_RESP( page ) IS_RESP( GET_FHEADER(page) )


#define GET_FHEADER( page ) ((struct owm_frame_layout*)((intptr_t)(page) - sizeof(struct owm_frame_layout)))



struct owm_frame_layout {
	size_t size; // Size of
	int proto_status; // Fresh, Invalid, TransientWriter, Responsible.
	int version;      // In case of Invalid, version number of the last request.
	node_id_t next_resp;
	char data[]; 
};


struct frame_struct {
  struct static_data * static_data;
  long sc;
  long args[];
};

struct static_data {
  void (*fn)();
  int sc;
  long arg_types[];
};



#ifdef __cplusplus
}
#endif

#endif
