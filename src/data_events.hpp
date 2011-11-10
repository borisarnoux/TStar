#ifndef DATA_EVENTS_HPP
#define DATA_EVENTS_HPP

#include <data_events.hpp>
#include <identifiers.h>
#include <mappable_event.hpp>
#include <fat_pointers.hpp>


void signal_write_arrived( PageType page );
void signal_write_commited( serial_t serial, PageType page);
void signal_data_arrived(PageType page );
void acquire_rec( fat_pointer_p ptr, Closure * t );
void release_rec( fat_pointer_p ptr );


#endif



