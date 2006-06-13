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

#include "clutter-clone-texture.h"
#include "clutter-main.h"
#include "clutter-util.h" 
#include "clutter-enum-types.h"
#include "clutter-private.h" 	/* for DBG */

enum
{
  PROP_0,
  PROP_PARENT_TEXTURE
};

G_DEFINE_TYPE (ClutterCloneTexture,
	       clutter_clone_texture,
	       CLUTTER_TYPE_ACTOR);

#define CLUTTER_CLONE_TEXTURE_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_CLONE_TEXTURE, ClutterCloneTexturePrivate))

struct _ClutterCloneTexturePrivate
{
  ClutterTexture      *parent_texture;
};

static void
clone_texture_render_to_gl_quad (ClutterCloneTexture *ctexture, 
				 int             x1, 
				 int             y1, 
				 int             x2, 
				 int             y2)
{
  gint   qx1 = 0, qx2 = 0, qy1 = 0, qy2 = 0;
  gint   qwidth = 0, qheight = 0;
  gint   x, y, i =0, lastx = 0, lasty = 0;
  gint   n_x_tiles, n_y_tiles; 
  gint   pwidth, pheight;
  float tx, ty;

  ClutterCloneTexturePrivate *priv = ctexture->priv;
  ClutterActor *parent_actor = CLUTTER_ACTOR (priv->parent_texture);

  priv = ctexture->priv;

  qwidth  = x2 - x1;
  qheight = y2 - y1;

  if (!CLUTTER_ACTOR_IS_REALIZED (parent_actor))
      clutter_actor_realize (parent_actor);

  /* Only paint if parent is in a state to do so */
  if (!clutter_texture_has_generated_tiles (priv->parent_texture))
    return;
  
  clutter_texture_get_base_size (priv->parent_texture, &pwidth, &pheight); 

  if (!clutter_texture_is_tiled (priv->parent_texture))
    {
      clutter_texture_bind_tile (priv->parent_texture, 0);

      tx = (float) pwidth / clutter_util_next_p2 (pwidth);  
      ty = (float) pheight / clutter_util_next_p2 (pheight);

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

  clutter_texture_get_n_tiles (priv->parent_texture, &n_x_tiles, &n_y_tiles); 

  for (x=0; x < n_x_tiles; x++)
    {
      lasty = 0;

      for (y=0; y < n_y_tiles; y++)
	{
	  gint actual_w, actual_h;
	  gint xpos, ypos, xsize, ysize, ywaste, xwaste;
	  
	  clutter_texture_bind_tile (priv->parent_texture, i);
	 
	  clutter_texture_get_x_tile_detail (priv->parent_texture, 
					     x, &xpos, &xsize, &xwaste);

	  clutter_texture_get_y_tile_detail (priv->parent_texture, 
					     y, &ypos, &ysize, &ywaste);

	  actual_w = xsize - xwaste;
	  actual_h = ysize - ywaste;

	  tx = (float) actual_w / xsize;
	  ty = (float) actual_h / ysize;

	  qx1 = x1 + lastx;
	  qx2 = qx1 + ((qwidth * actual_w ) / pwidth );
	  
	  qy1 = y1 + lasty;
	  qy2 = qy1 + ((qheight * actual_h) / pheight );

	  CLUTTER_DBG("rendering text tile x: %i, y: %i - %ix%i", 
		      x, y, actual_w, actual_h);

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

static void
clutter_clone_texture_paint (ClutterActor *self)
{
  ClutterCloneTexturePrivate  *priv;
  ClutterActor              *parent_texture;
  gint                         x1, y1, x2, y2;

  priv = CLUTTER_CLONE_TEXTURE (self)->priv;

  /* parent texture may have been hidden, there for need to make sure its 
   * realised with resources available.  
  */
  parent_texture = CLUTTER_ACTOR (priv->parent_texture);
  if (!CLUTTER_ACTOR_IS_REALIZED (parent_texture))
    clutter_actor_realize (parent_texture);

  glEnable(GL_BLEND);
  glEnable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glColor4ub(255, 255, 255, clutter_actor_get_opacity(self));

  clutter_actor_get_coords (self, &x1, &y1, &x2, &y2);

  CLUTTER_DBG("paint to x1: %i, y1: %i x2: %i, y2: %i opacity: %i", 
	      x1, y1, x2, y2, clutter_actor_get_opacity(self) );

  clone_texture_render_to_gl_quad (CLUTTER_CLONE_TEXTURE(self), 
				   x1, y1, x2, y2);

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
}

static void
set_parent_texture (ClutterCloneTexture *ctexture,
		    ClutterTexture      *texture)
{
  ClutterCloneTexturePrivate *priv = ctexture->priv;

  if (priv->parent_texture)
    {
      g_object_unref (priv->parent_texture);
      priv->parent_texture = NULL;
    }

  if (texture) 
    {
      gint width, height;

      priv->parent_texture = g_object_ref (texture);

      /* Sync up the size to parent texture base pixbuf size. 
      */
      clutter_texture_get_base_size (texture, &width, &height);
      clutter_actor_set_size (CLUTTER_ACTOR(ctexture), width, height);
    }
      
}

static void 
clutter_clone_texture_dispose (GObject *object)
{
  ClutterCloneTexture         *self = CLUTTER_CLONE_TEXTURE(object);
  ClutterCloneTexturePrivate  *priv = self->priv;  

  if (priv->parent_texture)
    g_object_unref (priv->parent_texture);

  priv->parent_texture = NULL;

  G_OBJECT_CLASS (clutter_clone_texture_parent_class)->dispose (object);
}

static void 
clutter_clone_texture_finalize (GObject *object)
{
  G_OBJECT_CLASS (clutter_clone_texture_parent_class)->finalize (object);
}

static void
clutter_clone_texture_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)
{
  ClutterCloneTexture *ctexture = CLUTTER_CLONE_TEXTURE (object);

  switch (prop_id)
    {
    case PROP_PARENT_TEXTURE:
      set_parent_texture (ctexture, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_clone_texture_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
  ClutterCloneTexture *ctexture = CLUTTER_CLONE_TEXTURE (object);

  switch (prop_id)
    {
    case PROP_PARENT_TEXTURE:
      g_value_set_object (value, ctexture->priv->parent_texture);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_clone_texture_class_init (ClutterCloneTextureClass *klass)
{
  GObjectClass        *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint        = clutter_clone_texture_paint;

  gobject_class->finalize     = clutter_clone_texture_finalize;
  gobject_class->dispose      = clutter_clone_texture_dispose;
  gobject_class->set_property = clutter_clone_texture_set_property;
  gobject_class->get_property = clutter_clone_texture_get_property;

  g_object_class_install_property (gobject_class,
		  		   PROP_PARENT_TEXTURE,
				   g_param_spec_object ("parent-texture",
					   		"Parent Texture",
							"The parent texture to clone",
							CLUTTER_TYPE_TEXTURE,
							(G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE)));

  g_type_class_add_private (gobject_class, sizeof (ClutterCloneTexturePrivate));
}

static void
clutter_clone_texture_init (ClutterCloneTexture *self)
{
  ClutterCloneTexturePrivate *priv;

  self->priv = priv = CLUTTER_CLONE_TEXTURE_GET_PRIVATE (self);
  priv->parent_texture = NULL;
}

/**
 * clutter_clone_texture_new:
 * @texture: a #ClutterTexture
 *
 * Creates an efficient 'clone' of a pre-existing texture if which it 
 * shares the underlying pixbuf data. 
 *
 * Return value: the newly created #ClutterCloneTexture
 */
ClutterActor *
clutter_clone_texture_new (ClutterTexture *texture)
{
  g_return_val_if_fail (CLUTTER_IS_TEXTURE (texture), NULL);

  return g_object_new (CLUTTER_TYPE_CLONE_TEXTURE,
 		       "parent-texture", texture,
		       NULL);
}
