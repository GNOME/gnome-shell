#ifndef _HAVE_CLTR_H
#define _HAVE_CLTR_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <GL/glx.h>
#include <GL/gl.h>

#include <glib.h>

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

/* texture stuff */

/* ******************* */

#include "cltr-core.h"
#include "cltr-texture.h"
#include "cltr-events.h"
#include "cltr-widget.h"
#include "cltr-window.h"

#include "cltr-photo-grid.h"

#endif
