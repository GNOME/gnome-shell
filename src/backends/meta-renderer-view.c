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
  PROP_TRANSFORM,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

struct _MetaRendererView
{
  ClutterStageViewCogl parent;

  MetaMonitorTransform transform;
  MetaLogicalMonitor *logical_monitor;
};

G_DEFINE_TYPE (MetaRendererView, meta_renderer_view,
               CLUTTER_TYPE_STAGE_VIEW_COGL)

MetaLogicalMonitor *
meta_renderer_view_get_logical_monitor (MetaRendererView *view)
{
  return view->logical_monitor;
}

MetaMonitorTransform
meta_renderer_view_get_transform (MetaRendererView *view)
{
  return view->transform;
}

static void
meta_renderer_view_get_offscreen_transformation_matrix (ClutterStageView *view,
                                                        CoglMatrix       *matrix)
{
  MetaRendererView *renderer_view = META_RENDERER_VIEW (view);

  cogl_matrix_init_identity (matrix);

  switch (renderer_view->transform)
    {
    case META_MONITOR_TRANSFORM_NORMAL:
      break;
    case META_MONITOR_TRANSFORM_90:
      cogl_matrix_rotate (matrix, 90, 0, 0, 1);
      cogl_matrix_translate (matrix, 0, -1, 0);
      break;
    case META_MONITOR_TRANSFORM_180:
      cogl_matrix_rotate (matrix, 180, 0, 0, 1);
      cogl_matrix_translate (matrix, -1, -1, 0);
      break;
    case META_MONITOR_TRANSFORM_270:
      cogl_matrix_rotate (matrix, 270, 0, 0, 1);
      cogl_matrix_translate (matrix, -1, 0, 0);
      break;
    case META_MONITOR_TRANSFORM_FLIPPED:
      cogl_matrix_scale (matrix, -1, 1, 1);
      cogl_matrix_translate (matrix, -1, 0, 0);
      break;
    case META_MONITOR_TRANSFORM_FLIPPED_90:
      cogl_matrix_scale (matrix, -1, 1, 1);
      cogl_matrix_rotate (matrix, 90, 0, 0, 1);
      break;
    case META_MONITOR_TRANSFORM_FLIPPED_180:
      cogl_matrix_scale (matrix, -1, 1, 1);
      cogl_matrix_rotate (matrix, 180, 0, 0, 1);
      cogl_matrix_translate (matrix, 0, -1, 0);
      break;
    case META_MONITOR_TRANSFORM_FLIPPED_270:
      cogl_matrix_scale (matrix, -1, 1, 1);
      cogl_matrix_rotate (matrix, 270, 0, 0, 1);
      cogl_matrix_translate (matrix, -1, -1, 0);
      break;
    }
}

static void
meta_renderer_view_setup_offscreen_blit_pipeline (ClutterStageView *view,
                                                  CoglPipeline     *pipeline)
{
  CoglMatrix matrix;

  meta_renderer_view_get_offscreen_transformation_matrix (view, &matrix);
  cogl_pipeline_set_layer_matrix (pipeline, 0, &matrix);
}

static void
meta_renderer_view_set_transform (MetaRendererView     *view,
                                  MetaMonitorTransform  transform)
{
  if (view->transform == transform)
    return;

  view->transform = transform;
  clutter_stage_view_invalidate_offscreen_blit_pipeline (CLUTTER_STAGE_VIEW (view));
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
      g_value_set_pointer (value, view->logical_monitor);
      break;
    case PROP_TRANSFORM:
      g_value_set_uint (value, view->transform);
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
      view->logical_monitor = g_value_get_pointer (value);
      break;
    case PROP_TRANSFORM:
      meta_renderer_view_set_transform (view, g_value_get_uint (value));
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
  ClutterStageViewClass *view_class = CLUTTER_STAGE_VIEW_CLASS (klass);

  view_class->setup_offscreen_blit_pipeline =
    meta_renderer_view_setup_offscreen_blit_pipeline;
  view_class->get_offscreen_transformation_matrix =
    meta_renderer_view_get_offscreen_transformation_matrix;

  object_class->get_property = meta_renderer_view_get_property;
  object_class->set_property = meta_renderer_view_set_property;

  obj_props[PROP_MONITOR_INFO] =
    g_param_spec_pointer ("logical-monitor",
                          "MetaLogicalMonitor",
                          "The logical monitor of the view",
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_CONSTRUCT_ONLY);
  obj_props[PROP_TRANSFORM] =
    g_param_spec_uint ("transform",
                       "Transform",
                       "Transform to apply to the view",
                       META_MONITOR_TRANSFORM_NORMAL,
                       META_MONITOR_TRANSFORM_FLIPPED_270,
                       META_MONITOR_TRANSFORM_NORMAL,
                       G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT_ONLY |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}
