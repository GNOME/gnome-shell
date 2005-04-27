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
  int refcnt;
};

typedef ClutterFont CltrFont ; 	/* Tsk Tsk .. */

ClutterFont*
font_new (const char *face);

void
cltr_font_ref(CltrFont *font);

void
cltr_font_unref(CltrFont *font);

void
font_draw(ClutterFont *font, 
	  Pixbuf      *pixb, 
	  const char  *text,
	  int          x, 
	  int          y,
	  PixbufPixel *p);

void
font_get_pixel_size (ClutterFont *font, 
		     const char  *text,
		     int         *width,
		     int         *height);

#endif
