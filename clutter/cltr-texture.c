#include "cltr-texture.h"
#include "cltr-private.h"

/* 
   IDEAS or less memory

   + texture compression - made no difference ?

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
  int   qx1 = 0, qx2 = 0, qy1 = 0, qy2 = 0;
  int   qwidth = 0, qheight = 0;
  int   x, y, i =0, lastx = 0, lasty = 0;
  float tx, ty;

  qwidth  = x2-x1;
  qheight = y2-y1;

  if (texture->tiles == NULL)
    cltr_texture_realize(texture);

  if (!texture->tiled)
    {
      glBindTexture(GL_TEXTURE_2D, texture->tiles[0]);

      tx = (float) texture->pixb->width  / texture->width;
      ty = (float) texture->pixb->height / texture->height;

      qx1 = x1;
      qx2 = x2;
      
      qy1 = y1;
      qy2 = y2;
      
      glBegin (GL_QUADS);
      glTexCoord2f (tx, ty);   glVertex2i   (qx2, qy2);
      glTexCoord2f (0,  ty);   glVertex2i   (qx1, qy2);
      glTexCoord2f (0,  0);    glVertex2i   (qx1, qy1);
      glTexCoord2f (tx, 0);    glVertex2i   (qx2, qy1);
      glEnd ();	
      
      return;
    }

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

  if (!texture->tiled)
    glDeleteTextures(1, texture->tiles);
  else
    glDeleteTextures(texture->n_x_tiles * texture->n_y_tiles, texture->tiles);

  g_free(texture->tiles);

  texture->tiles = NULL;

}

void
cltr_texture_realize(CltrTexture *texture)
{
  int        x, y, i = 0;

  if (!texture->tiled)
    {
      if (!texture->tiles)
	{
	  texture->tiles = g_new (GLuint, 1);
	  glGenTextures (1, texture->tiles);
	}

      glBindTexture(GL_TEXTURE_2D, texture->tiles[0]);
	
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexEnvi      (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      
      glTexImage2D(GL_TEXTURE_2D, 0, /*GL_COMPRESSED_RGBA_ARB*/ GL_RGBA, 
		   texture->width,
		   texture->height,
		   0, GL_RGBA, 
		   GL_UNSIGNED_INT_8_8_8_8,
		   NULL);

      CLTR_GLERR();

      cltr_texture_sync_pixbuf(texture);

      return;
    }

  if (!texture->tiles)
    {
      texture->tiles = g_new (GLuint, texture->n_x_tiles * texture->n_y_tiles);
      glGenTextures (texture->n_x_tiles * texture->n_y_tiles, texture->tiles);
    }

  for (x=0; x < texture->n_x_tiles; x++)
    for (y=0; y < texture->n_y_tiles; y++)
      {
	Pixbuf *pixtmp;
	int src_h, src_w;
	
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
	
	/* Only break the pixbuf up if we have multiple tiles */
	/* if (texture->n_x_tiles > 1 && texture->n_y_tiles >1) */
	{
	  pixtmp = pixbuf_new(texture->tile_x_size[x], 
			      texture->tile_y_size[y]);
	  
	  pixbuf_copy(texture->pixb, 
		      pixtmp,
		      texture->tile_x_position[x],
		      texture->tile_y_position[y],
		      texture->tile_x_size[x], 
		      texture->tile_y_size[y],
		      0,0);
	}
	/* else pixtmp = texture->pixb; */
	
	
	glBindTexture(GL_TEXTURE_2D, texture->tiles[i]);
	
	CLTR_GLERR();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	/* glTexEnvi      (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,   GL_); */

	/* glPixelStorei (GL_UNPACK_ALIGNMENT, 4); */
	/* glPixelStorei (GL_UNPACK_ROW_LENGTH, texture->tile_x_size[x]); */

	glTexImage2D(GL_TEXTURE_2D, 0, /*GL_COMPRESSED_RGBA_ARB*/ GL_RGBA, 
		     pixtmp->width,
		     pixtmp->height,
		     0, GL_RGBA, 
		     GL_UNSIGNED_INT_8_8_8_8, 
		     /* GL_UNSIGNED_BYTE, */
		     pixtmp->data);

	CLTR_GLERR();

	CLTR_DBG("pixtmp is %ix%i texture %ix%i\n", 
		 pixtmp->width, pixtmp->height, 
		 texture->width, texture->height); 

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
  texture->tiled  = TRUE;

  /* maybe we should copy the pixbuf - a change to refed one would explode */
  texture->pixb   = pixb;
  texture->mutex  = g_mutex_new();

  pixbuf_ref(pixb);

  init_tiles (texture);

  cltr_texture_ref(texture);

  return texture;
}

void
cltr_texture_ref(CltrTexture *texture)
{
  texture->refcnt++;
}

void
cltr_texture_unref(CltrTexture *texture)
{
  texture->refcnt--;

  if (texture->refcnt <= 0)
    {
      cltr_texture_unrealize(texture);
      g_free(texture);
      pixbuf_unref(texture->pixb);
    }
}

CltrTexture*
cltr_texture_no_tile_new(Pixbuf *pixb)
{
  CltrTexture *texture;

  CLTR_MARK();

  texture = g_malloc0(sizeof(CltrTexture));
  
  texture->tiled  = FALSE;
  texture->width  = next_p2(pixb->width);
  texture->height = next_p2(pixb->height);

  if (!can_create (texture->width, texture->height))
    {
      free(texture);
      return NULL;
    }

  texture->pixb   = pixb;
  texture->mutex  = g_mutex_new();

  pixbuf_ref(pixb);
  cltr_texture_ref(texture);

  return texture;
}


Pixbuf*
cltr_texture_get_pixbuf(CltrTexture* texture)
{
  return texture->pixb;
}

void
cltr_texture_lock(CltrTexture* texture)
{
  g_mutex_lock(texture->mutex);
}

void
cltr_texture_unlock(CltrTexture* texture)
{
  g_mutex_unlock(texture->mutex);
}

void
cltr_texture_sync_pixbuf(CltrTexture* texture)
{
  if (texture->tiled)
    {
      cltr_texture_realize(texture);
    }
  else
    {
      glBindTexture(GL_TEXTURE_2D, texture->tiles[0]);

      glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0,
		       texture->pixb->width,
		       texture->pixb->height,
		       GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, 
		       texture->pixb->data);
    }
}


/*
 * This is a nasty hack to work round me not figuring out 
 * how to get RGBA data out of gstreamer in a format clutters
 * GL setup can handle :(
 *
 * The good side is it probably speeds video playback up by
 * avoiding copys of frame data. 
 */
void
cltr_texture_force_rgb_data(CltrTexture *texture,
			    int          width,
			    int          height,
			    int         *data)
{
  if (texture->tiled)
    return;

  if (!texture->tiles)
    cltr_texture_realize(texture);
    
  glBindTexture(GL_TEXTURE_2D, texture->tiles[0]);

  glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0,
		   width, 
		   height,
		   GL_RGB, GL_UNSIGNED_BYTE,
		   data);
}
