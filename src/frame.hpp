#ifndef FRAME_HDR
#define FRAME_HDR

#include <stdlib.h>
#include <identifiers.h>

#include <tstariface.h>

#ifdef __cplusplus
extern "C" {
#endif

#define R_FRAME_TYPE 0x44
#define W_FRAME_TYPE 0x45
#define RW_FRAME_TYPE 0X46
#define FATP_TYPE 0x34
#define DATA_TYPE 0x35


#define VALID 10
#define INVALID 0
#define TRANSIENT 12
#define RESP 13

#define  PROTO_TO_STATE( __fheader, newstate ) do { __fheader->proto_status = newstate; __sync_synchronize(); } while (0)
#define  INVALIDATE( fh ) PROTO_TO_STATE( (fh), INVALID )
#define  TRANSIENTIZE( fh ) PROTO_TO_STATE( (fh), TRANSIENT )
#define  RESPONSIBILIZE( fh ) PROTO_TO_STATE( (fh), RESP )
#define  VALIDATE( fh ) PROTO_TO_STATE( (fh), VALID )

#define IS_INVALID( fh ) ((fh)->proto_status == INVALID )
#define IS_AVAILABLE( fh ) IS_VALID_OR_RESP(fh)
#define IS_VALID_OR_RESP( fh ) ((fh)->proto_status == VALID || (fh)->proto_status == RESP )
#define IS_RESP( fh ) ((fh)->proto_status == RESP )
#define IS_VALID( fh ) ((fh)->proto_status == VALID )
#define IS_TRANSIENT( fh ) ((fh)->proto_status == TRANSIENT )

#define HAS_ZERO_COUNT(fh) ((fh)->usecount == 0)
#define PAGE_IS_TRANSIENT( page ) IS_TRANSIENT( GET_FHEADER(page))

#define PAGE_IS_AVAILABLE( page ) IS_AVAILABLE( GET_FHEADER( page ) )
#define PAGE_IS_VALID( page ) IS_VALID( GET_FHEADER(page) )
#define PAGE_IS_RESP( page ) IS_RESP( GET_FHEADER(page) )
#define PAGE_IS_INVALID(page) IS_INVALID( GET_FHEADER(page))
#define PAGE_HAS_ZERO_COUNT( page ) HAS_ZERO_COUNT( GET_FHEADER(page))


#define GET_FHEADER( page ) ((struct owm_frame_layout*)((intptr_t)(page) - sizeof(struct owm_frame_layout)))
#define CHECK_CANARIES( page ) \
    do { struct owm_frame_layout * fheader = GET_FHEADER(page);\
    CFATAL( fheader->canari != CANARI || fheader->canari2 != CANARI, "Wrong canaries for page %p", page );\
} while (0)

#define CANARI ((int)0xdeadbeef)


struct owm_frame_layout {
        int canari;
	size_t size; // Size of
        int usecount;
	int proto_status; // Fresh, Invalid, TransientWriter, Responsible.
	node_id_t next_resp;
        int reserved;
        int canari2;

	char data[]; 
} __attribute__((__packed__));




#ifdef __cplusplus
}
#endif

#endif
