#include "cltr-window.h"
#include "cltr-private.h"

static gboolean 
cltr_window_handle_xevent (CltrWidget *widget, XEvent *xev);

static void
cltr_window_show(CltrWidget *widget);

struct CltrWindow
{
  CltrWidget  widget;  
  Window      xwin;
  CltrWidget *focused_child;
};

CltrWidget*
cltr_window_new(int width, int height)
{
  ClutterMainContext *ctx = CLTR_CONTEXT();
  CltrWindow *win;

  win = util_malloc0(sizeof(CltrWindow));

  win->widget.width          = width;
  win->widget.height         = height;
  win->widget.show           = cltr_window_show;
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

  /* view port */
  glViewport (0, 0, width, height);
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glOrtho (0, width, height, 0, -1, 1); /* 2d */
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  ctx->window = CLTR_WIDGET(win);

  return CLTR_WIDGET(win);
}

Window
cltr_window_xwin(CltrWindow *win)
{
  return win->xwin;
}

static void
cltr_window_show(CltrWidget *widget)
{
  ClutterMainContext *ctx = CLTR_CONTEXT();
  CltrWindow         *win = CLTR_WINDOW(widget);

  /* XXX set focused call */
  if (widget->children)
    {
      win->focused_child = g_list_nth_data(widget->children, 0);
    }

  XMapWindow(ctx->xdpy, win->xwin);
}

static gboolean 
cltr_window_handle_xevent (CltrWidget *widget, XEvent *xev) 
{
  CltrWindow *win = CLTR_WINDOW(widget);

  /* XXX handle exposes here too */

  if (xev->type == Expose)
    {
      ;
    }

  /* XXX Very basic - assumes we are only interested in mouse clicks */
  if (win->focused_child)
    return cltr_widget_handle_xevent(win->focused_child, xev);

  return FALSE;
}

/* window only */



