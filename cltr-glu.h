#ifndef _HAVE_CLTR_GLU_H
#define _HAVE_CLTR_GLU_H

#include "cltr.h"

void
cltr_glu_set_color(PixbufPixel *p);

void 
cltr_glu_rounded_rect(int        x1, 
		      int        y1, 
		      int        x2, 
		      int        y2,
		      int        radius, 
		      PixbufPixel *col);

#endif
