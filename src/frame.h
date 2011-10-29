#ifndef FRAME_H
#define FRAME_H


#define INVALIDATE( fheader ) TODO // Define that


void signal_page_arrival( int pagevt_dir, void * page );


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

#endif
