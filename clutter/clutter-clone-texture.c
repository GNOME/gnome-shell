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
 * SECTION:clutter-clone-texture
 * @short_description: Actor for cloning existing textures in an 
 * efficient way.
 *
 * #ClutterCloneTexture allows the cloning of existing #ClutterTexture based
 * actors whilst saving underlying graphics resources.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-clone-texture.h"
#include "clutter-main.h"
#include "clutter-feature.h"
#include "clutter-actor.h"
#include "clutter-util.h" 
#include "clutter-enum-types.h"
#include "clutter-private.h"
#include "clutter-debug.h"

#include "cogl.h"

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
				 int                  x_1, 
				 int                  y_1, 
				 int                  x_2, 
				 int                  y_2)
{
  gint   qx1 = 0, qx2 = 0, qy1 = 0, qy2 = 0;
  gint   qwidth = 0, qheight = 0;
  gint   x, y, i = 0, lastx = 0, lasty = 0;
  gint   n_x_tiles, n_y_tiles; 
  gint   pwidth, pheight;
  float tx, ty;

  ClutterCloneTexturePrivate *priv = ctexture->priv;
  ClutterActor *parent_actor = CLUTTER_ACTOR (priv->parent_texture);

  priv = ctexture->priv;

  qwidth  = x_2 - x_1;
  qheight = y_2 - y_1;

  if (!CLUTTER_ACTOR_IS_REALIZED (parent_actor))
      clutter_actor_realize (parent_actor);

  /* Only paint if parent is in a state to do so */
  if (!clutter_texture_has_generated_tiles (priv->parent_texture))
    return;
  
  clutter_texture_get_base_size (priv->parent_texture, &pwidth, &pheight); 

  if (!clutter_texture_is_tiled (priv->parent_texture))
    {
      clutter_texture_bind_tile (priv->parent_texture, 0);

      /* NPOTS textures *always* used if extension available
       */
      if (clutter_feature_available (CLUTTER_FEATURE_TEXTURE_RECTANGLE))
	{
	  tx = (float) pwidth;
	  ty = (float) pheight;
	}
      else
	{
	  tx = (float) pwidth / clutter_util_next_p2 (pwidth);  
	  ty = (float) pheight / clutter_util_next_p2 (pheight);
	}

      cogl_texture_quad (x_1, x_2, y_1, y_2, 
			 0,
			 0,
			 CLUTTER_FLOAT_TO_FIXED (tx),
			 CLUTTER_FLOAT_TO_FIXED (ty));
      return;
    }

  clutter_texture_get_n_tiles (priv->parent_texture, &n_x_tiles, &n_y_tiles); 

  for (x = 0; x < n_x_tiles; x++)
    {
      lasty = 0;

      for (y = 0; y < n_y_tiles; y++)
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

	  qx1 = x_1 + lastx;
	  qx2 = qx1 + ((qwidth * actual_w ) / pwidth );
	  
	  qy1 = y_1 + lasty;
	  qy2 = qy1 + ((qheight * actual_h) / pheight );

	  CLUTTER_NOTE (TEXTURE,
                        "rendering text tile x: %i, y: %i - %ix%i",
			x, y,
			actual_w, actual_h);

	  cogl_texture_quad (qx1, qx2, qy1, qy2, 
			     0,
			     0,
			     CLUTTER_FLOAT_TO_FIXED (tx),
			     CLUTTER_FLOAT_TO_FIXED (ty));

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
  ClutterActor                *parent_texture;
  gint                         x_1, y_1, x_2, y_2;
  GLenum                       target_type;
  ClutterColor                 col = { 0xff, 0xff, 0xff, 0xff };

  priv = CLUTTER_CLONE_TEXTURE (self)->priv;

  /* no need to paint stuff if we don't have a texture to clone */
  if (!priv->parent_texture)
    return;

  CLUTTER_NOTE (PAINT,
                "painting clone texture '%s'",
		clutter_actor_get_name (self) ? clutter_actor_get_name (self)
                                              : "unknown");

  /* parent texture may have been hidden, there for need to make sure its 
   * realised with resources available.  
  */
  parent_texture = CLUTTER_ACTOR (priv->parent_texture);
  if (!CLUTTER_ACTOR_IS_REALIZED (parent_texture))
    clutter_actor_realize (parent_texture);

  cogl_push_matrix ();

  /* FIXME: figure out nicer way of getting at this info...  
   */  
  if (clutter_feature_available (CLUTTER_FEATURE_TEXTURE_RECTANGLE) &&
      clutter_texture_is_tiled (CLUTTER_TEXTURE (parent_texture)) == FALSE)
    {
      target_type = CGL_TEXTURE_RECTANGLE_ARB;
      cogl_enable (CGL_ENABLE_TEXTURE_RECT|CGL_ENABLE_BLEND);
    }
  else
    {
      target_type = CGL_TEXTURE_2D;
      cogl_enable (CGL_ENABLE_TEXTURE_2D|CGL_ENABLE_BLEND);
    }

  col.alpha = clutter_actor_get_opacity (self);
  cogl_color (&col);

  clutter_actor_get_coords (self, &x_1, &y_1, &x_2, &y_2);

  CLUTTER_NOTE (PAINT, "paint to x1: %i, y1: %i x2: %i, y2: %i "
		"opacity: %i",
		x_1, y_1, x_2, y_2,
		clutter_actor_get_opacity (self));

  /* Parent paint translated us into position */
  clone_texture_render_to_gl_quad (CLUTTER_CLONE_TEXTURE (self), 
				   0, 0, x_2 - x_1, y_2 - y_1);

  cogl_pop_matrix ();
}

static void
set_parent_texture (ClutterCloneTexture *ctexture,
		    ClutterTexture      *texture)
{
  ClutterCloneTexturePrivate *priv = ctexture->priv;
  ClutterActor *actor = CLUTTER_ACTOR (ctexture);

  if (priv->parent_texture)
    {
      g_object_unref (priv->parent_texture);
      priv->parent_texture = NULL;
    }

  clutter_actor_hide (actor);

  if (texture) 
    {
      gint width, height;

      priv->parent_texture = g_object_ref (texture);

      /* Sync up the size to parent texture base pixbuf size. */
      clutter_texture_get_base_size (texture, &width, &height);
      clutter_actor_set_size (actor, width, height);

      /* queue a redraw if the cloned texture is already visible */
      if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (priv->parent_texture)) &&
          CLUTTER_ACTOR_IS_VISIBLE (actor))
        clutter_actor_queue_redraw (actor);
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
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint = clutter_clone_texture_paint;

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
							(G_PARAM_CONSTRUCT | CLUTTER_PARAM_READWRITE)));

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
 * @texture: a #ClutterTexture or %NULL
 *
 * Creates an efficient 'clone' of a pre-existing texture if which it 
 * shares the underlying pixbuf data.
 *
 * You can use clutter_clone_texture_set_parent_texture() to change the
 * parent texture to be cloned.
 *
 * Return value: the newly created #ClutterCloneTexture
 */
ClutterActor *
clutter_clone_texture_new (ClutterTexture *texture)
{
  g_return_val_if_fail (texture == NULL || CLUTTER_IS_TEXTURE (texture), NULL);

  return g_object_new (CLUTTER_TYPE_CLONE_TEXTURE,
 		       "parent-texture", texture,
		       NULL);
}

/**
 * clutter_clone_texture_get_parent_texture:
 * @clone: a #ClutterCloneTexture
 * 
 * Retrieves the parent #ClutterTexture used by @clone.
 *
 * Return value: a #ClutterTexture actor, or %NULL
 *
 * Since: 0.2
 */
ClutterTexture *
clutter_clone_texture_get_parent_texture (ClutterCloneTexture *clone)
{
  g_return_val_if_fail (CLUTTER_IS_CLONE_TEXTURE (clone), NULL);

  return clone->priv->parent_texture;
}

/**
 * clutter_clone_texture_set_parent_texture:
 * @clone: a #ClutterCloneTexture
 * @texture: a #ClutterTexture or %NULL
 *
 * Sets the parent texture cloned by the #ClutterCloneTexture.
 *
 * Since: 0.2
 */
void
clutter_clone_texture_set_parent_texture (ClutterCloneTexture *clone,
                                          ClutterTexture      *texture)
{
  g_return_if_fail (CLUTTER_IS_CLONE_TEXTURE (clone));
  g_return_if_fail (texture == NULL || CLUTTER_IS_TEXTURE (texture));

  g_object_ref (clone);

  set_parent_texture (clone, texture);

  g_object_notify (G_OBJECT (clone), "parent-texture");
  g_object_unref (clone);
}
