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

#include "cogl/cogl.h"

enum
{
  PROP_0,
  PROP_PARENT_TEXTURE,
  PROP_REPEAT_Y,
  PROP_REPEAT_X
};

G_DEFINE_TYPE (ClutterCloneTexture,
	       clutter_clone_texture,
	       CLUTTER_TYPE_ACTOR);

#define CLUTTER_CLONE_TEXTURE_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_CLONE_TEXTURE, ClutterCloneTexturePrivate))

struct _ClutterCloneTexturePrivate
{
  ClutterTexture      *parent_texture;
  guint                repeat_x : 1;
  guint                repeat_y : 1;
};

static void
clutter_clone_texture_get_preferred_width (ClutterActor *self,
                                           ClutterUnit   for_height,
                                           ClutterUnit  *min_width_p,
                                           ClutterUnit  *natural_width_p)
{
  ClutterCloneTexturePrivate *priv = CLUTTER_CLONE_TEXTURE (self)->priv;
  ClutterActor *parent_texture;
  ClutterActorClass *parent_texture_class;

  /* Note that by calling the get_width_request virtual method directly
   * and skipping the clutter_actor_get_preferred_width() wrapper, we
   * are ignoring any size request override set on the parent texture
   * and just getting the normal size of the parent texture.
   */
  parent_texture = CLUTTER_ACTOR (priv->parent_texture);
  if (!parent_texture)
    {
      if (min_width_p)
        *min_width_p = 0;

      if (natural_width_p)
        *natural_width_p = 0;

      return;
    }

  parent_texture_class = CLUTTER_ACTOR_GET_CLASS (parent_texture);
  parent_texture_class->get_preferred_width (parent_texture,
                                             for_height,
                                             min_width_p,
                                             natural_width_p);
}

static void
clutter_clone_texture_get_preferred_height (ClutterActor *self,
                                            ClutterUnit   for_width,
                                            ClutterUnit  *min_height_p,
                                            ClutterUnit  *natural_height_p)
{
  ClutterCloneTexturePrivate *priv = CLUTTER_CLONE_TEXTURE (self)->priv;
  ClutterActor *parent_texture;
  ClutterActorClass *parent_texture_class;

  /* Note that by calling the get_height_request virtual method directly
   * and skipping the clutter_actor_get_preferred_height() wrapper, we
   * are ignoring any size request override set on the parent texture and
   * just getting the normal size of the parent texture.
   */
  parent_texture = CLUTTER_ACTOR (priv->parent_texture);
  if (!parent_texture)
    {
      if (min_height_p)
        *min_height_p = 0;

      if (natural_height_p)
        *natural_height_p = 0;

      return;
    }

  parent_texture_class = CLUTTER_ACTOR_GET_CLASS (parent_texture);
  parent_texture_class->get_preferred_height (parent_texture,
                                              for_width,
                                              min_height_p,
                                              natural_height_p);
}

static void
clutter_clone_texture_paint (ClutterActor *self)
{
  ClutterCloneTexturePrivate  *priv;
  ClutterActor                *parent_texture;
  gint                         x_1, y_1, x_2, y_2;
  CoglHandle                   cogl_texture;
  ClutterFixed                 t_w, t_h;
  guint                        tex_width, tex_height;

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

   /* If 'parent' texture isn't visible we run its paint to be sure it 
    * updates. Needed for TFP and likely FBOs. 
    * Potentially could cause issues 
    */
  if (!clutter_actor_get_paint_visibility(parent_texture))
    {
      CLUTTER_SET_PRIVATE_FLAGS(parent_texture,
                                CLUTTER_TEXTURE_IN_CLONE_PAINT);
      g_signal_emit_by_name (priv->parent_texture, "paint", NULL);
      CLUTTER_UNSET_PRIVATE_FLAGS(parent_texture,
                                  CLUTTER_TEXTURE_IN_CLONE_PAINT);
    }

  cogl_set_source_color4ub (255, 255, 255,
                            clutter_actor_get_paint_opacity (self));

  clutter_actor_get_allocation_coords (self, &x_1, &y_1, &x_2, &y_2);

  CLUTTER_NOTE (PAINT, "paint to x1: %i, y1: %i x2: %i, y2: %i "
		"opacity: %i",
		x_1, y_1, x_2, y_2,
		clutter_actor_get_opacity (self));

  cogl_texture = clutter_texture_get_cogl_texture (priv->parent_texture);

  if (cogl_texture == COGL_INVALID_HANDLE)
    return;

  tex_width = cogl_texture_get_width (cogl_texture);
  tex_height = cogl_texture_get_height (cogl_texture);

  if (priv->repeat_x && tex_width > 0)
    t_w = COGL_FIXED_DIV (COGL_FIXED_FROM_INT (x_2 - x_1),
                          COGL_FIXED_FROM_INT (tex_width));
  else
    t_w = COGL_FIXED_1;
  if (priv->repeat_y && tex_height > 0)
    t_h = COGL_FIXED_DIV (COGL_FIXED_FROM_INT (y_2 - y_1),
                          COGL_FIXED_FROM_INT (tex_height));
  else
    t_h = COGL_FIXED_1;

  /* Parent paint translated us into position */
  cogl_texture_rectangle (cogl_texture, 0, 0,
			  COGL_FIXED_FROM_INT (x_2 - x_1),
			  COGL_FIXED_FROM_INT (y_2 - y_1),
			  0, 0, t_w, t_h);
}

static void
set_parent_texture (ClutterCloneTexture *ctexture,
		    ClutterTexture      *texture)
{
  ClutterCloneTexturePrivate *priv = ctexture->priv;
  ClutterActor *actor = CLUTTER_ACTOR (ctexture);
  gboolean was_visible = CLUTTER_ACTOR_IS_VISIBLE (ctexture);

  if (priv->parent_texture)
    {
      g_object_unref (priv->parent_texture);
      priv->parent_texture = NULL;

      if (was_visible)
        clutter_actor_hide (actor);
    }

  if (texture) 
    {
      priv->parent_texture = g_object_ref (texture);

      /* queue a redraw if the cloned texture is already visible */
      if (CLUTTER_ACTOR_IS_VISIBLE (priv->parent_texture) &&
          was_visible)
        {
          clutter_actor_show (actor);
          clutter_actor_queue_redraw (actor);
        }

      clutter_actor_queue_relayout (actor);
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
  ClutterCloneTexture        *ctexture = CLUTTER_CLONE_TEXTURE (object);
  ClutterCloneTexturePrivate *priv;

  priv = ctexture->priv;

  switch (prop_id)
    {
    case PROP_PARENT_TEXTURE:
      set_parent_texture (ctexture, g_value_get_object (value));
      break;
    case PROP_REPEAT_X:
      if (priv->repeat_x != g_value_get_boolean (value))
	{
	  priv->repeat_x = !priv->repeat_x;
	  clutter_actor_queue_redraw (CLUTTER_ACTOR (ctexture));
	}
      break;
    case PROP_REPEAT_Y:
      if (priv->repeat_y != g_value_get_boolean (value))
	{
	  priv->repeat_y = !priv->repeat_y;
	  clutter_actor_queue_redraw (CLUTTER_ACTOR (ctexture));
	}
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
  ClutterCloneTexturePrivate *priv;

  priv = ctexture->priv;

  switch (prop_id)
    {
    case PROP_PARENT_TEXTURE:
      g_value_set_object (value, ctexture->priv->parent_texture);
      break;
    case PROP_REPEAT_X:
      g_value_set_boolean (value, priv->repeat_x);
      break;
    case PROP_REPEAT_Y:
      g_value_set_boolean (value, priv->repeat_y);
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

  actor_class->paint =
    clutter_clone_texture_paint;
  actor_class->get_preferred_width =
    clutter_clone_texture_get_preferred_width;
  actor_class->get_preferred_height =
    clutter_clone_texture_get_preferred_height;

  gobject_class->finalize     = clutter_clone_texture_finalize;
  gobject_class->dispose      = clutter_clone_texture_dispose;
  gobject_class->set_property = clutter_clone_texture_set_property;
  gobject_class->get_property = clutter_clone_texture_get_property;

  g_object_class_install_property
    (gobject_class, PROP_PARENT_TEXTURE,
     g_param_spec_object ("parent-texture",
			  "Parent Texture",
			  "The parent texture to clone",
			  CLUTTER_TYPE_TEXTURE,
			  CLUTTER_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_REPEAT_X,
     g_param_spec_boolean ("repeat-x",
			   "Tile underlying pixbuf in x direction",
			   "Reapeat underlying pixbuf rather than scale "
			   "in x direction.",
			   FALSE,
			   CLUTTER_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_REPEAT_Y,
     g_param_spec_boolean ("repeat-y",
			   "Tile underlying pixbuf in y direction",
			   "Reapeat underlying pixbuf rather than scale "
			   "in y direction.",
			   FALSE,
			   CLUTTER_PARAM_READWRITE));

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
 * @texture: a #ClutterTexture, or %NULL
 *
 * Creates an efficient 'clone' of a pre-existing texture with which it 
 * shares the underlying pixbuf data.
 *
 * You can use clutter_clone_texture_set_parent_texture() to change the
 * cloned texture.
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
