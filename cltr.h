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
      g_printerr (__FILE__ ": GL Error: %i [at %s:%d]\n",      \
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


/* xxxxxxxxxxxxxPHOTO GRIDxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */

typedef struct ClutterPhotoGrid ClutterPhotoGrid;

typedef struct ClutterPhotoGridCell ClutterPhotoGridCell;

typedef enum ClutterPhotoGridState
{
  CLTR_PHOTO_GRID_STATE_BROWSE        ,
  CLTR_PHOTO_GRID_STATE_ZOOM_IN       ,
  CLTR_PHOTO_GRID_STATE_ZOOMED        ,
  CLTR_PHOTO_GRID_STATE_ZOOM_OUT      ,
  CLTR_PHOTO_GRID_STATE_ZOOMED_MOVE   ,
} 
ClutterPhotoGridState;

struct ClutterPhotoGrid
{
  /* XXX should be base widget stuff  */
  int            x,y;
  int            width;
  int            height;
  ClutterWindow *parent;

  /* ****** */

  int            n_rows;
  int            n_cols;

  int            cell_width;
  int            cell_height;

  GList         *cells_tail;
  GList         *cell_active;

  /* animation stuff  */

  int            anim_n_steps, anim_step;

  float          zoom_min, zoom_max, zoom_step;

  /* below needs better naming */
  float          view_min_x, view_max_x, view_min_y, view_max_y; 

  int            tex_w;
  int            tex_h;
  int           *tex_data;
  GLuint        *texs;

  ClutterPhotoGridState  state;
};


void
cltr_photo_grid_navigate(ClutterPhotoGrid *grid,
			 CltrDirection     direction) ;

void 				/* bleh badly named */
cltr_photo_grid_activate_cell(ClutterPhotoGrid *grid);


#endif
