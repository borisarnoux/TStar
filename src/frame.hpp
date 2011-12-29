#ifndef FRAME_HDR
#define FRAME_HDR

#include <stdlib.h>
#include <identifiers.h>

#include <tstariface.h>
#include <mapper.h>
#include <node.h>
#include <misc.h>
#ifdef __cplusplus
extern "C" {
#endif

#define R_FRAME_TYPE 0x44
#define W_FRAME_TYPE 0x45
#define RW_FRAME_TYPE 0x46
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
#define IS_MALFORMED( fh ) (!IS_INVALID(fh) && !IS_AVAILABLE(fh) && !IS_TRANSIENT(fh))

#define HAS_ZERO_COUNT(fh) ((fh)->usecount == 0)
#define PAGE_IS_TRANSIENT( page ) IS_TRANSIENT( GET_FHEADER(page))

#define PAGE_IS_AVAILABLE( page ) IS_AVAILABLE( GET_FHEADER( page ) )
#define PAGE_IS_VALID( page ) IS_VALID( GET_FHEADER(page) )
#define PAGE_IS_RESP( page ) IS_RESP( GET_FHEADER(page) )
#define PAGE_IS_INVALID(page) IS_INVALID( GET_FHEADER(page))
#define PAGE_HAS_ZERO_COUNT( page ) HAS_ZERO_COUNT( GET_FHEADER(page))
#define PAGE_IS_MALFORMED( page ) IS_MALFORMED( GET_FHEADER(page))

#define GET_FHEADER( page ) __get_fheader( page )



// Canari related functions.
#if DEBUG_ADDITIONAL_CODE
#define CHECK_CANARIES( page ) \
    do { struct owm_frame_layout * fheader = GET_FHEADER(page);\
    CFATAL( fheader->canari != CANARI || fheader->canari2 != CANARI, "Wrong canaries for page %p (%x,%x)", page, fheader->canari, fheader->canari2 );\
} while (0)
#endif

#if DEBUG_ADDITIONAL_CODE
#define SET_CANARIES(fheader) do { fheader->canari=fheader->canari2=CANARI;}while(0)
#define CANARI ((int)0xdeadbeef)
#endif

//
#define PAGE_GET_NEXT_RESP(page) get_next_resp(GET_FHEADER(page))


struct owm_frame_layout {
#if DEBUG_ADDITIONAL_CODE
        int canari;
#endif
	size_t size; // Size of
        int usecount;
        int proto_status; // Valid, Invalid, TransientWriter, Responsible.
	node_id_t next_resp;
        int reserved;
        int freed;
#if DEBUG_ADDITIONAL_CODE
        int canari2;
#endif

	char data[]; 
} __attribute__((__packed__));

static inline owm_frame_layout * __get_fheader( void * page ) {
#if DEBUG_ADDITIONAL_CODE
    if ( mapper_node_who_owns(page)==get_node_num()) {
        mapper_valid_address_check(page);
    }
#endif
    return (struct owm_frame_layout*)((intptr_t)(page) - sizeof(struct owm_frame_layout));
}



static inline node_id_t get_next_resp(owm_frame_layout*fheader) {
    // TODO : Change the code of this (relying on canari values...)
    // Make it more robust. (known to cause errors...)
#if DEBUG_ADDITIONAL_CODE
    if ( fheader->size != 0
         && fheader->next_resp <= get_num_nodes()
          &&fheader->next_resp >= 0
         && fheader->canari == fheader->canari2 &&
         fheader->canari == CANARI
         ) {
        return fheader->next_resp;
    }
#endif

    return mapper_node_who_owns(fheader);
}


#ifdef __cplusplus
}
#endif

#endif
