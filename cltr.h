#ifndef _HAVE_CLTR_H
#define _HAVE_CLTR_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <X11/Xlib.h>

#include <GL/glx.h>
#include <GL/gl.h>

#include <glib.h>

#include "pixbuf.h"
#include "fonts.h"

#define CLTR_WANT_DEBUG 1

#if (CLTR_WANT_DEBUG)

#define CLTR_DBG(x, a...) \
 g_printerr ( __FILE__ ":%d,%s() " x "\n", __LINE__, __func__, ##a)

#define CLTR_GLERR()                                           \
 {                                                             \
  GLenum err = glGetError (); 	/* Roundtrip */                \
  if (err != GL_NO_ERROR)                                      \
    {                                                          \
      g_printerr (__FILE__ ": GL Error: %x [at %s:%d]\n",      \
		  err, __func__, __LINE__);                    \
    }                                                          \
 }

#else

#define CLTR_DBG(x, a...) do {} while (0)
#define CLTR_GLERR()      do {} while (0)

#endif

#define CLTR_MARK() CLTR_DBG("mark")

typedef struct ClutterMainContext ClutterMainContext;

struct ClutterMainContext
{
  Display    *xdpy;
  Window      xwin_root;
  int         xscreen;
  GC          xgc;
  GLXContext  gl_context;
};

typedef enum CltrDirection
{
  CLTR_NORTH,
  CLTR_SOUTH,
  CLTR_EAST,
  CLTR_WEST
} 
CltrDirection;

typedef struct ClutterWindow ClutterWindow;

struct ClutterWindow
{
  Window xwin;
  int    width;
  int    height;
};

ClutterMainContext CltrCntx;

/* Event Loop Integration */

typedef void (*CltrXEventFunc) (XEvent *xev, gpointer user_data);

typedef struct 
{
  GSource  source;
  Display *display;
  GPollFD  event_poll_fd;
} 
CltrXEventSource;

#include "cltr-photo-grid.h"

#endif
