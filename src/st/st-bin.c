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
 * SECTION:st-bin
 * @short_description: a simple container with one actor
 *
 * #StBin is a simple container capable of having only one
 * #ClutterActor as a child.
 *
 * #StBin inherits from #StWidget, so it is fully themable.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

static void clutter_container_iface_init (ClutterContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (StBin, st_bin, ST_TYPE_WIDGET,
                         G_ADD_PRIVATE (StBin)
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                clutter_container_iface_init));

static void
st_bin_add (ClutterContainer *container,
            ClutterActor     *actor)
{
  st_bin_set_child (ST_BIN (container), actor);
}

static void
st_bin_remove (ClutterContainer *container,
               ClutterActor     *actor)
{
  StBin *bin = ST_BIN (container);
  StBinPrivate *priv = st_bin_get_instance_private (bin);

  if (priv->child == actor)
    st_bin_set_child (bin, NULL);
}

static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->add = st_bin_add;
  iface->remove = st_bin_remove;
}

static double
get_align_factor (ClutterActorAlign align)
{
  switch (align)
    {
    case CLUTTER_ACTOR_ALIGN_CENTER:
      return 0.5;

    case CLUTTER_ACTOR_ALIGN_START:
      return 0.0;

    case CLUTTER_ACTOR_ALIGN_END:
      return 1.0;

    case CLUTTER_ACTOR_ALIGN_FILL:
      break;
   }

  return 0.0;
}

static void
st_bin_allocate (ClutterActor          *self,
                 const ClutterActorBox *box)
{
  StBinPrivate *priv = st_bin_get_instance_private (ST_BIN (self));

  clutter_actor_set_allocation (self, box);

  if (priv->child && clutter_actor_is_visible (priv->child))
    {
      StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (self));
      ClutterActorAlign x_align = clutter_actor_get_x_align (priv->child);
      ClutterActorAlign y_align = clutter_actor_get_y_align (priv->child);
      ClutterActorBox childbox;

      st_theme_node_get_content_box (theme_node, box, &childbox);
      clutter_actor_allocate_align_fill (priv->child, &childbox,
                                         get_align_factor (x_align),
                                         get_align_factor (y_align),
                                         x_align == CLUTTER_ACTOR_ALIGN_FILL,
                                         y_align == CLUTTER_ACTOR_ALIGN_FILL);
    }
}

static void
st_bin_get_preferred_width (ClutterActor *self,
                            gfloat        for_height,
                            gfloat       *min_width_p,
                            gfloat       *natural_width_p)
{
  StBinPrivate *priv = st_bin_get_instance_private (ST_BIN (self));
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (self));

  st_theme_node_adjust_for_height (theme_node, &for_height);

  if (priv->child == NULL || !clutter_actor_is_visible (priv->child))
    {
      if (min_width_p)
        *min_width_p = 0;

      if (natural_width_p)
        *natural_width_p = 0;
    }
  else
    {
      ClutterActorAlign y_align = clutter_actor_get_y_align (priv->child);

      _st_actor_get_preferred_width (priv->child, for_height,
                                     y_align == CLUTTER_ACTOR_ALIGN_FILL,
                                     min_width_p,
                                     natural_width_p);
    }

  st_theme_node_adjust_preferred_width (theme_node, min_width_p, natural_width_p);
}

static void
st_bin_get_preferred_height (ClutterActor *self,
                             gfloat        for_width,
                             gfloat       *min_height_p,
                             gfloat       *natural_height_p)
{
  StBinPrivate *priv = st_bin_get_instance_private (ST_BIN (self));
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (self));

  st_theme_node_adjust_for_width (theme_node, &for_width);

  if (priv->child == NULL || !clutter_actor_is_visible (priv->child))
    {
      if (min_height_p)
        *min_height_p = 0;

      if (natural_height_p)
        *natural_height_p = 0;
    }
  else
    {
      ClutterActorAlign x_align = clutter_actor_get_y_align (priv->child);

      _st_actor_get_preferred_height (priv->child, for_width,
                                      x_align == CLUTTER_ACTOR_ALIGN_FILL,
                                      min_height_p,
                                      natural_height_p);
    }

  st_theme_node_adjust_preferred_height (theme_node, min_height_p, natural_height_p);
}

static void
st_bin_destroy (ClutterActor *actor)
{
  StBinPrivate *priv = st_bin_get_instance_private (ST_BIN (actor));

  if (priv->child)
    clutter_actor_destroy (priv->child);
  g_assert (priv->child == NULL);

  CLUTTER_ACTOR_CLASS (st_bin_parent_class)->destroy (actor);
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

  gobject_class->set_property = st_bin_set_property;
  gobject_class->get_property = st_bin_get_property;

  actor_class->get_preferred_width = st_bin_get_preferred_width;
  actor_class->get_preferred_height = st_bin_get_preferred_height;
  actor_class->allocate = st_bin_allocate;
  actor_class->destroy = st_bin_destroy;

  widget_class->popup_menu = st_bin_popup_menu;
  widget_class->navigate_focus = st_bin_navigate_focus;

  /**
   * StBin:child:
   *
   * The child #ClutterActor of the #StBin container.
   */
  props[PROP_CHILD] =
    g_param_spec_object ("child",
                         "Child",
                         "The child of the Bin",
                         CLUTTER_TYPE_ACTOR,
                         ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, props);
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

  if (priv->child == child)
    return;

  if (child)
    {
      ClutterActor *parent = clutter_actor_get_parent (child);

      if (parent)
        {
          g_warning ("%s: The provided 'child' actor %p already has a "
                     "(different) parent %p and can't be made a child of %p.",
                     G_STRFUNC, child, parent, bin);
          return;
        }
    }

  if (priv->child)
    clutter_actor_remove_child (CLUTTER_ACTOR (bin), priv->child);

  priv->child = NULL;

  if (child)
    {
      priv->child = child;
      clutter_actor_add_child (CLUTTER_ACTOR (bin), child);
    }

  clutter_actor_queue_relayout (CLUTTER_ACTOR (bin));

  g_object_notify_by_pspec (G_OBJECT (bin), props[PROP_CHILD]);
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
  g_return_val_if_fail (ST_IS_BIN (bin), NULL);

  return ((StBinPrivate *)st_bin_get_instance_private (bin))->child;
}
