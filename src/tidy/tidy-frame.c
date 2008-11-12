/* tidy-frame.c: Simple container with a background
 *
 * Copyright (C) 2007 OpenedHand
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
 *
 * Written by: Emmanuele Bassi <ebassi@openedhand.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>

#include <cogl/cogl.h>
#include <clutter/clutter.h>

#include "tidy-frame.h"
#include "tidy-private.h"
#include "tidy-stylable.h"

#define TIDY_FRAME_GET_PRIVATE(obj)     (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TIDY_TYPE_FRAME, TidyFramePrivate))

enum
{
  PROP_0,

  PROP_CHILD,
  PROP_TEXTURE
};

struct _TidyFramePrivate
{
  ClutterActor *child;
  ClutterActor *texture;
};

static ClutterColor default_bg_color = { 0xcc, 0xcc, 0xcc, 0xff };

static void clutter_container_iface_init (ClutterContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (TidyFrame, tidy_frame, TIDY_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                clutter_container_iface_init));

static void
tidy_frame_get_preferred_width (ClutterActor *actor,
                                ClutterUnit   for_height,
                                ClutterUnit  *min_width_p,
                                ClutterUnit  *natural_width_p)
{
  TidyFramePrivate *priv = TIDY_FRAME (actor)->priv;
  TidyPadding padding = { 0, };
  ClutterUnit min_width, natural_width;

  tidy_actor_get_padding (TIDY_ACTOR (actor), &padding);

  min_width = 0;
  natural_width = padding.left + padding.right;

  if (priv->child)
    {
      ClutterUnit child_min, child_natural;

      clutter_actor_get_preferred_width (priv->child, for_height,
                                         &child_min,
                                         &child_natural);

      min_width += child_min;
      natural_width += child_natural;
    }

  if (min_width_p)
    *min_width_p = min_width;

  if (natural_width_p)
    *natural_width_p = natural_width;
}

static void
tidy_frame_get_preferred_height (ClutterActor *actor,
                                 ClutterUnit   for_width,
                                 ClutterUnit  *min_height_p,
                                 ClutterUnit  *natural_height_p)
{
  TidyFramePrivate *priv = TIDY_FRAME (actor)->priv;
  TidyPadding padding = { 0, };
  ClutterUnit min_height, natural_height;

  tidy_actor_get_padding (TIDY_ACTOR (actor), &padding);

  min_height = 0;
  natural_height = padding.top + padding.bottom;

  if (priv->child)
    {
      ClutterUnit child_min, child_natural;

      clutter_actor_get_preferred_height (priv->child, for_width,
                                          &child_min,
                                          &child_natural);

      min_height += child_min;
      natural_height += child_natural;
    }

  if (min_height_p)
    *min_height_p = min_height;

  if (natural_height_p)
    *natural_height_p = natural_height;
}

static void
tidy_frame_allocate (ClutterActor          *actor,
                     const ClutterActorBox *box,
                     gboolean               origin_changed)
{
  TidyFramePrivate *priv = TIDY_FRAME (actor)->priv;
  ClutterActorClass *klass;

  klass = CLUTTER_ACTOR_CLASS (tidy_frame_parent_class);
  klass->allocate (actor, box, origin_changed);

  if (priv->texture)
    {
      ClutterActorBox texture_box = { 0, };

      texture_box.x1 = 0;
      texture_box.y1 = 0;
      texture_box.x2 = box->x2 - box->x1;
      texture_box.y2 = box->y2 - box->y1;

      clutter_actor_allocate (priv->texture, &texture_box, origin_changed);
    }

  if (priv->child)
    {
      TidyPadding padding = { 0, };
      ClutterFixed x_align, y_align;
      ClutterUnit available_width, available_height;
      ClutterUnit child_width, child_height;
      ClutterActorBox child_box = { 0, };

      tidy_actor_get_padding (TIDY_ACTOR (actor), &padding);
      tidy_actor_get_alignmentx (TIDY_ACTOR (actor), &x_align, &y_align);

      available_width  = box->x2 - box->x1
                       - padding.left
                       - padding.right;
      available_height = box->y2 - box->y1
                       - padding.top
                       - padding.bottom;

      if (available_width < 0)
        available_width = 0;

      if (available_height < 0)
        available_height = 0;

      clutter_actor_get_preferred_size (priv->child,
                                        NULL, NULL,
                                        &child_width,
                                        &child_height);

      if (child_width > available_width)
        child_width = available_width;

      if (child_height > available_height)
        child_height = available_height;

      child_box.x1 = CLUTTER_FIXED_MUL ((available_width - child_width),
                                        x_align)
                   + padding.left;
      child_box.y1 = CLUTTER_FIXED_MUL ((available_height - child_height),
                                        y_align)
                   + padding.top;

      child_box.x2 = child_box.x1 + child_width;
      child_box.y2 = child_box.y1 + child_height;

      clutter_actor_allocate (priv->child, &child_box, origin_changed);
    }
}

static void
tidy_frame_paint (ClutterActor *actor)
{
  TidyFrame *frame = TIDY_FRAME (actor);
  TidyFramePrivate *priv = frame->priv;

  cogl_push_matrix ();

  if (priv->texture)
    clutter_actor_paint (priv->texture);
  else
    {
      ClutterActorBox allocation = { 0, };
      ClutterColor *bg_color;
      guint w, h;

      tidy_stylable_get (TIDY_STYLABLE (frame), "bg-color", &bg_color, NULL);
      if (!bg_color)
        bg_color = &default_bg_color;

      bg_color->alpha = clutter_actor_get_paint_opacity (actor)
                      * bg_color->alpha
                      / 255;

      clutter_actor_get_allocation_box (actor, &allocation);

      w = CLUTTER_UNITS_TO_DEVICE (allocation.x2 - allocation.x1);
      h = CLUTTER_UNITS_TO_DEVICE (allocation.y2 - allocation.y1);

      cogl_color (bg_color);
      cogl_rectangle (0, 0, w, h);
      
      if (bg_color != &default_bg_color)
        clutter_color_free (bg_color);
    }

  if (priv->child && CLUTTER_ACTOR_IS_VISIBLE (priv->child))
    clutter_actor_paint (priv->child);

  cogl_pop_matrix ();
}

static void
tidy_frame_pick (ClutterActor       *actor,
                 const ClutterColor *pick_color)
{
  TidyFramePrivate *priv = TIDY_FRAME (actor)->priv;

  /* chain up, so we get a box with our coordinates */
  CLUTTER_ACTOR_CLASS (tidy_frame_parent_class)->pick (actor, pick_color);

  if (priv->child && CLUTTER_ACTOR_IS_VISIBLE (priv->child))
    clutter_actor_paint (priv->child);
}

static void
tidy_frame_dispose (GObject *gobject)
{
  TidyFramePrivate *priv = TIDY_FRAME (gobject)->priv;

  if (priv->child)
    {
      clutter_actor_unparent (priv->child);
      priv->child = NULL;
    }

  if (priv->texture)
    {
      clutter_actor_unparent (priv->texture);
      priv->texture = NULL;
    }

  G_OBJECT_CLASS (tidy_frame_parent_class)->dispose (gobject);
}

static void
tidy_frame_set_property (GObject      *gobject,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_CHILD:
      clutter_container_add_actor (CLUTTER_CONTAINER (gobject),
                                   g_value_get_object (value));
      break;

    case PROP_TEXTURE:
      tidy_frame_set_texture (TIDY_FRAME (gobject),
                              g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
tidy_frame_get_property (GObject    *gobject,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  TidyFramePrivate *priv = TIDY_FRAME (gobject)->priv;

  switch (prop_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, priv->child);
      break;

    case PROP_TEXTURE:
      g_value_set_object (value, priv->texture);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
tidy_frame_class_init (TidyFrameClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TidyFramePrivate));

  gobject_class->set_property = tidy_frame_set_property;
  gobject_class->get_property = tidy_frame_get_property;
  gobject_class->dispose = tidy_frame_dispose;

  actor_class->pick = tidy_frame_pick;
  actor_class->paint = tidy_frame_paint;
  actor_class->allocate = tidy_frame_allocate;
  actor_class->get_preferred_width = tidy_frame_get_preferred_width;
  actor_class->get_preferred_height = tidy_frame_get_preferred_height;

  g_object_class_install_property (gobject_class,
                                   PROP_CHILD,
                                   g_param_spec_object ("child",
                                                        "Child",
                                                        "The child of the frame",
                                                        CLUTTER_TYPE_ACTOR,
                                                        TIDY_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_TEXTURE,
                                   g_param_spec_object ("texture",
                                                        "Texture",
                                                        "The background texture of the frame",
                                                        CLUTTER_TYPE_ACTOR,
                                                        TIDY_PARAM_READWRITE));
}

static void
tidy_frame_init (TidyFrame *frame)
{
  frame->priv = TIDY_FRAME_GET_PRIVATE (frame);
}

static void
tidy_frame_add_actor (ClutterContainer *container,
                      ClutterActor     *actor)
{
  TidyFramePrivate *priv = TIDY_FRAME (container)->priv;

  if (priv->child)
    clutter_actor_unparent (priv->child);

  clutter_actor_set_parent (actor, CLUTTER_ACTOR (container));
  priv->child = actor;

  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));

  g_signal_emit_by_name (container, "actor-added", actor);

  g_object_notify (G_OBJECT (container), "child");
}

static void
tidy_frame_remove_actor (ClutterContainer *container,
                         ClutterActor     *actor)
{
  TidyFramePrivate *priv = TIDY_FRAME (container)->priv;

  if (priv->child == actor)
    {
      g_object_ref (priv->child);

      clutter_actor_unparent (priv->child);
      priv->child = NULL;

      clutter_actor_queue_relayout (CLUTTER_ACTOR (container));

      g_signal_emit_by_name (container, "actor-removed", priv->child);

      g_object_unref (priv->child);
    }
}

static void
tidy_frame_foreach (ClutterContainer *container,
                    ClutterCallback   callback,
                    gpointer          callback_data)
{
  TidyFramePrivate *priv = TIDY_FRAME (container)->priv;

  if (priv->texture)
    callback (priv->texture, callback_data);

  if (priv->child)
    callback (priv->child, callback_data);
}

static void
tidy_frame_lower (ClutterContainer *container,
                  ClutterActor     *actor,
                  ClutterActor     *sibling)
{
  /* single child */
}

static void
tidy_frame_raise (ClutterContainer *container,
                  ClutterActor     *actor,
                  ClutterActor     *sibling)
{
  /* single child */
}

static void
tidy_frame_sort_depth_order (ClutterContainer *container)
{
  /* single child */
}

static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->add = tidy_frame_add_actor;
  iface->remove = tidy_frame_remove_actor;
  iface->foreach = tidy_frame_foreach;
  iface->lower = tidy_frame_lower;
  iface->raise = tidy_frame_raise;
  iface->sort_depth_order = tidy_frame_sort_depth_order;
}

ClutterActor *
tidy_frame_new (void)
{
  return g_object_new (TIDY_TYPE_FRAME, NULL);
}

ClutterActor *
tidy_frame_get_child (TidyFrame *frame)
{
  g_return_val_if_fail (TIDY_IS_FRAME (frame), NULL);

  return frame->priv->child;
}

void
tidy_frame_set_texture (TidyFrame    *frame,
                        ClutterActor *texture)
{
  TidyFramePrivate *priv;

  g_return_if_fail (TIDY_IS_FRAME (frame));
  g_return_if_fail (CLUTTER_IS_ACTOR (texture));

  priv = frame->priv;

  if (priv->texture == texture)
    return;

  if (priv->texture)
    {
      clutter_actor_unparent (priv->texture);
      priv->texture = NULL;
    }

  if (texture)
    {
      ClutterActor *parent = clutter_actor_get_parent (texture);

      if (G_UNLIKELY (parent != NULL))
        {
          g_warning ("Unable to set the background texture of type `%s' for "
                     "the frame of type `%s': the texture actor is already "
                     "a child of a container of type `%s'",
                     g_type_name (G_OBJECT_TYPE (texture)),
                     g_type_name (G_OBJECT_TYPE (frame)),
                     g_type_name (G_OBJECT_TYPE (parent)));
          return;
        }

      priv->texture = texture;
      clutter_actor_set_parent (texture, CLUTTER_ACTOR (frame));
    }

  clutter_actor_queue_relayout (CLUTTER_ACTOR (frame));

  g_object_notify (G_OBJECT (frame), "texture");
}

ClutterActor *
tidy_frame_get_texture (TidyFrame *frame)
{
  g_return_val_if_fail (TIDY_IS_FRAME (frame), NULL);

  return frame->priv->texture;
}

