#ifndef _HAVE_CLTR_H
#define _HAVE_CLTR_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include <GL/glx.h>
#include <GL/gl.h>

#include <glib.h>

#include <gst/gconf/gconf.h>

#include "pixbuf.h"
#include "fonts.h"

typedef enum CltrDirection
{
  CLTR_NORTH,
  CLTR_SOUTH,
  CLTR_EAST,
  CLTR_WEST
} 
CltrDirection;

typedef enum CltrNamedColor
{
  CLTR_COL_BG = 0,
  CLTR_COL_BDR,
  CLTR_COL_FG,
  CLTR_N_COLS
} 
CltrNamedColor;

typedef struct CltrRect
{
  int x, y, width, height;
}
CltrRect;

typedef struct CltrTexture CltrTexture;

#define cltr_rect_x1(r) ((r).x)
#define cltr_rect_y1(r) ((r).y)
#define cltr_rect_x2(r) ((r).x + (r).width)
#define cltr_rect_y2(r) ((r).y + (r).height)

typedef struct CltrWidget CltrWidget;



typedef void (*CltrCallback) (CltrWidget *widget, void *userdata) ;

typedef void (*CltrXEventCallback) (CltrWidget *widget, 
				    XEvent     *xev,
				    void       *userdata) ;


/* texture stuff */

/* ******************* */

#include "cltr-core.h"
#include "cltr-glu.h"
#include "cltr-texture.h"
#include "cltr-events.h"
#include "cltr-widget.h"
#include "cltr-animator.h"
#include "cltr-window.h"
#include "cltr-overlay.h"
#include "cltr-label.h"
#include "cltr-button.h"
#include "cltr-photo-grid.h"
#include "cltr-list.h"
#include "cltr-video.h"

#endif
