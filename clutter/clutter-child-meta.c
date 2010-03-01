/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Jorn Baayen  <jorn@openedhand.com>
 *             Emmanuele Bassi  <ebassi@openedhand.com>
 *             Tomas Frydrych <tf@openedhand.com>
 *             Øyvind Kolås <ok@openedhand.com>
 *
 * Copyright (C) 2008 OpenedHand
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:clutter-child-meta
 * @short_description: Wrapper for actors inside a container
 *
 * #ClutterChildMeta is a wrapper object created by #ClutterContainer
 * implementations in order to store child-specific data and properties.
 *
 * A #ClutterChildMeta wraps a #ClutterActor inside a #ClutterContainer.
 *
 * #ClutterChildMeta is available since Clutter 0.8
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-child-meta.h"
#include "clutter-container.h"
#include "clutter-debug.h"
#include "clutter-private.h"

G_DEFINE_ABSTRACT_TYPE (ClutterChildMeta, clutter_child_meta, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_CONTAINER,
  PROP_ACTOR
};

static void
clutter_child_meta_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ClutterChildMeta *child_meta = CLUTTER_CHILD_META (object);

  switch (prop_id) 
    {
    case PROP_CONTAINER:
      child_meta->container = g_value_get_object (value);
      break;

    case PROP_ACTOR:
      child_meta->actor = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
clutter_child_meta_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ClutterChildMeta *child_meta = CLUTTER_CHILD_META (object);

  switch (prop_id) 
    {
    case PROP_CONTAINER:
      g_value_set_object (value, child_meta->container);
      break;

    case PROP_ACTOR:
      g_value_set_object (value, child_meta->actor);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_child_meta_class_init (ClutterChildMetaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->set_property = clutter_child_meta_set_property;
  gobject_class->get_property = clutter_child_meta_get_property;

  /**
   * ClutterChildMeta:container:
   *
   * The #ClutterContainer that created this #ClutterChildMeta.
   *
   * Since: 0.8
   */
  pspec = g_param_spec_object ("container",
                               "Container",
                               "The container that created this data",
                               CLUTTER_TYPE_CONTAINER,
                               G_PARAM_CONSTRUCT_ONLY |
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_CONTAINER, pspec);

  /**
   * ClutterChildMeta:actor:
   *
   * The #ClutterActor being wrapped by this #ClutterChildMeta
   *
   * Since: 0.8
   */
  pspec = g_param_spec_object ("actor",
                               "Actor",
                               "The actor wrapped by this data",
                               CLUTTER_TYPE_ACTOR,
                               G_PARAM_CONSTRUCT_ONLY |
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_ACTOR, pspec);
}

static void
clutter_child_meta_init (ClutterChildMeta *self)
{
}

/**
 * clutter_child_meta_get_container:
 * @data: a #ClutterChildMeta
 *
 * Retrieves the container using @data
 *
 * Return value: (transfer none): a #ClutterContainer
 *
 * Since: 0.8
 */
ClutterContainer *
clutter_child_meta_get_container (ClutterChildMeta *data)
{
  g_return_val_if_fail (CLUTTER_IS_CHILD_META (data), NULL);

  return data->container;
}

/**
 * clutter_child_meta_get_actor:
 * @data: a #ClutterChildMeta
 *
 * Retrieves the actor wrapped by @data
 *
 * Return value: (transfer none): a #ClutterActor
 *
 * Since: 0.8
 */
ClutterActor *
clutter_child_meta_get_actor (ClutterChildMeta *data)
{
  g_return_val_if_fail (CLUTTER_IS_CHILD_META (data), NULL);

  return data->actor;
}
