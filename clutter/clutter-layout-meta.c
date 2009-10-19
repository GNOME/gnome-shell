/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009  Intel Corporation.
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
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-layout-meta
 * @short_description: Wrapper for actors inside a layout manager
 *
 * #ClutterLayoutMeta is a wrapper object created by #ClutterLayoutManager
 * implementations in order to store child-specific data and properties.
 *
 * A #ClutterLayoutMeta wraps a #ClutterActor inside a #ClutterContainer
 * using a #ClutterLayoutManager.
 *
 * #ClutterLayoutMeta is available since Clutter 1.2
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-layout-meta.h"
#include "clutter-debug.h"
#include "clutter-private.h"

G_DEFINE_ABSTRACT_TYPE (ClutterLayoutMeta,
                        clutter_layout_meta,
                        CLUTTER_TYPE_CHILD_META);

enum
{
  PROP_0,

  PROP_MANAGER
};

static void
clutter_layout_meta_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ClutterLayoutMeta *layout_meta = CLUTTER_LAYOUT_META (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      layout_meta->manager = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_layout_meta_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ClutterLayoutMeta *layout_meta = CLUTTER_LAYOUT_META (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      g_value_set_object (value, layout_meta->manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_layout_meta_class_init (ClutterLayoutMetaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->set_property = clutter_layout_meta_set_property;
  gobject_class->get_property = clutter_layout_meta_get_property;

  /**
   * ClutterLayoutMeta:manager:
   *
   * The #ClutterLayoutManager that created this #ClutterLayoutMeta.
   *
   * Since: 1.2
   */
  pspec = g_param_spec_object ("manager",
                               "Manager",
                               "The manager that created this data",
                               CLUTTER_TYPE_LAYOUT_MANAGER,
                               G_PARAM_CONSTRUCT_ONLY |
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_MANAGER, pspec);
}

static void
clutter_layout_meta_init (ClutterLayoutMeta *self)
{
}

/**
 * clutter_layout_meta_get_manager:
 * @data: a #ClutterLayoutMeta
 *
 * Retrieves the actor wrapped by @data
 *
 * Return value: (transfer none): a #ClutterLayoutManager
 *
 * Since: 1.2
 */
ClutterLayoutManager *
clutter_layout_meta_get_manager (ClutterLayoutMeta *data)
{
  g_return_val_if_fail (CLUTTER_IS_LAYOUT_META (data), NULL);

  return data->manager;
}
