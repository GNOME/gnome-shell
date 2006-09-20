/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:clutter-texture
 * @short_description: An actor for displaying and manipulating images.
 *
 * #ClutterTexture is a base class for displaying and manipulating pixel
 * buffer type data.
 *
 * The #clutter_texture_set_from_data and #clutter_texture_set_pixbuf are
 * used to copy image data into texture memory and subsequently realize the
 * the texture. Unrealizing/hiding frees image data from texture memory moving
 * to main system memory. Re-realizing then performs the opposite operation.
 * This process allows basic management of commonly limited available texture
 * memory.  
 */

#include "clutter-texture.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-feature.h"
#include "clutter-util.h"
#include "clutter-private.h" 	/* for DBG */

#include <GL/glx.h>
#include <GL/gl.h>

G_DEFINE_TYPE (ClutterTexture, clutter_texture, CLUTTER_TYPE_ACTOR);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define PIXEL_TYPE GL_UNSIGNED_BYTE
#else
#define PIXEL_TYPE GL_UNSIGNED_INT_8_8_8_8_REV
#endif

typedef struct ClutterTextureTileDimention
{
  gint pos, size, waste;
}
ClutterTextureTileDimention;

struct ClutterTexturePrivate
{
  gint                         width, height;
  GLenum                       pixel_format;
  GLenum                       pixel_type;
  GLenum                       target_type; 

  GdkPixbuf                   *local_pixbuf; /* non video memory copy */

  gboolean                     sync_actor_size;
  gint                         max_tile_waste;
  guint                        filter_quality;
  gboolean                     repeat_x, repeat_y; /* non working */

  
  gboolean                     tiled;
  ClutterTextureTileDimention *x_tiles, *y_tiles;
  gint                         n_x_tiles, n_y_tiles;
  GLuint                      *tiles;
};

enum
{
  PROP_0,
  PROP_PIXBUF,
  PROP_USE_TILES,
  PROP_MAX_TILE_WASTE,
  PROP_PIXEL_TYPE, 		/* Texture type */
  PROP_PIXEL_FORMAT,		/* Texture format */
  PROP_SYNC_SIZE,
  PROP_REPEAT_Y,
  PROP_REPEAT_X,
  PROP_FILTER_QUALITY
};

enum
{
  SIGNAL_SIZE_CHANGE,
  SIGNAL_PIXBUF_CHANGE,
  LAST_SIGNAL
};

static int texture_signals[LAST_SIGNAL] = { 0 };

static gboolean
can_create (int    width, 
	    int    height,
	    GLenum pixel_format,
	    GLenum pixel_type)
{
  GLint new_width = 0;

  CLUTTER_DBG("checking %ix%i", width, height);

  glTexImage2D (GL_PROXY_TEXTURE_2D, 0, GL_RGBA,
                width, height, 0 /* border */,
                pixel_format, pixel_type, NULL);

  CLUTTER_GLERR();

  glGetTexLevelParameteriv (GL_PROXY_TEXTURE_2D, 0,
                            GL_TEXTURE_WIDTH, &new_width);

  return new_width != 0;
}

static gboolean
can_create_rect_arb (int    width, 
		     int    height,
		     GLenum pixel_format,
		     GLenum pixel_type)
{
  /* FIXME: How to correctly query what max size of NPOTS text can be */
  if (width > 4096 || height > 4096)
    return FALSE;

  return TRUE;
}

static int
tile_dimension (int                          to_fill,
		int                          start_size,
		int                          waste,
		ClutterTextureTileDimention *tiles)
{
  int pos     = 0;
  int n_tiles = 0;
  int size    = start_size;

  while (TRUE)
    {
      if (tiles)
	{
	  tiles[n_tiles].pos = pos;
	  tiles[n_tiles].size = size;
	  tiles[n_tiles].waste = 0;
	}

      n_tiles++;
	
      if (to_fill <= size)
	{
	  if (tiles)
	    tiles[n_tiles-1].waste = size - to_fill;
	  break;
	}
      else
	{
	  to_fill -= size; pos += size;
	  while (size >= 2 * to_fill || size - to_fill > waste)
	    size /= 2;
	}
    }

  return n_tiles;
}

static void
texture_init_tiles (ClutterTexture *texture)
{
  ClutterTexturePrivate *priv;
  gint                   x_pot, y_pot;

  priv = texture->priv;

  x_pot = clutter_util_next_p2 (priv->width);
  y_pot = clutter_util_next_p2 (priv->height);

  while (!(can_create (x_pot, y_pot, priv->pixel_format, priv->pixel_type) 
	   && (x_pot - priv->width < priv->max_tile_waste) 
	   && (y_pot - priv->height < priv->max_tile_waste)))
    {
      CLUTTER_DBG("x_pot:%i - width:%i < max_waste:%i", 
		  x_pot, priv->width, priv->max_tile_waste);
      
      CLUTTER_DBG("y_pot:%i - height:%i < max_waste:%i", 
		  y_pot, priv->height, priv->max_tile_waste);
      
      if (x_pot > y_pot)
	x_pot /= 2;
      else
	y_pot /= 2;
    }
  
  if (priv->x_tiles)
    g_free(priv->x_tiles);

  priv->n_x_tiles = tile_dimension (priv->width, x_pot, 
				    priv->max_tile_waste, NULL);
  priv->x_tiles = g_new (ClutterTextureTileDimention, priv->n_x_tiles);
  tile_dimension (priv->width, x_pot, priv->max_tile_waste, priv->x_tiles);

  if (priv->y_tiles)
    g_free(priv->y_tiles);

  priv->n_y_tiles = tile_dimension (priv->height, y_pot, 
				    priv->max_tile_waste, NULL);
  priv->y_tiles = g_new (ClutterTextureTileDimention, priv->n_y_tiles);
  tile_dimension (priv->height, y_pot, priv->max_tile_waste, priv->y_tiles);

  CLUTTER_DBG("x_pot:%i, width:%i, y_pot:%i, height: %i max_waste:%i, "
              " n_x_tiles: %i, n_y_tiles: %i",
	      x_pot, priv->width, y_pot, priv->height, priv->max_tile_waste,
	      priv->n_x_tiles, priv->n_y_tiles);

}

static void
texture_render_to_gl_quad (ClutterTexture *texture, 
			   int             x1, 
			   int             y1, 
			   int             x2, 
			   int             y2)
{
  int   qx1 = 0, qx2 = 0, qy1 = 0, qy2 = 0;
  int   qwidth = 0, qheight = 0;
  int   x, y, i =0, lastx = 0, lasty = 0;
  float tx, ty;

  ClutterTexturePrivate *priv;

  priv = texture->priv;

  qwidth  = x2-x1;
  qheight = y2-y1;

  if (!CLUTTER_ACTOR_IS_REALIZED (CLUTTER_ACTOR(texture)))
      clutter_actor_realize (CLUTTER_ACTOR(texture));

  g_return_if_fail(priv->tiles != NULL);

  /* OPT: Put in display list */

  /* OPT: Optionally avoid tiling and use texture rectangles ext if
   *      supported. 
  */

  if (!priv->tiled)
    {
      glBindTexture(priv->target_type, priv->tiles[0]);

      if (priv->target_type == GL_TEXTURE_2D) /* POT */
	{
	  tx = (float) priv->width / clutter_util_next_p2 (priv->width);  
	  ty = (float) priv->height / clutter_util_next_p2 (priv->height);
	}
      else
	{
	  tx = (float) priv->width;
	  ty = (float) priv->height;

	}

      qx1 = x1; qx2 = x2;
      qy1 = y1; qy2 = y2;
      
      glBegin (GL_QUADS);
      glTexCoord2f (tx, ty);   glVertex2i   (qx2, qy2);
      glTexCoord2f (0,  ty);   glVertex2i   (qx1, qy2);
      glTexCoord2f (0,  0);    glVertex2i   (qx1, qy1);
      glTexCoord2f (tx, 0);    glVertex2i   (qx2, qy1);
      glEnd ();	
      
      return;
    }

  for (x=0; x < priv->n_x_tiles; x++)
    {
      lasty = 0;

      for (y=0; y < priv->n_y_tiles; y++)
	{
	  int actual_w, actual_h;

	  glBindTexture(priv->target_type, priv->tiles[i]);
	 
	  actual_w = priv->x_tiles[x].size - priv->x_tiles[x].waste;
	  actual_h = priv->y_tiles[y].size - priv->y_tiles[y].waste;

	  CLUTTER_DBG("rendering text tile x: %i, y: %i - %ix%i", 
		      x, y, actual_w, actual_h);

	  tx = (float) actual_w / priv->x_tiles[x].size;
	  ty = (float) actual_h / priv->y_tiles[y].size;

	  qx1 = x1 + lastx;
	  qx2 = qx1 + ((qwidth * actual_w ) / priv->width );
	  
	  qy1 = y1 + lasty;
	  qy2 = qy1 + ((qheight * actual_h) / priv->height );

	  glBegin (GL_QUADS);
	  glTexCoord2f (tx, ty);   glVertex2i   (qx2, qy2);
	  glTexCoord2f (0,  ty);   glVertex2i   (qx1, qy2);
	  glTexCoord2f (0,  0);    glVertex2i   (qx1, qy1);
	  glTexCoord2f (tx, 0);    glVertex2i   (qx2, qy1);
	  glEnd ();	

	  lasty += (qy2 - qy1) ;	  

	  i++;
	}
      lastx += (qx2 - qx1);
    }
}

static void
texture_free_gl_resources (ClutterTexture *texture)
{
  ClutterTexturePrivate *priv;

  priv = texture->priv;

  CLUTTER_MARK();

  if (priv->tiles)
    {
      if (!priv->tiled)
	glDeleteTextures(1, priv->tiles);
      else
	glDeleteTextures(priv->n_x_tiles * priv->n_y_tiles, priv->tiles);

      g_free(priv->tiles);
      priv->tiles = NULL;
    }

  if (priv->x_tiles)
    {
      g_free(priv->x_tiles);
      priv->x_tiles = NULL;
    }

  if (priv->y_tiles)
    {
      g_free(priv->y_tiles);
      priv->y_tiles = NULL;
    }
}

static void
texture_upload_data (ClutterTexture *texture,
		     const guchar   *data,
		     gboolean        has_alpha,
		     gint            width,
		     gint            height,
		     gint            rowstride,
		     gint            bpp)
{
  ClutterTexturePrivate *priv;
  int                    x, y, i = 0;
  gboolean               create_textures = FALSE;

  priv = texture->priv;

  g_return_if_fail (data != NULL);

  CLUTTER_MARK();

  if (!priv->tiled)
    {
      /* Single Texture */
      if (!priv->tiles)
	{
	  priv->tiles = g_new (GLuint, 1);
	  glGenTextures (1, priv->tiles);
	  create_textures = TRUE;
	}

      CLUTTER_DBG("syncing for single tile");

      glBindTexture(priv->target_type, priv->tiles[0]);

      glTexParameteri(priv->target_type, 
		      GL_TEXTURE_WRAP_S, 
		      priv->repeat_x ? GL_REPEAT : GL_CLAMP_TO_EDGE);

      glTexParameteri(priv->target_type, 
		      GL_TEXTURE_WRAP_T, 
		      priv->repeat_y ? GL_REPEAT : GL_CLAMP_TO_EDGE);

      priv->filter_quality = 1;

      glTexParameteri(priv->target_type, GL_TEXTURE_MAG_FILTER, 
		      priv->filter_quality ? GL_LINEAR : GL_NEAREST);

      glTexParameteri(priv->target_type, GL_TEXTURE_MIN_FILTER, 
		      priv->filter_quality ? GL_LINEAR : GL_NEAREST);

      glPixelStorei (GL_UNPACK_ROW_LENGTH, priv->width);
      glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
	  
      if (create_textures)
	{
	  gint width, height;

	  width  = priv->width;
	  height = priv->height;

	  if (priv->target_type == GL_TEXTURE_2D) /* POT */
	    {
	      width  = clutter_util_next_p2(priv->width);
	      height = clutter_util_next_p2(priv->height);
	    }

	  glTexImage2D(priv->target_type, 
		       0, 
		       /* (has_alpha) ? GL_RGBA : GL_RGB, */
		       GL_RGBA,
		       width,
		       height,
		       0, 
		       priv->pixel_format, 
		       priv->pixel_type, 
		       NULL);
	}
      
      glTexSubImage2D (priv->target_type, 0, 0, 0,
		       priv->width,
		       priv->height,
		       priv->pixel_format, 
		       priv->pixel_type, 
		       data);
      return;
    }

  /* Multiple tiled texture */
  
  CLUTTER_DBG("syncing for multiple tiles for %ix%i pixbuf",
	      priv->width, priv->height);

  g_return_if_fail (priv->x_tiles != NULL && priv->y_tiles != NULL);
  
  if (priv->tiles == NULL)
    {
      priv->tiles = g_new (GLuint, priv->n_x_tiles * priv->n_y_tiles);
      glGenTextures (priv->n_x_tiles * priv->n_y_tiles, priv->tiles);
      create_textures = TRUE;
    }

  for (x=0; x < priv->n_x_tiles; x++)
    for (y=0; y < priv->n_y_tiles; y++)
      {
	guchar    *tmp;
	gint        src_h, src_w, dy;
	
	src_w = priv->x_tiles[x].size;
	src_h = priv->y_tiles[y].size;
	
	/* fixme - gslice ? */
	tmp = g_malloc (sizeof(guchar) * priv->x_tiles[x].size 
			  * priv->y_tiles[y].size * bpp);

	/* clip */
	if (priv->x_tiles[x].pos + src_w > priv->width)
	  {
	    src_w = priv->width - priv->x_tiles[x].pos;
	  }

	if (priv->y_tiles[y].pos + src_h > priv->height)
	  {
	    src_h = priv->height - priv->y_tiles[y].pos;
	  }

	CLUTTER_DBG("copying tile %i,%i - %ix%i to 0,0 %ix%i", 
		    priv->x_tiles[x].pos,
		    priv->y_tiles[y].pos,
		    src_w,
		    src_h,
		    priv->x_tiles[x].size,
		    priv->y_tiles[y].size);

	for (dy = 0; dy < src_h; dy++)
	  {
	    memcpy (tmp + (dy * src_w * bpp),
		    data + ((priv->y_tiles[y].pos + dy) * rowstride)
		         + (priv->x_tiles[x].pos * bpp),
		    (src_w * bpp));
	  }

#ifdef CLUTTER_DUMP_TILES
	{
	  gchar  *filename;

	  filename = g_strdup_printf("/tmp/%i-%i-%i.png",
				     clutter_actor_get_id(CLUTTER_ACTOR(texture)), 
				     x, y);
	  printf("saving %s\n", filename);
	  gdk_pixbuf_save (pixtmp, filename , "png", NULL, NULL);
	}
#endif

	glBindTexture(priv->target_type, priv->tiles[i]);

	glTexParameteri(priv->target_type, 
			GL_TEXTURE_WRAP_S, 
			priv->repeat_x ? GL_REPEAT : GL_CLAMP_TO_EDGE);

	glTexParameteri(priv->target_type, 
			GL_TEXTURE_WRAP_T, 
			priv->repeat_y ? GL_REPEAT : GL_CLAMP_TO_EDGE);

	glTexParameteri(priv->target_type, GL_TEXTURE_MAG_FILTER, 
			priv->filter_quality ? GL_LINEAR : GL_NEAREST);

	glTexParameteri(priv->target_type, GL_TEXTURE_MIN_FILTER, 
			priv->filter_quality ? GL_LINEAR : GL_NEAREST);

	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glPixelStorei (GL_UNPACK_ROW_LENGTH, priv->width);
	glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
	
	if (create_textures)
	  {

	    glTexImage2D(priv->target_type, 
			 0, 
			 (bpp == 4) ? 
			 GL_RGBA : GL_RGB,
			 priv->x_tiles[x].size,
			 priv->y_tiles[y].size,
			 0, 
			 priv->pixel_format, 
			 priv->pixel_type, 
			 tmp);
	  }
	else 
	  {
	    /* Textures already created, so just update whats inside 
	    */
	    glTexSubImage2D (priv->target_type, 0, 
			     0, 0,
			     priv->x_tiles[x].size,
			     priv->y_tiles[y].size,
			     priv->pixel_format, 
			     priv->pixel_type, 
			     tmp);
	  }

	g_free(tmp);

	i++;
      }
}

static void
clutter_texture_unrealize (ClutterActor *actor)
{
  ClutterTexture        *texture;
  ClutterTexturePrivate *priv;

  texture = CLUTTER_TEXTURE(actor);
  priv = texture->priv;

  if (priv->tiles == NULL)
    return;

  CLUTTER_MARK();

  /* Move image data from video to main memory 
  */
  if (priv->local_pixbuf == NULL)
    priv->local_pixbuf = clutter_texture_get_pixbuf (texture);
  
  texture_free_gl_resources (texture);

  CLUTTER_DBG("Texture unrealized");
}

static void
clutter_texture_realize (ClutterActor *actor)
{
  ClutterTexture       *texture;
  ClutterTexturePrivate *priv;

  texture = CLUTTER_TEXTURE(actor);
  priv = texture->priv;

  CLUTTER_MARK();

  if (priv->local_pixbuf != NULL)
    {
      /* Move any local image data we have from unrealization  
       * back into video memory.	 
      */
      clutter_texture_set_pixbuf (texture, priv->local_pixbuf);
      g_object_unref (priv->local_pixbuf);
      priv->local_pixbuf = NULL;
    }
  else
    {
      /* Dont allow realization with no pixbuf - note set_pixbuf/data 
       * will set realize flags.	 
      */
      CLUTTER_DBG("*** Texture has no image data cannot realize ***");
      CLUTTER_DBG("*** flags %i ***", actor->flags);
      CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
      CLUTTER_DBG("*** flags %i ***", actor->flags);
      return;
    }

  CLUTTER_DBG("Texture realized");
}

static void
clutter_texture_show (ClutterActor *self)
{
  clutter_actor_realize (self);
}

static void
clutter_texture_hide (ClutterActor *self)
{
  clutter_actor_unrealize (self);
}

static void
clutter_texture_paint (ClutterActor *self)
{
  ClutterTexture *texture = CLUTTER_TEXTURE(self);
  gint            x1, y1, x2, y2;
  guint8          opacity;

  CLUTTER_DBG("@@@ for '%s' @@@", 
	      clutter_actor_get_name(self) ? 
                          clutter_actor_get_name(self) : "unknown");
  glPushMatrix();

  glEnable(GL_BLEND);
  glEnable(texture->priv->target_type);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 

  opacity = clutter_actor_get_opacity(self);

  CLUTTER_DBG("setting opacity to %i\n", opacity);
  glColor4ub(255, 255, 255, opacity);

  clutter_actor_get_coords (self, &x1, &y1, &x2, &y2);
  /* Paint will of translated us */
  texture_render_to_gl_quad (texture, 0, 0, x2 - x1, y2 - y1);

  glDisable(texture->priv->target_type);
  glDisable(GL_BLEND);

  glPopMatrix();
}

static void 
clutter_texture_dispose (GObject *object)
{
  ClutterTexture *texture = CLUTTER_TEXTURE(object);
  ClutterTexturePrivate *priv;

  priv = texture->priv;

  if (priv != NULL)
    {
      texture_free_gl_resources (texture);

      if (priv->local_pixbuf != NULL)
	{
	  g_object_unref (priv->local_pixbuf);
	  priv->local_pixbuf = NULL;
	}
    }

  G_OBJECT_CLASS (clutter_texture_parent_class)->dispose (object);
}

static void 
clutter_texture_finalize (GObject *object)
{
  ClutterTexture *self = CLUTTER_TEXTURE(object);

  if (self->priv)
    {
      g_free(self->priv);
      self->priv = NULL;
    }

  G_OBJECT_CLASS (clutter_texture_parent_class)->finalize (object);
}

static void
clutter_texture_set_property (GObject      *object, 
			      guint         prop_id,
			      const GValue *value, 
			      GParamSpec   *pspec)
{
  ClutterTexture        *texture;
  ClutterTexturePrivate *priv;

  texture = CLUTTER_TEXTURE(object);
  priv = texture->priv;

  switch (prop_id) 
    {
    case PROP_PIXBUF:
      clutter_texture_set_pixbuf (texture, 
				  (GdkPixbuf*)g_value_get_pointer(value));
      break;
    case PROP_USE_TILES:
      priv->tiled = g_value_get_boolean (value);

      if (priv->target_type == GL_TEXTURE_RECTANGLE_ARB
	  && priv->tiled == TRUE)
	priv->target_type  = GL_TEXTURE_2D;

      CLUTTER_DBG("Texture is tiled ? %i", priv->tiled);
      break;
    case PROP_MAX_TILE_WASTE:
      priv->max_tile_waste = g_value_get_int (value);
      break;
    case PROP_PIXEL_TYPE:
      priv->pixel_type = g_value_get_int (value);
      break;
    case PROP_PIXEL_FORMAT:
      priv->pixel_format = g_value_get_int (value);
      break;
    case PROP_SYNC_SIZE:
      priv->sync_actor_size = g_value_get_boolean (value);
      break;
    case PROP_REPEAT_X:
      priv->repeat_x = g_value_get_boolean (value);
      break;
    case PROP_REPEAT_Y:
      priv->repeat_y = g_value_get_boolean (value);
      break;
    case PROP_FILTER_QUALITY:
      priv->filter_quality = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_texture_get_property (GObject    *object, 
			      guint       prop_id,
			      GValue     *value, 
			      GParamSpec *pspec)
{
  ClutterTexture        *texture;
  ClutterTexturePrivate *priv;

  texture = CLUTTER_TEXTURE(object);
  priv = texture->priv;

  switch (prop_id) 
    {
    case PROP_PIXBUF:
      {
	GdkPixbuf *pixb;
	pixb = clutter_texture_get_pixbuf (texture);
	g_value_set_pointer (value, pixb);
      }
      break;
    case PROP_USE_TILES:
      g_value_set_boolean (value, priv->tiled);
      break;
    case PROP_MAX_TILE_WASTE:
      g_value_set_int (value, priv->max_tile_waste);
      break;
    case PROP_PIXEL_TYPE:
      g_value_set_int (value, priv->pixel_type);
      break;
    case PROP_PIXEL_FORMAT:
      g_value_set_int (value, priv->pixel_format);
      break;
    case PROP_SYNC_SIZE:
      g_value_set_boolean (value, priv->sync_actor_size);
      break;
    case PROP_REPEAT_X:
      g_value_set_boolean (value, priv->repeat_x);
      break;
    case PROP_REPEAT_Y:
      g_value_set_boolean (value, priv->repeat_y);
      break;
    case PROP_FILTER_QUALITY:
      g_value_set_int (value, priv->filter_quality);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    } 
}


static void
clutter_texture_class_init (ClutterTextureClass *klass)
{
  GObjectClass        *gobject_class;
  ClutterActorClass *actor_class;

  gobject_class = (GObjectClass*)klass;
  actor_class = (ClutterActorClass*)klass;

  actor_class->paint      = clutter_texture_paint;
  actor_class->realize    = clutter_texture_realize;
  actor_class->unrealize  = clutter_texture_unrealize;
  actor_class->show       = clutter_texture_show;
  actor_class->hide       = clutter_texture_hide;

  gobject_class->dispose      = clutter_texture_dispose;
  gobject_class->finalize     = clutter_texture_finalize;
  gobject_class->set_property = clutter_texture_set_property;
  gobject_class->get_property = clutter_texture_get_property;

  g_object_class_install_property
    (gobject_class, PROP_PIXBUF,
     g_param_spec_pointer ("pixbuf",
			   "Pixbuf source for Texture.",
			   "Pixbuf source for Texture.",
			   G_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_USE_TILES,
     g_param_spec_boolean ("tiled",
			   "Enable use of tiled textures",
			   "Enables the use of tiled GL textures to more "
			   "efficiently use available texture memory",
			   /* FIXME: This default set at runtime :/  
                            * As tiling depends on what GL features available.
                            * Need to figure out better solution
			   */
			   (clutter_feature_available 
			     (CLUTTER_FEATURE_TEXTURE_RECTANGLE) == FALSE),
			   G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_SYNC_SIZE,
     g_param_spec_boolean ("sync-size",
			   "Sync size of actor",
			   "Auto sync size of actor to underlying pixbuf"
			   "dimentions",
			   TRUE,
			   G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_REPEAT_X,
     g_param_spec_boolean ("repeat-x",
			   "Tile underlying pixbuf in x direction",
			   "Reapeat underlying pixbuf rather than scale" 
			   "in x direction. Currently UNWORKING",
			   FALSE,
			   G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_REPEAT_Y,
     g_param_spec_boolean ("repeat-y",
			   "Tile underlying pixbuf in y direction",
			   "Reapeat underlying pixbuf rather than scale" 
			   "in y direction. Currently UNWORKING",
			   FALSE,
			   G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  /* FIXME: Ideally this option needs to have some kind of global
   *        overide as to imporve performance.
  */
  g_object_class_install_property
    (gobject_class, PROP_FILTER_QUALITY,
     g_param_spec_int ("filter-quality",
		       "Quality of filter used when scaling a texture",
		       "Values 0 and 1 current only supported, with 0"
		       "being lower quality but fast, 1 being better "
		       "quality but slower. ( Currently just maps to "
		       " GL_NEAREST / GL_LINEAR )",
		       0,
		       G_MAXINT,
		       1,
		       G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_MAX_TILE_WASTE,
     g_param_spec_int ("tile-waste",
		       "Tile dimention to waste",
		       "Max wastage dimention of a texture when using "
		       "tiled textures. Bigger values use less textures, "
		       "smaller values less texture memory. ",
		       0,
		       G_MAXINT,
		       64,
		       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_PIXEL_TYPE,
     g_param_spec_int ("pixel-type",
		       "Texture Pixel Type",
		       "GL texture pixel type used",
		       0,
		       G_MAXINT,
		       PIXEL_TYPE,
		       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_PIXEL_FORMAT,
     g_param_spec_int ("pixel-format",
		       "Texture pixel format",
		       "GL texture pixel format used",
		       0,
		       G_MAXINT,
		       GL_RGBA,
		       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  texture_signals[SIGNAL_SIZE_CHANGE] =
    g_signal_new ("size-change",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterTextureClass, size_change),
		  NULL, NULL,
		  clutter_marshal_VOID__INT_INT,
		  G_TYPE_NONE, 
		  2, G_TYPE_INT, G_TYPE_INT);

  texture_signals[SIGNAL_PIXBUF_CHANGE] =
    g_signal_new ("pixbuf-change",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterTextureClass, pixbuf_change),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 
		  0); 
}

static void
clutter_texture_init (ClutterTexture *self)
{
  ClutterTexturePrivate *priv;

  priv = g_new0 (ClutterTexturePrivate, 1);

  priv->max_tile_waste = 64;
  priv->filter_quality = 0;
  priv->tiled        = TRUE;
  priv->pixel_type   = PIXEL_TYPE;
  priv->pixel_format = GL_RGBA;
  priv->repeat_x     = FALSE;
  priv->repeat_y     = FALSE;

  if (clutter_feature_available (CLUTTER_FEATURE_TEXTURE_RECTANGLE))
    {
      priv->target_type  = GL_TEXTURE_RECTANGLE_ARB;
      priv->tiled        = FALSE;
    }
  else
    priv->target_type = GL_TEXTURE_2D;

  self->priv  = priv;
}

static void
pixbuf_destroy_notify (guchar  *pixels, gpointer data)
{
  g_free (pixels);
}

/**
 * clutter_texture_get_pixbuf:
 * @texture: A #ClutterTexture
 *
 * Gets a #GdkPixbuf representation of the #ClutterTexture data. 
 * The created #GdkPixbuf is not owned by the texture but the caller.  
 *
 * Return value: A #GdkPixbuf
 **/
GdkPixbuf*
clutter_texture_get_pixbuf (ClutterTexture* texture)
{
  ClutterTexturePrivate *priv;
  GdkPixbuf             *pixbuf = NULL;
  guchar                *pixels = NULL;

  priv = texture->priv;

  if (priv->tiles == NULL)
    return NULL; 

  if (!priv->tiled)
    {
      pixels = g_malloc (priv->width * priv->height * 4);
      
      if (pixels == NULL)
	return NULL;

      glBindTexture(priv->target_type, priv->tiles[0]);

      glPixelStorei (GL_UNPACK_ROW_LENGTH, priv->width);
      glPixelStorei (GL_UNPACK_ALIGNMENT, 4);

      /* read data from gl text and return as pixbuf */
      glGetTexImage (priv->target_type,
		     0,
		     priv->pixel_format,
		     priv->pixel_type,
		     (GLvoid*)pixels);

      pixbuf = gdk_pixbuf_new_from_data ((const guchar*)pixels,
					 GDK_COLORSPACE_RGB,
					 (priv->pixel_format == GL_RGBA),
					 8,
					 priv->width,
					 priv->height,
					 priv->width * 4,
					 pixbuf_destroy_notify,
					 NULL);
    }
  else
    {
      int x,y,i;

      i = 0;

      pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
			       (priv->pixel_format == GL_RGBA),
			       8,
			       priv->width,
			       priv->height);

      for (x=0; x < priv->n_x_tiles; x++)
	for (y=0; y < priv->n_y_tiles; y++)
	  {
	    GdkPixbuf  *tmp_pixb;
	    gint        src_h, src_w;
	
	    src_w = priv->x_tiles[x].size;
	    src_h = priv->y_tiles[y].size;

	    pixels = g_malloc (src_w * src_h *4);

	    glBindTexture(priv->target_type, priv->tiles[i]);
	    
	    glPixelStorei (GL_UNPACK_ROW_LENGTH, src_w);
	    glPixelStorei (GL_UNPACK_ALIGNMENT, 4);

	    glGetTexImage (priv->target_type,
			   0,
			   priv->pixel_format,
			   priv->pixel_type,
			   (GLvoid*)pixels);
	
	    tmp_pixb 
	      = gdk_pixbuf_new_from_data ((const guchar*)pixels,
					  GDK_COLORSPACE_RGB,
					  (priv->pixel_format == GL_RGBA),
					  8,
					  src_w,
					  src_h,
					  src_w * 4,
					  pixbuf_destroy_notify,
					  NULL);
	
	    gdk_pixbuf_copy_area (tmp_pixb,
				  0,
				  0,
				  src_w,
				  src_h,
				  pixbuf,
				  priv->x_tiles[x].pos,
				  priv->x_tiles[y].pos);

	    g_object_unref (tmp_pixb);

	    i++;
	  }
      
    }

  return pixbuf;
}

/**
 * clutter_texture_set_from_data:
 * @texture: A #ClutterTexture
 * @data: Image data in RGB type colorspace.
 * @has_alpha: Set to TRUE if image data has a alpha channel.
 * @width: Width in pixels of image data.
 * @height: Height in pixels of image data
 * @rowstride: Distance in bytes between row starts.
 * @bpp: bytes per pixel ( Currently only 4 supported )
 *
 * Sets #ClutterTexture image data.
 *
 * Since 0.2. This function is likely to change in future versions.
 **/
void
clutter_texture_set_from_data (ClutterTexture *texture,
			       const guchar   *data,
			       gboolean        has_alpha,
			       gint            width,
			       gint            height,
			       gint            rowstride,
			       gint            bpp)
{
  ClutterTexturePrivate *priv;
  gboolean               texture_dirty = TRUE;

  priv = texture->priv;

  g_return_if_fail (data != NULL);
  g_return_if_fail (bpp == 4);

  /* FIXME: check other image props */
  texture_dirty = (width != priv->width || height != priv->height);

  priv->width  = width;
  priv->height = height;

  if (has_alpha)
    priv->pixel_format = GL_RGBA;
  else
    priv->pixel_format = GL_RGB;

  if (texture_dirty)
    {
      texture_free_gl_resources (texture);

      if (priv->tiled == FALSE)
	{
	  if (priv->target_type == GL_TEXTURE_RECTANGLE_ARB
	      && !can_create_rect_arb (priv->width, 
				       priv->height,
				       priv->pixel_format,
				       priv->pixel_type))
	    {
	      /* If we cant create NPOT tex of this size fall back to tiles */
	      priv->tiled = TRUE; 
	      priv->target_type = GL_TEXTURE_2D;
	    }
	  else if (priv->target_type == GL_TEXTURE_2D
		   && !can_create(clutter_util_next_p2(priv->width), 
				  clutter_util_next_p2(priv->height),
				  priv->pixel_format, 
				  priv->pixel_type))
	    { 
	      priv->tiled = TRUE; 
	    }
	}

      /* Figure our tiling etc */
      if (priv->tiled)
	texture_init_tiles (texture); 
    }

  CLUTTER_DBG("set size %ix%i\n", priv->width, priv->height);

  texture_upload_data (texture, data, has_alpha, 
		       width, height, rowstride, bpp);

  CLUTTER_ACTOR_SET_FLAGS (CLUTTER_ACTOR(texture), CLUTTER_ACTOR_REALIZED);

  if (texture_dirty)
    {
      g_signal_emit (texture, texture_signals[SIGNAL_SIZE_CHANGE], 
		     0, priv->width, priv->height);

      if (priv->sync_actor_size)
	clutter_actor_set_size (CLUTTER_ACTOR(texture), 
				priv->width, 
				priv->height);
    }
  
  /* rename signal */
  g_signal_emit (texture, texture_signals[SIGNAL_PIXBUF_CHANGE], 0); 

  /* If resized actor may need resizing but paint() will do this */
  if (CLUTTER_ACTOR_IS_MAPPED (CLUTTER_ACTOR(texture)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR(texture));

}

/**
 * clutter_texture_set_pixbuf:
 * @texture: A #ClutterTexture
 * @pixbuf: A #GdkPixbuf
 *
 * Sets a  #ClutterTexture image data from a #GdkPixbuf
 *
 **/
void
clutter_texture_set_pixbuf (ClutterTexture *texture, GdkPixbuf *pixbuf)
{
  ClutterTexturePrivate *priv;

  priv = texture->priv;

  g_return_if_fail (pixbuf != NULL);

  clutter_texture_set_from_data (texture,
				 gdk_pixbuf_get_pixels (pixbuf),
				 gdk_pixbuf_get_has_alpha (pixbuf),
				 gdk_pixbuf_get_width (pixbuf),
				 gdk_pixbuf_get_height (pixbuf),
				 gdk_pixbuf_get_rowstride (pixbuf),
				 4);
}

/**
 * clutter_texture_new_from_pixbuf:
 * @pixbuf: A #GdkPixbuf
 *
 * Creates a new #ClutterTexture object.
 *
 * Return value: A newly created #ClutterTexture object.
 **/
ClutterActor*
clutter_texture_new_from_pixbuf (GdkPixbuf *pixbuf)
{
  ClutterTexture *texture;

  texture = g_object_new (CLUTTER_TYPE_TEXTURE, "pixbuf", pixbuf, NULL);

  return CLUTTER_ACTOR(texture);
}

/**
 * clutter_texture_new:
 *
 * Creates a new empty #ClutterTexture object.
 *
 * Return value: A newly created #ClutterTexture object.
 **/
ClutterActor*
clutter_texture_new (void)
{
  ClutterTexture *texture;

  texture = g_object_new (CLUTTER_TYPE_TEXTURE, NULL);

  return CLUTTER_ACTOR(texture);
}

/**
 * clutter_texture_get_base_size:
 * @texture: A #ClutterTexture
 * @width:   Pointer to gint to be populated with width value if non NULL.
 * @height:  Pointer to gint to be populated with height value if non NULL.
 *
 * Gets the size in pixels of the untransformed underlying texture pixbuf data.
 *
 **/
void 				/* FIXME: rename to get_image_size */
clutter_texture_get_base_size (ClutterTexture *texture, 
			       gint           *width,
			       gint           *height)
{
  /* Attempt to realize, mainly for subclasses ( such as labels )
   * which maynot create pixbuf data and thus base size until
   * realization happens.
  */
  if (!CLUTTER_ACTOR_IS_REALIZED(CLUTTER_ACTOR(texture)))
    clutter_actor_realize (CLUTTER_ACTOR(texture));

  if (width)
    *width = texture->priv->width;

  if (height)
    *height = texture->priv->height;

}

/**
 * clutter_texture_bind_tile:
 * @texture: A #ClutterTexture
 * @index: Tile index to bind
 *
 * Proxys a call to glBindTexture a to bind an internal 'tile'. 
 *
 * This function is only useful for sub class implementations 
 * and never should be called by an application.
 **/
void
clutter_texture_bind_tile (ClutterTexture *texture, gint index)
{
  ClutterTexturePrivate *priv;

  priv = texture->priv;
  glBindTexture(priv->target_type, priv->tiles[index]);
}

/**
 * clutter_texture_get_n_tiles:
 * @texture: A #ClutterTexture
 * @n_x_tiles: Location to store number of tiles in horizonally axis
 * @n_y_tiles: Location to store number of tiles in vertical axis
 *
 * Retreives internal tile dimentioning.
 *
 * This function is only useful for sub class implementations 
 * and never should be called by an application.
 **/
void
clutter_texture_get_n_tiles (ClutterTexture *texture, 
			     gint           *n_x_tiles,
			     gint           *n_y_tiles)
{
  if (n_x_tiles)
    *n_x_tiles = texture->priv->n_x_tiles;

  if (n_y_tiles)
    *n_y_tiles = texture->priv->n_y_tiles;

}

/**
 * clutter_texture_get_x_tile_detail:
 * @texture: A #ClutterTexture
 * @x_index: X index of tile to query
 * @pos: Location to store tiles X position
 * @size: Location to store tiles horizontal size in pixels 
 * @waste: Location to store tiles horizontal wastage in pixels
 *
 * Retreives details of a tile on x axis.
 *
 * This function is only useful for sub class implementations 
 * and never should be called by an application.
 **/
void
clutter_texture_get_x_tile_detail (ClutterTexture *texture, 
				   gint            x_index,
				   gint           *pos,
				   gint           *size,
				   gint           *waste)
{
  g_return_if_fail(x_index < texture->priv->n_x_tiles);

  if (pos)
    *pos = texture->priv->x_tiles[x_index].pos;

  if (size)
    *size = texture->priv->x_tiles[x_index].size;

  if (waste)
    *waste = texture->priv->x_tiles[x_index].waste;
}

/**
 * clutter_texture_get_y_tile_detail:
 * @texture: A #ClutterTexture
 * @y_index: Y index of tile to query
 * @pos: Location to store tiles Y position
 * @size: Location to store tiles vertical size in pixels 
 * @waste: Location to store tiles vertical wastage in pixels
 *
 * Retreives details of a tile on y axis.
 *
 * This function is only useful for sub class implementations 
 * and never should be called by an application.
 **/
void
clutter_texture_get_y_tile_detail (ClutterTexture *texture, 
				   gint            y_index,
				   gint           *pos,
				   gint           *size,
				   gint           *waste)
{
  g_return_if_fail(y_index < texture->priv->n_y_tiles);

  if (pos)
    *pos = texture->priv->y_tiles[y_index].pos;

  if (size)
    *size = texture->priv->y_tiles[y_index].size;

  if (waste)
    *waste = texture->priv->y_tiles[y_index].waste;
}

/**
 * clutter_texture_has_generated_tiles:
 * @texture: A #ClutterTexture
 *
 * Checks if #ClutterTexture has generated underlying GL texture tiles.
 *
 * This function is only useful for sub class implementations 
 * and never should be called by an application.
 *
 * Return value: TRUE if texture has pregenerated GL tiles.
 **/
gboolean
clutter_texture_has_generated_tiles (ClutterTexture *texture)
{
  return (texture->priv->tiles != NULL);
}

/**
 * clutter_texture_is_tiled:
 * @texture: A #ClutterTexture
 *
 * Checks if #ClutterTexture is tiled.
 *
 * This function is only useful for sub class implementations 
 * and never should be called by an application.
 *
 * Return value: TRUE if texture is tiled
 **/
gboolean
clutter_texture_is_tiled (ClutterTexture *texture)
{
  return texture->priv->tiled;
}
