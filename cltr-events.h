#ifndef _HAVE_CLTR_EVENT_H
#define _HAVE_CLTR_EVENT_H

#include "cltr.h"



void
cltr_main_loop();

void
cltr_dispatch_x_event (XEvent  *xevent,
		       gpointer data);

void
cltr_events_init();

#endif
