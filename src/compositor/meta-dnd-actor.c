/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2014 Red Hat, Inc.
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
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

/**
 * SECTION:meta-dnd-actor
 * @title: MetaDnDActor
 * @short_description: Actor for painting the drag and drop surface
 *
 */

#include <config.h>

#include <clutter/clutter.h>

#include "meta-dnd-actor-private.h"

#define DRAG_FAILED_DURATION 500

enum {
  PROP_DRAG_ORIGIN = 1,
  PROP_DRAG_START_X,
  PROP_DRAG_START_Y
};

typedef struct _MetaDnDActorPrivate MetaDnDActorPrivate;

struct _MetaDnDActorPrivate
{
  ClutterActor *drag_origin;
  int drag_start_x;
  int drag_start_y;
};

G_DEFINE_TYPE_WITH_PRIVATE (MetaDnDActor, meta_dnd_actor, META_TYPE_FEEDBACK_ACTOR)

static void
meta_dnd_actor_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  MetaDnDActor *self = META_DND_ACTOR (object);
  MetaDnDActorPrivate *priv = meta_dnd_actor_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DRAG_ORIGIN:
      priv->drag_origin = g_value_get_object (value);
      break;
    case PROP_DRAG_START_X:
      priv->drag_start_x = g_value_get_int (value);
      break;
    case PROP_DRAG_START_Y:
      priv->drag_start_y = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_dnd_actor_get_property (GObject      *object,
                             guint         prop_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  MetaDnDActor *self = META_DND_ACTOR (object);
  MetaDnDActorPrivate *priv = meta_dnd_actor_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DRAG_ORIGIN:
      g_value_set_object (value, priv->drag_origin);
      break;
    case PROP_DRAG_START_X:
      g_value_set_int (value, priv->drag_start_x);
      break;
    case PROP_DRAG_START_Y:
      g_value_set_int (value, priv->drag_start_y);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_dnd_actor_class_init (MetaDnDActorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  object_class->set_property = meta_dnd_actor_set_property;
  object_class->get_property = meta_dnd_actor_get_property;

  pspec = g_param_spec_object ("drag-origin",
                               "Drag origin",
                               "The origin of the DnD operation",
                               CLUTTER_TYPE_ACTOR,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_property (object_class,
                                   PROP_DRAG_ORIGIN,
                                   pspec);

  pspec = g_param_spec_int ("drag-start-x",
                            "Drag start X",
                            "The X axis of the drag start point",
                            0, G_MAXINT, 0,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_property (object_class,
                                   PROP_DRAG_START_X,
                                   pspec);

  pspec = g_param_spec_int ("drag-start-y",
                            "Drag start Y",
                            "The Y axis of the drag start point",
                            0, G_MAXINT, 0,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_property (object_class,
                                   PROP_DRAG_START_Y,
                                   pspec);
}

static void
meta_dnd_actor_init (MetaDnDActor *self)
{
}

/**
 * meta_dnd_actor_new:
 *
 * Creates a new actor to draw the current drag and drop surface.
 *
 * Return value: the newly created background actor
 */
ClutterActor *
meta_dnd_actor_new (ClutterActor *drag_origin,
                    int           drag_start_x,
                    int           drag_start_y)
{
  MetaDnDActor *self;

  self = g_object_new (META_TYPE_DND_ACTOR,
                       "drag-origin", drag_origin,
                       "drag-start-x", drag_start_x,
                       "drag-start-y", drag_start_y,
                       NULL);

  return CLUTTER_ACTOR (self);
}

static void
drag_failed_complete (ClutterTimeline *timeline,
                      gboolean         is_finished,
                      gpointer         user_data)
{
  ClutterActor *self = user_data;

  clutter_actor_remove_all_children (self);
  clutter_actor_destroy (self);
}

void
meta_dnd_actor_drag_finish (MetaDnDActor *self,
                            gboolean      success)
{
  MetaDnDActorPrivate *priv;
  ClutterActor *actor;

  g_return_if_fail (META_IS_DND_ACTOR (self));

  actor = CLUTTER_ACTOR (self);
  priv = meta_dnd_actor_get_instance_private (self);

  if (success)
    {
      clutter_actor_remove_all_children (CLUTTER_ACTOR (self));
      clutter_actor_destroy (CLUTTER_ACTOR (self));
    }
  else
    {
      ClutterTransition *transition;

      clutter_actor_save_easing_state (actor);
      clutter_actor_set_easing_mode (actor, CLUTTER_EASE_OUT_CUBIC);
      clutter_actor_set_easing_duration (actor, DRAG_FAILED_DURATION);
      clutter_actor_set_opacity (actor, 0);

      if (CLUTTER_ACTOR_IS_VISIBLE (priv->drag_origin))
        {
          int anchor_x, anchor_y;
          ClutterPoint dest;

          clutter_actor_get_transformed_position (priv->drag_origin,
                                                  &dest.x, &dest.y);
          meta_feedback_actor_get_anchor (META_FEEDBACK_ACTOR (self),
                                          &anchor_x, &anchor_y);

          dest.x += priv->drag_start_x - anchor_x;
          dest.y += priv->drag_start_y - anchor_y;
          clutter_actor_set_position (actor, dest.x, dest.y);
        }

      transition = clutter_actor_get_transition (actor, "opacity");
      g_signal_connect (transition, "stopped",
                        G_CALLBACK (drag_failed_complete), self);

      clutter_actor_restore_easing_state (actor);
    }
}
