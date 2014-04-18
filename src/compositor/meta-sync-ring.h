#ifndef _META_SYNC_RING_H_
#define _META_SYNC_RING_H_

#include <glib.h>

#include <X11/Xlib.h>

gboolean meta_sync_ring_init (Display *dpy);
void meta_sync_ring_destroy (void);
gboolean meta_sync_ring_after_frame (void);
gboolean meta_sync_ring_insert_wait (void);
void meta_sync_ring_handle_event (XEvent *event);

#endif  /* _META_SYNC_RING_H_ */
