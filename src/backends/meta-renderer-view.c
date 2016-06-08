/*
 * Copyright (C) 2016 Red Hat Inc.
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

#include "config.h"

#include "backends/meta-renderer-view.h"

#include "backends/meta-renderer.h"
#include "clutter/clutter-mutter.h"

enum
{
  PROP_0,

  PROP_MONITOR_INFO,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

struct _MetaRendererView
{
  ClutterStageView parent;

  MetaMonitorInfo *monitor_info;
};

G_DEFINE_TYPE (MetaRendererView, meta_renderer_view,
               CLUTTER_TYPE_STAGE_VIEW_COGL)

MetaMonitorInfo *
meta_renderer_view_get_monitor_info (MetaRendererView *view)
{
  return view->monitor_info;
}

static void
meta_renderer_view_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  MetaRendererView *view = META_RENDERER_VIEW (object);

  switch (prop_id)
    {
    case PROP_MONITOR_INFO:
      g_value_set_pointer (value, view->monitor_info);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_renderer_view_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  MetaRendererView *view = META_RENDERER_VIEW (object);

  switch (prop_id)
    {
    case PROP_MONITOR_INFO:
      view->monitor_info = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_renderer_view_init (MetaRendererView *view)
{
}

static void
meta_renderer_view_class_init (MetaRendererViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = meta_renderer_view_get_property;
  object_class->set_property = meta_renderer_view_set_property;

  obj_props[PROP_MONITOR_INFO] =
    g_param_spec_pointer ("monitor-info",
                          "MetaMonitorInfo",
                          "The monitor info of the view",
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}
