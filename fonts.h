#ifndef _HAVE_FONTS_H
#define _HAVE_FONTS_H

#include <pango/pangoft2.h>

#include "pixbuf.h"
#include "util.h"

/* Code based on stuff found in luminocity */

typedef struct ClutterFont ClutterFont;

struct ClutterFont
{
  PangoFontMap *font_map;
  PangoContext *context;
};

ClutterFont*
font_new (const char *face);


void
font_draw(ClutterFont *font, 
	  Pixbuf      *pixb, 
	  const char  *text,
	  int          x, 
	  int          y);


#endif
