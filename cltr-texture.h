#ifndef _HAVE_CLTR_TEX_H
#define _HAVE_CLTR_TEX_H

#include "cltr.h"

CltrTexture*
cltr_texture_new(Pixbuf *pixb);

void
cltr_texture_unrealize(CltrTexture *texture);

void
cltr_texture_realize(CltrTexture *texture);

void
cltr_texture_render_to_gl_quad(CltrTexture *texture, 
			       int          x1, 
			       int          y1, 
			       int          x2, 
			       int          y2);

#endif
