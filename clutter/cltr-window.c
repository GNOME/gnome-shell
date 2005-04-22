#include "cltr-window.h"
#include "cltr-private.h"

static gboolean 
cltr_window_handle_xevent (CltrWidget *widget, XEvent *xev);

static void
cltr_window_show(CltrWidget *widget);

static void
cltr_window_paint(CltrWidget *widget);

struct CltrWindow
{
  CltrWidget  widget;  
  Window      xwin;
  CltrWidget *focused_child;
};

void
clrt_window_set_gl_viewport(CltrWindow *win)
{
  CltrWidget *widget = CLTR_WIDGET(win);

  glViewport (0, 0, widget->width, widget->height);
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glOrtho (0, widget->width, widget->height, 0, -1, 1); /* 2d */
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
}

CltrWidget*
cltr_window_new(int width, int height)
{
  ClutterMainContext *ctx = CLTR_CONTEXT();
  CltrWindow *win;

  win = util_malloc0(sizeof(CltrWindow));

  win->widget.width          = width;
  win->widget.height         = height;
  win->widget.show           = cltr_window_show;
  win->widget.paint          = cltr_window_paint;
  win->widget.xevent_handler = cltr_window_handle_xevent;

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

  ctx->window = CLTR_WIDGET(win);

  clrt_window_set_gl_viewport(win);

  return CLTR_WIDGET(win);
}

static void
cltr_window_show(CltrWidget *widget)
{
  ClutterMainContext *ctx = CLTR_CONTEXT();
  CltrWindow         *win = CLTR_WINDOW(widget);

  /* XXX set focused call */
  if (widget->children)
    {
      if (win->focused_child == NULL)
	win->focused_child = g_list_nth_data(widget->children, 0);
    }

  XMapWindow(ctx->xdpy, win->xwin);
}

static void
cltr_window_paint(CltrWidget *widget)
{
  glClear(GL_COLOR_BUFFER_BIT);
  glClearColor( 0.0, 0.0, 0.0, 0.0 ); /* needed for saturate to work */

}

static void
cltr_window_handle_xconfigure(CltrWindow *win, XConfigureEvent *cxev)
{
  
  /* 
     widget.width = cxev->width;
     widget.height = cxev->height;
  */

}			      

static gboolean 
cltr_window_handle_xevent (CltrWidget *widget, XEvent *xev) 
{
  CltrWindow *win = CLTR_WINDOW(widget);

  /* XXX handle exposes here too */

  if (xev->type == Expose)
    {
      cltr_widget_queue_paint(widget);
    }

  /*
    case ConfigureNotify:
    wm_handle_configure_request(w, &ev.xconfigure); break;
  */

  /* XXX Very basic - assumes we are only interested in mouse clicks */
  if (win->focused_child)
    return cltr_widget_handle_xevent(win->focused_child, xev);

  return FALSE;
}

/* window only methods */

Window
cltr_window_xwin(CltrWindow *win)
{
  return win->xwin;
}

void
cltr_window_set_fullscreen(CltrWindow *win)
{
  ClutterMainContext *ctx = CLTR_CONTEXT();

  Atom atom_WINDOW_STATE, atom_WINDOW_STATE_FULLSCREEN;

  atom_WINDOW_STATE 
    = XInternAtom(ctx->xdpy, "_NET_WM_STATE", False);
  atom_WINDOW_STATE_FULLSCREEN 
    = XInternAtom(ctx->xdpy, "_NET_WM_STATE_FULLSCREEN",False);

  XChangeProperty(ctx->xdpy, win->xwin,
		  atom_WINDOW_STATE, XA_ATOM, 32,
		  PropModeReplace,
		  (unsigned char *)&atom_WINDOW_STATE_FULLSCREEN, 1);

  /* 
    XF86VidModeSwitchToMode (GLWin.dpy, GLWin.screen, &GLWin.deskMode);
    XF86VidModeSetViewPort (GLWin.dpy, GLWin.screen, 0, 0);
  */
}


Window
cltr_window_focus_widget(CltrWindow *win, CltrWidget *widget)
{
  /* XXX Should check widget is an actual child of the window */

  ClutterMainContext *ctx = CLTR_CONTEXT();

  win->focused_child = widget;

}


