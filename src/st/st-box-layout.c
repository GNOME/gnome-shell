/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-box-layout.h: box layout actor
 *
 * Copyright 2009 Intel Corporation.
 * Copyright 2009 Abderrahim Kitouni
 * Copyright 2009, 2010 Red Hat, Inc.
 * Copyright 2010 Florian Muellner
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

/* Portions copied from Clutter:
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

/**
 * StBoxLayout:
 *
 * Layout container arranging children in a single line.
 *
 * The #StBoxLayout arranges its children along a single line, where each
 * child can be allocated either its preferred size or larger if the expand
 * option is set. If the fill option is set, the actor will be allocated more
 * than its requested size. If the fill option is not set, but the expand option
 * is enabled, then the position of the actor within the available space can
 * be determined by the alignment child property.
 */

#include <stdlib.h>

#include "st-box-layout.h"

#include "st-private.h"
#include "st-scrollable.h"


enum {
  PROP_0,

  PROP_ORIENTATION,
  PROP_VERTICAL,

  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

struct _StBoxLayoutPrivate
{
  StAdjustment *hadjustment;
  StAdjustment *vadjustment;
};

G_DEFINE_TYPE_WITH_PRIVATE (StBoxLayout, st_box_layout, ST_TYPE_VIEWPORT);


static void
st_box_layout_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ClutterLayoutManager *layout;
  ClutterOrientation orientation;

  switch (property_id)
    {
    case PROP_ORIENTATION:
      layout = clutter_actor_get_layout_manager (CLUTTER_ACTOR (object));
      orientation = clutter_box_layout_get_orientation (CLUTTER_BOX_LAYOUT (layout));
      g_value_set_enum (value, orientation);
      break;
    case PROP_VERTICAL:
      layout = clutter_actor_get_layout_manager (CLUTTER_ACTOR (object));
      orientation = clutter_box_layout_get_orientation (CLUTTER_BOX_LAYOUT (layout));
      g_value_set_boolean (value, orientation == CLUTTER_ORIENTATION_VERTICAL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
st_box_layout_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  StBoxLayout *box = ST_BOX_LAYOUT (object);

  switch (property_id)
    {
    case PROP_ORIENTATION:
      st_box_layout_set_orientation (box, g_value_get_enum (value));
      break;
    case PROP_VERTICAL:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      st_box_layout_set_vertical (box, g_value_get_boolean (value));
G_GNUC_END_IGNORE_DEPRECATIONS
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
st_box_layout_style_changed (StWidget *self)
{
  StThemeNode *theme_node = st_widget_get_theme_node (self);
  ClutterBoxLayout *layout;
  double spacing;

  layout = CLUTTER_BOX_LAYOUT (clutter_actor_get_layout_manager (CLUTTER_ACTOR (self)));

  spacing = st_theme_node_get_length (theme_node, "spacing");
  clutter_box_layout_set_spacing (layout, (int)(spacing + 0.5));

  ST_WIDGET_CLASS (st_box_layout_parent_class)->style_changed (self);
}

static void
on_layout_orientation_changed (GObject    *object,
                               GParamSpec *pspec,
                               gpointer    user_data)
{
  GObject *box = user_data;
  g_object_notify_by_pspec (box, props[PROP_ORIENTATION]);
  g_object_notify_by_pspec (box, props[PROP_VERTICAL]);
}

static void
on_layout_manager_notify (GObject    *object,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
  ClutterActor *actor = CLUTTER_ACTOR (object);
  ClutterLayoutManager *layout = clutter_actor_get_layout_manager (actor);

  if (layout == NULL)
    return;

  g_signal_connect (layout, "notify::orientation",
                    G_CALLBACK (on_layout_orientation_changed), object);
}

static void
st_box_layout_class_init (StBoxLayoutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  StWidgetClass *widget_class = ST_WIDGET_CLASS (klass);

  object_class->get_property = st_box_layout_get_property;
  object_class->set_property = st_box_layout_set_property;

  widget_class->style_changed = st_box_layout_style_changed;

  /**
   * StBoxLayout:orientation:
   *
   * The orientation of the #StBoxLayout, either horizontal or
   * vertical
   */
  props[PROP_ORIENTATION] =
    g_param_spec_enum ("orientation", NULL, NULL,
                       CLUTTER_TYPE_ORIENTATION,
                       CLUTTER_ORIENTATION_HORIZONTAL,
                       ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StBoxLayout:vertical:
   *
   * A convenience property for the #ClutterBoxLayout:vertical property of the
   * internal layout for #StBoxLayout.
   */
  props[PROP_VERTICAL] =
    g_param_spec_boolean ("vertical", NULL, NULL,
                          FALSE,
                          ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_DEPRECATED);

  g_object_class_install_properties (object_class, N_PROPS, props);

  clutter_actor_class_set_layout_manager_type (actor_class, CLUTTER_TYPE_BOX_LAYOUT);
}

static void
st_box_layout_init (StBoxLayout *self)
{
  self->priv = st_box_layout_get_instance_private (self);

  g_signal_connect (self, "notify::layout-manager",
                    G_CALLBACK (on_layout_manager_notify), NULL);
}

/**
 * st_box_layout_new:
 *
 * Create a new #StBoxLayout.
 *
 * Returns: a newly allocated #StBoxLayout
 */
StWidget *
st_box_layout_new (void)
{
  return g_object_new (ST_TYPE_BOX_LAYOUT, NULL);
}

/**
 * st_box_layout_set_orientation:
 * @box: A #StBoxLayout
 * @orientation: the orientation of the #StBoxLayout
 *
 * Set the value of the #StBoxLayout:orientation property
 */
void
st_box_layout_set_orientation (StBoxLayout        *box,
                               ClutterOrientation  orientation)
{
  ClutterLayoutManager *layout;

  g_return_if_fail (ST_IS_BOX_LAYOUT (box));

  layout = clutter_actor_get_layout_manager (CLUTTER_ACTOR (box));

  if (clutter_box_layout_get_orientation (CLUTTER_BOX_LAYOUT (layout)) != orientation)
    clutter_box_layout_set_orientation (CLUTTER_BOX_LAYOUT (layout), orientation);
}

/**
 * st_box_layout_get_orientation:
 * @box: A #StBoxLayout
 *
 * Get the value of the #StBoxLayout:orientation property.
 *
 * Returns: the orientation
 */
ClutterOrientation
st_box_layout_get_orientation (StBoxLayout *box)
{
  ClutterLayoutManager *layout;

  g_return_val_if_fail (ST_IS_BOX_LAYOUT (box),
                        CLUTTER_ORIENTATION_HORIZONTAL);

  layout = clutter_actor_get_layout_manager (CLUTTER_ACTOR (box));
  return clutter_box_layout_get_orientation (CLUTTER_BOX_LAYOUT (layout));
}

/**
 * st_box_layout_set_vertical:
 * @box: A #StBoxLayout
 * @vertical: %TRUE if the layout should be vertical
 *
 * Set the value of the #StBoxLayout:vertical property
 */
void
st_box_layout_set_vertical (StBoxLayout *box,
                            gboolean     vertical)
{
  ClutterOrientation orientation;

  g_return_if_fail (ST_IS_BOX_LAYOUT (box));

  orientation = vertical ? CLUTTER_ORIENTATION_VERTICAL
                         : CLUTTER_ORIENTATION_HORIZONTAL;
  st_box_layout_set_orientation (box, orientation);
}

/**
 * st_box_layout_get_vertical:
 * @box: A #StBoxLayout
 *
 * Get the value of the #StBoxLayout:vertical property.
 *
 * Returns: %TRUE if the layout is vertical
 */
gboolean
st_box_layout_get_vertical (StBoxLayout *box)
{
  ClutterOrientation orientation;

  g_return_val_if_fail (ST_IS_BOX_LAYOUT (box), FALSE);

  orientation = st_box_layout_get_orientation (box);
  return orientation == CLUTTER_ORIENTATION_VERTICAL;
}
