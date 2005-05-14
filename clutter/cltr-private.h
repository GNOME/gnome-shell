#ifndef _HAVE_CLTR_PRIVATE_H
#define _HAVE_CLTR_PRIVATE_H

#include "cltr.h"

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

typedef void     (*WidgetPaintMethod)   (CltrWidget *widget ) ;
typedef void     (*WidgetShowMethod)    (CltrWidget *widget ) ;
typedef void     (*WidgetDestroyMethod) (CltrWidget *widget) ;
typedef void     (*WidgetFocusMethod)   (CltrWidget *widget) ;
typedef void     (*WidgetUnfocusMethod) (CltrWidget *widget) ;
typedef gboolean (*WidgetXEventHandler) (CltrWidget *widget, XEvent *xev) ;

struct CltrWidget
{
  int         type;
  int         x,y,width,height;
  CltrWidget *parent;

  gboolean    visible;

  GList      *children;

  /* focus */

  CltrWidget *focus_next_north, *focus_next_south, 
    *focus_next_west, *focus_next_east;

  /* methods */

  WidgetPaintMethod   paint;
  WidgetShowMethod    show;
  WidgetDestroyMethod destroy;
  WidgetFocusMethod   focus_in;
  WidgetUnfocusMethod  focus_out;

  WidgetXEventHandler xevent_handler;
};

typedef struct ClutterMainContext ClutterMainContext;

struct ClutterMainContext
{
  Display        *xdpy;
  Window          xwin_root;
  int             xscreen;
  GC              xgc;

  GLXContext      gl_context;
  
  CltrWidget     *window;
  GAsyncQueue    *internal_event_q;
  
  PixbufPixel    colors[CLTR_N_COLS];
};

ClutterMainContext CltrCntx;

#define CLTR_CONTEXT() &CltrCntx

#define FPS_TO_TIMEOUT(t) (1000/(t))

#endif
