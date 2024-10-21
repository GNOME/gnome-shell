/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-bin.c: Basic container actor
 *
 * Copyright 2009 Intel Corporation.
 * Copyright 2009, 2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * StBin:
 *
 * A simple container with one actor.
 *
 * #StBin is a simple container capable of having only one
 * #ClutterActor as a child.
 */

#include "config.h"

#include <clutter/clutter.h>

#include "st-bin.h"
#include "st-enum-types.h"
#include "st-private.h"

typedef struct _StBinPrivate          StBinPrivate;
struct _StBinPrivate
{
  ClutterActor *child;
};

enum
{
  PROP_0,

  PROP_CHILD,

  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (StBin, st_bin, ST_TYPE_WIDGET)

static void
st_bin_dispose (GObject *object)
{
  StBinPrivate *priv = st_bin_get_instance_private (ST_BIN (object));

  g_clear_weak_pointer (&priv->child);

  G_OBJECT_CLASS (st_bin_parent_class)->dispose (object);
}

static void
set_child (StBin *bin, ClutterActor *child)
{
  StBinPrivate *priv = st_bin_get_instance_private (bin);

  if (!g_set_weak_pointer (&priv->child, child))
    return;

  clutter_actor_queue_relayout (CLUTTER_ACTOR (bin));

  g_object_notify_by_pspec (G_OBJECT (bin), props[PROP_CHILD]);
}

static void
st_bin_child_added (ClutterActor *container,
                    ClutterActor *actor)
{
  StBin *bin = ST_BIN (container);
  StBinPrivate *priv = st_bin_get_instance_private (bin);

  if (priv->child)
    g_warning ("Attempting to add an actor of type %s to "
               "an StBin, but the bin already contains a %s. "
               "Was add_child() used repeatedly?",
               G_OBJECT_TYPE_NAME (actor),
               G_OBJECT_TYPE_NAME (priv->child));

  set_child (ST_BIN (container), actor);
}

static void
st_bin_child_removed (ClutterActor *container,
                      ClutterActor *actor)
{
  StBin *bin = ST_BIN (container);
  StBinPrivate *priv = st_bin_get_instance_private (bin);

  if (priv->child == actor)
    set_child (bin, NULL);
}

static void
st_bin_popup_menu (StWidget *widget)
{
  StBinPrivate *priv = st_bin_get_instance_private (ST_BIN (widget));

  if (priv->child && ST_IS_WIDGET (priv->child))
    st_widget_popup_menu (ST_WIDGET (priv->child));
}

static gboolean
st_bin_navigate_focus (StWidget         *widget,
                       ClutterActor     *from,
                       StDirectionType   direction)
{
  StBinPrivate *priv = st_bin_get_instance_private (ST_BIN (widget));
  ClutterActor *bin_actor = CLUTTER_ACTOR (widget);

  if (st_widget_get_can_focus (widget))
    {
      if (from && clutter_actor_contains (bin_actor, from))
        return FALSE;

      if (clutter_actor_is_mapped (bin_actor))
        {
          clutter_actor_grab_key_focus (bin_actor);
          return TRUE;
        }
      else
        {
          return FALSE;
        }
    }
  else if (priv->child && ST_IS_WIDGET (priv->child))
    return st_widget_navigate_focus (ST_WIDGET (priv->child), from, direction, FALSE);
  else
    return FALSE;
}

static void
st_bin_set_property (GObject      *gobject,
                     guint         prop_id,
                     const GValue *value,
                     GParamSpec   *pspec)
{
  StBin *bin = ST_BIN (gobject);

  switch (prop_id)
    {
    case PROP_CHILD:
      st_bin_set_child (bin, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
st_bin_get_property (GObject    *gobject,
                     guint       prop_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
  StBinPrivate *priv = st_bin_get_instance_private (ST_BIN (gobject));

  switch (prop_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, priv->child);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
st_bin_class_init (StBinClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  StWidgetClass *widget_class = ST_WIDGET_CLASS (klass);

  gobject_class->dispose = st_bin_dispose;
  gobject_class->set_property = st_bin_set_property;
  gobject_class->get_property = st_bin_get_property;

  actor_class->child_added = st_bin_child_added;
  actor_class->child_removed = st_bin_child_removed;

  widget_class->popup_menu = st_bin_popup_menu;
  widget_class->navigate_focus = st_bin_navigate_focus;

  /**
   * StBin:child:
   *
   * The child #ClutterActor of the #StBin container.
   */
  props[PROP_CHILD] =
    g_param_spec_object ("child", NULL, NULL,
                         CLUTTER_TYPE_ACTOR,
                         ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, props);

  clutter_actor_class_set_layout_manager_type (actor_class, CLUTTER_TYPE_BIN_LAYOUT);
}

static void
st_bin_init (StBin *bin)
{
}

/**
 * st_bin_new:
 *
 * Creates a new #StBin, a simple container for one child.
 *
 * Returns: the newly created #StBin actor
 */
StWidget *
st_bin_new (void)
{
  return g_object_new (ST_TYPE_BIN, NULL);
}

/**
 * st_bin_set_child:
 * @bin: a #StBin
 * @child: (nullable): a #ClutterActor, or %NULL
 *
 * Sets @child as the child of @bin.
 *
 * If @bin already has a child, the previous child is removed.
 */
void
st_bin_set_child (StBin        *bin,
                  ClutterActor *child)
{
  StBinPrivate *priv;

  g_return_if_fail (ST_IS_BIN (bin));
  g_return_if_fail (child == NULL || CLUTTER_IS_ACTOR (child));

  priv = st_bin_get_instance_private (bin);

  g_object_freeze_notify (G_OBJECT (bin));

  if (priv->child)
    clutter_actor_remove_child (CLUTTER_ACTOR (bin), priv->child);

  if (child)
    clutter_actor_add_child (CLUTTER_ACTOR (bin), child);

  g_object_thaw_notify (G_OBJECT (bin));
}

/**
 * st_bin_get_child:
 * @bin: a #StBin
 *
 * Gets the #ClutterActor child for @bin.
 *
 * Returns: (transfer none) (nullable): a #ClutterActor, or %NULL
 */
ClutterActor *
st_bin_get_child (StBin *bin)
{
  StBinPrivate *priv;

  g_return_val_if_fail (ST_IS_BIN (bin), NULL);

  priv = st_bin_get_instance_private (bin);

  return priv->child;
}
