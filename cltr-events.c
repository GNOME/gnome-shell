#include <X11/keysym.h>

#include "cltr-events.h"
#include "cltr-private.h"

typedef void (*CltrXEventFunc) (XEvent *xev, gpointer user_data);

typedef struct 
{
  GSource  source;
  Display *display;
  GPollFD  event_poll_fd;
} 
CltrXEventSource;

static gboolean  
x_event_prepare (GSource  *source,
		 gint     *timeout)
{
  Display *display = ((CltrXEventSource*)source)->display;

  *timeout = -1;

  return XPending (display);
}

static gboolean  
x_event_check (GSource *source) 
{
  CltrXEventSource *display_source = (CltrXEventSource*)source;
  gboolean         retval;

  if (display_source->event_poll_fd.revents & G_IO_IN)
    retval = XPending (display_source->display);
  else
    retval = FALSE;

  return retval;
}

static gboolean  
x_event_dispatch (GSource    *source,
		  GSourceFunc callback,
		  gpointer    user_data)
{
  Display *display = ((CltrXEventSource*)source)->display;
  CltrXEventFunc event_func = (CltrXEventFunc) callback;
  
  XEvent xev;

  if (XPending (display))
    {
      XNextEvent (display, &xev);

      if (event_func)
	(*event_func) (&xev, user_data);
    }

  return TRUE;
}

static const GSourceFuncs x_event_funcs = {
  x_event_prepare,
  x_event_check,
  x_event_dispatch,
  NULL
};

void
cltr_dispatch_expose(XExposeEvent *xexpev)
{
  // cltr_photo_grid_redraw(Grid);
}

void
cltr_dispatch_x_event (XEvent  *xevent,
		       gpointer data)
{
  /* Should actually forward on to focussed widget */

  ClutterMainContext *ctx = CLTR_CONTEXT();

  cltr_widget_handle_xevent(ctx->window, xevent);
  
#if 0
  switch (xevent->type)
    {
    case MapNotify:
      CLTR_DBG("Map Notify Event");
      break;
    case Expose:
      CLTR_DBG("Expose"); 	/* TODO COMPRESS */
      cltr_dispatch_expose(&xevent->xexpose);
      break;
    case KeyPress:
      CLTR_DBG("KeyPress");
      cltr_dispatch_keypress(&xevent->xkey);
      break;
    }
#endif 
}

void
cltr_events_init()
{
  GMainContext         *gmain_context;
  int                   connection_number;
  GSource              *source;
  CltrXEventSource     *display_source;

  ClutterMainContext   *ctx = CLTR_CONTEXT();

  /* g_main loop stuff */

  gmain_context = g_main_context_default ();

  g_main_context_ref (gmain_context);

  connection_number = ConnectionNumber (CltrCntx.xdpy);
  
  source = g_source_new ((GSourceFuncs *)&x_event_funcs, 
			 sizeof (CltrXEventSource));

  display_source = (CltrXEventSource *)source;

  display_source->event_poll_fd.fd     = connection_number;
  display_source->event_poll_fd.events = G_IO_IN;
  display_source->display              = CltrCntx.xdpy;
  
  g_source_add_poll (source, &display_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);

  g_source_set_callback (source, 
			 (GSourceFunc) cltr_dispatch_x_event, 
			 NULL  /* no userdata */, NULL);

  g_source_attach (source, gmain_context);
  g_source_unref (source);

  ctx->internal_event_q = g_queue_new();  
}

void
cltr_main_loop()
{
  ClutterMainContext *ctx = CLTR_CONTEXT();

  /*
  GMainLoop          *loop;

  loop = g_main_loop_new (g_main_context_default (), FALSE);

  g_main_loop_run (loop);
  */

  while (TRUE)
    {
      if (!g_queue_is_empty (ctx->internal_event_q))
	{
	  CltrWindow *win    = CLTR_WINDOW(ctx->window);

	  /* Empty the queue  */
	  while (g_queue_pop_head(ctx->internal_event_q) != NULL) ;

	  /* Repaint everything visible from window down - URG. 
           * GL workings make it difficult to paint single part with
           * layering etc..
           * Is this really bad ? time will tell...
	  */
	  cltr_widget_paint(CLTR_WIDGET(win));

	  /* Swap Buffers */
	  glXSwapBuffers(ctx->xdpy, cltr_window_xwin(win));  
	}

      /* while (g_main_context_pending(g_main_context_default ())) */
      g_main_context_iteration (g_main_context_default (), TRUE);

    }
}
