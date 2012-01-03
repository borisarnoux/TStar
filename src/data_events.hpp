#ifndef DATA_EVENTS_HPP
#define DATA_EVENTS_HPP

#include <data_events.hpp>
#include <identifiers.h>
#include <mappable_event.hpp>

void register_for_usecount_zero( PageType page, Closure * c );
void signal_usecount_zero( PageType page );

void signal_write_arrival( PageType page );
void signal_write_commited( serial_t serial, PageType page);
void signal_data_arrival(PageType page );
void register_for_data_arrival( PageType page, Closure * c );
void register_for_write_arrival( PageType page, Closure * c );
void request_page_data( PageType page );
void request_page_resp( PageType page );
void ask_or_do_tdec( void * page );
void ask_or_do_rwrite_then( PageType page, size_t offset, void *input_buffer, size_t len, Closure * c );

#endif



