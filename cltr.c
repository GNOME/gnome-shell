#include "cltr.h"

#include <X11/keysym.h>

/* temp temp temp */

float Zoom = 1.0;
ClutterPhotoGrid *Grid = NULL;

/* ************* */

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
  cltr_photo_grid_redraw(Grid);
}

void
cltr_dispatch_keypress(XKeyEvent *xkeyev)
{
  KeySym kc;

  kc = XKeycodeToKeysym(xkeyev->display, xkeyev->keycode, 0);

  switch (kc)
    {
    case XK_Left:
    case XK_KP_Left:
      cltr_photo_grid_navigate(Grid, CLTR_WEST);
      break;
    case XK_Up:
    case XK_KP_Up:
      cltr_photo_grid_navigate(Grid, CLTR_NORTH);
      break;
    case XK_Right:
    case XK_KP_Right:
      cltr_photo_grid_navigate(Grid, CLTR_EAST);
      break;
    case XK_Down:	
    case XK_KP_Down:	
      cltr_photo_grid_navigate(Grid, CLTR_SOUTH);
      break;
    case XK_Return:
      cltr_photo_grid_activate_cell(Grid);
      break;
    default:
      CLTR_DBG("unhandled keysym");
    }
}

static void
cltr_dispatch_x_event (XEvent  *xevent,
		       gpointer data)
{
  /* Should actually forward on to focussed widget */

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
}

int
cltr_init(int *argc, char ***argv)
{
  int  gl_attributes[] =
    {
      GLX_RGBA, 
      GLX_DOUBLEBUFFER,
      GLX_RED_SIZE, 1,
      GLX_GREEN_SIZE, 1,
      GLX_BLUE_SIZE, 1,
      0
    };

  XVisualInfo	       *vinfo;  

  GMainContext         *gmain_context;
  int                   connection_number;
  GSource              *source;
  CltrXEventSource     *display_source;

  g_thread_init (NULL);
  // XInitThreads ();

  if ((CltrCntx.xdpy = XOpenDisplay(getenv("DISPLAY"))) == NULL)
    {
      return 0;
    }

  CltrCntx.xscreen   = DefaultScreen(CltrCntx.xdpy);
  CltrCntx.xwin_root = RootWindow(CltrCntx.xdpy, CltrCntx.xscreen);

  if ((vinfo = glXChooseVisual(CltrCntx.xdpy, 
			       CltrCntx.xscreen,
			       gl_attributes)) == NULL)
    {
      fprintf(stderr, "Unable to find visual\n");
      return 0;
    }

  CltrCntx.gl_context = glXCreateContext(CltrCntx.xdpy, vinfo, 0, True);

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

  return 1;
}


ClutterWindow*
cltr_window_new(int width, int height)
{
  ClutterWindow *win;

  win = util_malloc0(sizeof(ClutterWindow));

  win->width  = width;
  win->height = height;

  win->xwin = XCreateSimpleWindow(CltrCntx.xdpy,
				  CltrCntx.xwin_root,
				  0, 0,
				  width, height,
				  0, 0, WhitePixel(CltrCntx.xdpy, 
						   CltrCntx.xscreen));

  XSelectInput(CltrCntx.xdpy, win->xwin, 
	       StructureNotifyMask|ExposureMask|
	       KeyPressMask|PropertyChangeMask);

  glXMakeCurrent(CltrCntx.xdpy, win->xwin, CltrCntx.gl_context);

  /* All likely better somewhere else */

  /* view port */
  glViewport (0, 0, width, height);
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glOrtho (0, width, height, 0, -1, 1); /* 2d */
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  glEnable        (GL_TEXTURE_2D);
  glTexParameteri (GL_TEXTURE_2D,  GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D,  GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexEnvi       (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,   GL_REPLACE);

  return win;
}

void
cltr_main_loop()
{
  GMainLoop *loop;

  loop = g_main_loop_new (g_main_context_default (), FALSE);

  CLTR_MARK();

  g_main_loop_run (loop);

}

gboolean
test_idle_cb(gpointer data)
{
  static float angle = 0.0;

  glClear(GL_COLOR_BUFFER_BIT);
  glClearColor( 0.6, 0.6, 0.62, 1.0);

  glColor3f(0.0, 0.0, 0.0);

  glTranslatef( 0.0, 0.0, 0.0);

  /* get a copy of our untranformed matrix */
  glPushMatrix();

  /* Translate origin to rotation point */
  glTranslatef( 320.0, 240.0, 0.0);

  /* Rotate around Z axis */
  glRotatef ( angle, 0.0, 0.0, 1.0);

  /* Draw tranformed rect with origin at center */
  glRecti(-50, -50, 50, 50);

  /* Reset our un transformed matrix */
  glPopMatrix();

  glRecti(-50, -50, 50, 50);

  angle += 0.1;

  glXSwapBuffers(CltrCntx.xdpy, (Window)data);  

  return TRUE;
}



int
main(int argc, char **argv)
{
  ClutterPhotoGrid *grid = NULL;
  ClutterWindow    *win = NULL;

  if (argc < 2)
    {
      g_printerr("usage: '%s' <path to not too heavily populated image dir>\n"
		 , argv[0]);
      exit(-1);
    }

  cltr_init(&argc, &argv);

  win = cltr_window_new(640, 480);

  grid = cltr_photo_grid_new(win, 4, 4, argv[1]);

  Grid = grid; 			/* laaaaaazy globals */

  // cltr_photo_grid_redraw(grid);

  // g_idle_add(idle_cb, grid);

  // g_timeout_add (10, idle_cb, grid);

  // g_idle_add(test_idle_cb, (gpointer)win->xwin);

  XFlush(CltrCntx.xdpy);

  XMapWindow(CltrCntx.xdpy, win->xwin);

  XFlush(CltrCntx.xdpy);

  cltr_main_loop();

  return 0;
}
