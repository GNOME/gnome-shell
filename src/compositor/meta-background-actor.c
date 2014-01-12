/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2009 Sander Dijkhuis
 * Copyright 2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Portions adapted from gnome-shell/src/shell-global.c
 */

/**
 * SECTION:meta-background-actor
 * @title: MetaBackgroundActor
 * @short_description: Actor for painting the root window background
 *
 */

#include <config.h>

#include <cogl/cogl-texture-pixmap-x11.h>

#include <clutter/clutter.h>

#include <X11/Xatom.h>

#include "cogl-utils.h"
#include "compositor-private.h"
#include <meta/errors.h>
#include <meta/meta-background.h>
#include "meta-background-actor-private.h"
#include "meta-cullable.h"

struct _MetaBackgroundActorPrivate
{
  cairo_region_t *clip_region;
};

static void cullable_iface_init (MetaCullableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaBackgroundActor, meta_background_actor, CLUTTER_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (META_TYPE_CULLABLE, cullable_iface_init));

static void
set_clip_region (MetaBackgroundActor *self,
                 cairo_region_t      *clip_region)
{
  MetaBackgroundActorPrivate *priv = self->priv;

  g_clear_pointer (&priv->clip_region, (GDestroyNotify) cairo_region_destroy);
  if (clip_region)
    priv->clip_region = cairo_region_copy (clip_region);
}

static void
meta_background_actor_dispose (GObject *object)
{
  MetaBackgroundActor *self = META_BACKGROUND_ACTOR (object);

  set_clip_region (self, NULL);

  G_OBJECT_CLASS (meta_background_actor_parent_class)->dispose (object);
}

static void
meta_background_actor_get_preferred_width (ClutterActor *actor,
                                           gfloat        for_height,
                                           gfloat       *min_width_p,
                                           gfloat       *natural_width_p)
{
  ClutterContent *content;
  gfloat width;

  content = clutter_actor_get_content (actor);

  if (content)
    clutter_content_get_preferred_size (content, &width, NULL);
  else
    width = 0;

  if (min_width_p)
    *min_width_p = width;
  if (natural_width_p)
    *natural_width_p = width;
}

static void
meta_background_actor_get_preferred_height (ClutterActor *actor,
                                            gfloat        for_width,
                                            gfloat       *min_height_p,
                                            gfloat       *natural_height_p)

{
  ClutterContent *content;
  gfloat height;

  content = clutter_actor_get_content (actor);

  if (content)
    clutter_content_get_preferred_size (content, NULL, &height);
  else
    height = 0;

  if (min_height_p)
    *min_height_p = height;
  if (natural_height_p)
    *natural_height_p = height;
}

static void
meta_background_actor_class_init (MetaBackgroundActorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MetaBackgroundActorPrivate));

  object_class->dispose = meta_background_actor_dispose;

  actor_class->get_preferred_width = meta_background_actor_get_preferred_width;
  actor_class->get_preferred_height = meta_background_actor_get_preferred_height;
}

static void
meta_background_actor_init (MetaBackgroundActor *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            META_TYPE_BACKGROUND_ACTOR,
                                            MetaBackgroundActorPrivate);
}

/**
 * meta_background_actor_new:
 *
 * Creates a new actor to draw the background for the given monitor.
 * This actor should be associated with a #MetaBackground using
 * clutter_actor_set_content()
 *
 * Return value: the newly created background actor
 */
ClutterActor *
meta_background_actor_new (void)
{
  MetaBackgroundActor *self;

  self = g_object_new (META_TYPE_BACKGROUND_ACTOR, NULL);

  return CLUTTER_ACTOR (self);
}

static void
meta_background_actor_cull_out (MetaCullable   *cullable,
                                cairo_region_t *unobscured_region,
                                cairo_region_t *clip_region)
{
  MetaBackgroundActor *self = META_BACKGROUND_ACTOR (cullable);
  set_clip_region (self, clip_region);
}

static void
meta_background_actor_reset_culling (MetaCullable *cullable)
{
  MetaBackgroundActor *self = META_BACKGROUND_ACTOR (cullable);
  set_clip_region (self, NULL);
}

static void
cullable_iface_init (MetaCullableInterface *iface)
{
  iface->cull_out = meta_background_actor_cull_out;
  iface->reset_culling = meta_background_actor_reset_culling;
}

/**
 * meta_background_actor_get_clip_region:
 * @self: a #MetaBackgroundActor
 *
 * Return value (transfer full): a #cairo_region_t that represents the part of
 * the background not obscured by other #MetaBackgroundActor or
 * #MetaWindowActor objects.
 */
cairo_region_t *
meta_background_actor_get_clip_region (MetaBackgroundActor *self)
{
  MetaBackgroundActorPrivate *priv = self->priv;
  ClutterActorBox content_box;
  cairo_rectangle_int_t content_area = { 0 };
  cairo_region_t *clip_region;

  g_return_val_if_fail (META_IS_BACKGROUND_ACTOR (self), NULL);

  if (!priv->clip_region)
      return NULL;

  clutter_actor_get_content_box (CLUTTER_ACTOR (self), &content_box);

  content_area.x = content_box.x1;
  content_area.y = content_box.y1;
  content_area.width = content_box.x2 - content_box.x1;
  content_area.height = content_box.y2 - content_box.y1;

  clip_region = cairo_region_create_rectangle (&content_area);
  cairo_region_intersect (clip_region, priv->clip_region);

  return clip_region;
}
