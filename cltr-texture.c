#include "cltr-texture.h"

/* 
   IDEAS or less memory

   + up to 4 textures tiled per image *DONE*

   + texture compression ?

   + mipmaps - make zoom faster ? ( vs memory )

   + check max texture size *DONE*

   + how much texture mem available ?
 */



static int 
next_p2 ( int a )
{
  int rval=1;
  while(rval < a) 
    rval <<= 1;

  return rval;
}

void
cltr_texture_render_to_gl_quad(CltrTexture *texture, 
			       int          x1, 
			       int          y1, 
			       int          x2, 
			       int          y2)
{
  int   qx1, qx2, qy1, qy2;
  int   qwidth, qheight;
  int   x, y, i =0, lastx = 0, lasty = 0;
  float tx, ty;

  qwidth  = x2-x1;
  qheight = y2-y1;

  if (texture->tiles == NULL)
    cltr_texture_realize(texture);

  for (x=0; x < texture->n_x_tiles; x++)
    {
      lasty = 0;

      for (y=0; y < texture->n_y_tiles; y++)
	{
	  int actual_w, actual_h;

	  glBindTexture(GL_TEXTURE_2D, texture->tiles[i]);
	 
	  actual_w = texture->tile_x_size[x] - texture->tile_x_waste[x];
	  actual_h = texture->tile_y_size[y] - texture->tile_y_waste[y]; 

	  tx = (float) actual_w / texture->tile_x_size[x];
	  ty = (float) actual_h / texture->tile_y_size[y];
	  
	  qx1 = x1 + lastx;
	  qx2 = qx1 + ((qwidth * actual_w ) / texture->width );
	  
	  qy1 = y1 + lasty;
	  qy2 = qy1 + ((qheight * actual_h) / texture->height );

	  glBegin (GL_QUADS);
	  glTexCoord2f (tx, ty);   glVertex2i   (qx2, qy2);
	  glTexCoord2f (0,  ty);   glVertex2i   (qx1, qy2);
	  glTexCoord2f (0,  0);    glVertex2i   (qx1, qy1);
	  glTexCoord2f (tx, 0);    glVertex2i   (qx2, qy1);
	  glEnd ();	

	  lasty += qy2 - qy1;	  

	  i++;
	}
      lastx += qx2 - qx1;
    }
}
			  

/* Code below based heavily from luminocity - copyright Owen Taylor */

/*  MAX_WASTE: The maximum dimension of blank area we'll accept
 *       in a pixmap. Bigger values use less textures, smaller
 *       values less texture memory. The current value of 
 *       256 means that the smallest texture we'll split to
 *       save texture memory is 513x512. (That will be split into
 *       a 512x512 and, if overlap is 32, a 64x512 texture)
 */
#define MAX_WASTE 64

/*
 * OVERLAP: when we divide the full-resolution image into
 *          tiles to deal with hardware limitations, we overlap
 *          tiles by this much. This means that we can scale
 *          down by up to OVERLAP before we start getting
 *          seems.
 */

#define OVERLAP 0 /* 32 */

static gboolean
can_create (int width, int height)
{
  GLint new_width;

  glTexImage2D (GL_PROXY_TEXTURE_2D, 0, GL_RGBA,
                width, height, 0 /* border */,
                GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);

  glGetTexLevelParameteriv (GL_PROXY_TEXTURE_2D, 0,
                            GL_TEXTURE_WIDTH, &new_width);

  return new_width != 0;
}


static int
tile_dimension (int  to_fill,
		int  start_size,
		int *positions,
		int *sizes,
		int *waste)
		
{
  int pos     = 0;
  int n_tiles = 0;
  int size    = start_size;

  while (TRUE)
    {
      if (positions)
	positions[n_tiles] = pos;
      if (sizes)
	sizes[n_tiles] = size;
      if (waste)
	waste[n_tiles] = 0;

      n_tiles++;
	
      if (to_fill <= size)
	{
	  if (waste)
	    waste[n_tiles-1] = size - to_fill;
	  break;
	}
      else
	{

	  to_fill -= (size - OVERLAP);
	  pos += size - OVERLAP;
	  while (size >= 2 * to_fill || size - to_fill > MAX_WASTE)
	    size /= 2;
	}


    }

  return n_tiles;
}

static void
init_tiles (CltrTexture *texture)
{
  int x_pot = next_p2 (texture->width);
  int y_pot = next_p2 (texture->height);

  while (!(can_create (x_pot, y_pot) &&
	   (x_pot - texture->width < MAX_WASTE) &&
	   (y_pot - texture->height < MAX_WASTE)))
    {
      if (x_pot > y_pot)
	x_pot /= 2;
      else
	y_pot /= 2;
    }

  texture->n_x_tiles = tile_dimension (texture->width, x_pot, 
				       NULL, NULL, NULL);

  texture->tile_x_position = g_new (int, texture->n_x_tiles);
  texture->tile_x_size     = g_new (int, texture->n_x_tiles);
  texture->tile_x_waste    = g_new (int, texture->n_x_tiles);

  tile_dimension (texture->width, x_pot,
		  texture->tile_x_position, 
		  texture->tile_x_size,
		  texture->tile_x_waste);
  
  texture->n_y_tiles = tile_dimension (texture->height, y_pot, 
				       NULL, NULL, NULL);

  texture->tile_y_position = g_new (int, texture->n_y_tiles);
  texture->tile_y_size     = g_new (int, texture->n_y_tiles);
  texture->tile_y_waste    = g_new (int, texture->n_y_tiles);

  tile_dimension (texture->height, y_pot,
		  texture->tile_y_position, 
		  texture->tile_y_size,
		  texture->tile_y_waste);
  

#if 0
  /* debug info */
 {
   int i;

   g_print("n_x_tiles %i, n_y_tiles %i\n", 
	   texture->n_x_tiles, texture->n_y_tiles);

   g_print ("Tiled %d x %d texture as [", texture->width, texture->height);
   for (i = 0; i < texture->n_x_tiles; i++)
     {
       if (i != 0)
	 g_print (",");
       g_print ("%d(%d)", texture->tile_x_size[i], texture->tile_x_position[i]);
     }
   g_print ("]x[");
   for (i = 0; i < texture->n_y_tiles; i++)
     {
       if (i != 0)
	 g_print (",");
       g_print ("%d(%d)", texture->tile_y_size[i], texture->tile_y_position[i]);
     }
   g_print ("]\n");
 }
#endif

}

/* End borrowed luminocity code */

void
cltr_texture_unrealize(CltrTexture *texture)
{
  if (texture->tiles == NULL)
    return;

  glDeleteTextures(texture->n_x_tiles * texture->n_y_tiles, texture->tiles);
  g_free(texture->tiles);

  texture->tiles = NULL;
}

void
cltr_texture_realize(CltrTexture *texture)
{
  int        x, y, i = 0;

  texture->tiles = g_new (GLuint, texture->n_x_tiles * texture->n_y_tiles);
  glGenTextures (texture->n_x_tiles * texture->n_y_tiles, texture->tiles);

  for (x=0; x < texture->n_x_tiles; x++)
    for (y=0; y < texture->n_y_tiles; y++)
      {
	Pixbuf *pixtmp;
	int src_h, src_w;
	
	pixtmp = pixbuf_new(texture->tile_x_size[x], texture->tile_y_size[y]);

	src_w = texture->tile_x_size[x];
	src_h = texture->tile_y_size[y];

	/*
	CLTR_DBG("%i+%i, %ix%i to %ix%i, waste %ix%i",
		 texture->tile_x_position[x],
		 texture->tile_y_position[y],
		 texture->tile_x_size[x], 
		 texture->tile_y_size[y],
		 texture->width,
		 texture->height,
		 texture->tile_x_waste[x], 
		 texture->tile_y_waste[y]);
	*/

	pixbuf_copy(texture->pixb, 
		    pixtmp,
		    texture->tile_x_position[x],
		    texture->tile_y_position[y],
		    texture->tile_x_size[x], 
		    texture->tile_y_size[y],
		    0,0);

	glBindTexture(GL_TEXTURE_2D, texture->tiles[i]);

	CLTR_GLERR();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexEnvi      (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,   GL_DECAL);

	glTexImage2D(GL_TEXTURE_2D, 0, /*GL_COMPRESSED_RGBA_ARB*/ GL_RGBA, 
		     pixtmp->width,
		     pixtmp->height,
		     0, GL_RGBA, 
		     GL_UNSIGNED_INT_8_8_8_8,
		     pixtmp->data);

	CLTR_GLERR();

	pixbuf_unref(pixtmp);
	
	i++;

      }
}

CltrTexture*
cltr_texture_new(Pixbuf *pixb)
{
  CltrTexture *texture;


  CLTR_MARK();

  texture = g_malloc0(sizeof(CltrTexture));

  texture->width  = pixb->width;
  texture->height = pixb->height;

  /* maybe we should copy the pixbuf - a change to refed one would explode */
  texture->pixb   = pixb;

  pixbuf_ref(pixb);

  init_tiles (texture);

  return texture;
}
