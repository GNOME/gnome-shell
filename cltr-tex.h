#ifndef _HAVE_CLTR_TEX_H
#define _HAVE_CLTR_TEX_H

#include "cltr.h"

CltrImage*
cltr_image_new(Pixbuf *pixb);

void
cltr_image_render_to_gl_quad(CltrImage *img, int x1, int y1, int x2, int y2);

#endif
