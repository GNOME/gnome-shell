#ifndef _HAVE_CLTR_TEX_H
#define _HAVE_CLTR_TEX_H

#include "cltr.h"

typedef struct CltrTexture CltrTexture;

struct CltrTexture
{
  Pixbuf *pixb;

  int    width, height;

  int     n_x_tiles, n_y_tiles;
  int    *tile_x_position, *tile_x_size, *tile_x_waste;
  int    *tile_y_position, *tile_y_size, *tile_y_waste;

  GLuint *tiles;

  gint    refcnt; 
};


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
