/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2016 Red Hat Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jonas Ådahl <jadahl@gmail.com>
 */

#include "config.h"

#include "backends/x11/meta-stage-x11-nested.h"

#include "backends/meta-backend-private.h"
#include "backends/meta-logical-monitor.h"
#include "backends/meta-monitor.h"
#include "backends/meta-output.h"
#include "backends/meta-renderer.h"
#include "backends/x11/nested/meta-renderer-x11-nested.h"
#include "clutter/clutter-mutter.h"

static ClutterStageWindowIface *clutter_stage_window_parent_iface = NULL;

struct _MetaStageX11Nested
{
  ClutterStageX11 parent;

  CoglPipeline *pipeline;
};

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaStageX11Nested, meta_stage_x11_nested,
                         CLUTTER_TYPE_STAGE_X11,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init))

typedef struct _ClutterStageX11View
{
  CoglTexture *texture;
  ClutterStageViewCogl *view;
} MetaStageX11NestedView;

static void
meta_stage_x11_nested_resize (ClutterStageWindow *stage_window,
                              gint                width,
                              gint                height)
{
  if (!meta_is_stage_views_enabled ())
    {
      MetaBackend *backend = meta_get_backend ();
      MetaRenderer *renderer = meta_backend_get_renderer (backend);
      MetaRendererX11Nested *renderer_x11_nested =
        META_RENDERER_X11_NESTED (renderer);

      meta_renderer_x11_nested_ensure_legacy_view (renderer_x11_nested,
                                                   width, height);
    }

  clutter_stage_window_parent_iface->resize (stage_window, width, height);
}

static gboolean
meta_stage_x11_nested_can_clip_redraws (ClutterStageWindow *stage_window)
{
  return FALSE;
}

static GList *
meta_stage_x11_nested_get_views (ClutterStageWindow *stage_window)
{
  MetaBackend *backend = meta_get_backend ();
  MetaRenderer *renderer = meta_backend_get_renderer (backend);

  return meta_renderer_get_views (renderer);
}

typedef struct
{
  MetaStageX11Nested *stage_nested;
  CoglTexture *texture;
  ClutterStageView *view;
  MetaLogicalMonitor *logical_monitor;
} DrawCrtcData;

static gboolean
draw_crtc (MetaMonitor         *monitor,
           MetaMonitorMode     *monitor_mode,
           MetaMonitorCrtcMode *monitor_crtc_mode,
           gpointer             user_data,
           GError             **error)
{
  DrawCrtcData *data = user_data;
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (data->stage_nested);
  CoglFramebuffer *onscreen = COGL_FRAMEBUFFER (stage_x11->onscreen);
  CoglTexture *texture = data->texture;
  MetaLogicalMonitor *logical_monitor = data->logical_monitor;
  MetaOutput *output = monitor_crtc_mode->output;
  MetaCrtc *crtc = output->crtc;
  MetaRendererView *renderer_view = META_RENDERER_VIEW (data->view);
  MetaMonitorTransform view_transform;
  MetaMonitorTransform layout_transform = META_MONITOR_TRANSFORM_NORMAL;
  cairo_rectangle_int_t view_layout;
  CoglMatrix projection_matrix;
  CoglMatrix transform;
  float texture_width, texture_height;
  float sample_x, sample_y, sample_width, sample_height;
  int viewport_x, viewport_y;
  int viewport_width, viewport_height;
  float s_1, t_1, s_2, t_2;

  texture_width = cogl_texture_get_width (texture);
  texture_height = cogl_texture_get_height (texture);

  clutter_stage_view_get_layout (data->view, &view_layout);
  sample_x = crtc->rect.x - view_layout.x;
  sample_y = crtc->rect.y - view_layout.y;
  sample_width = crtc->rect.width;
  sample_height = crtc->rect.height;

  clutter_stage_view_get_offscreen_transformation_matrix (data->view,
                                                          &transform);

  cogl_framebuffer_push_matrix (onscreen);
  cogl_matrix_init_identity (&projection_matrix);
  cogl_matrix_translate (&projection_matrix, -1, 1, 0);
  cogl_matrix_scale (&projection_matrix, 2, -2, 0);

  cogl_matrix_multiply (&projection_matrix, &projection_matrix, &transform);
  cogl_framebuffer_set_projection_matrix (onscreen, &projection_matrix);

  s_1 = sample_x / texture_width;
  t_1 = sample_y / texture_height;
  s_2 = (sample_x + sample_width) / texture_width;
  t_2 = (sample_y + sample_height) / texture_height;

  view_transform = meta_renderer_view_get_transform (renderer_view);

  if (view_transform == logical_monitor->transform)
    {
      switch (view_transform)
        {
        case META_MONITOR_TRANSFORM_NORMAL:
        case META_MONITOR_TRANSFORM_FLIPPED:
          layout_transform = META_MONITOR_TRANSFORM_NORMAL;
          break;
        case META_MONITOR_TRANSFORM_270:
        case META_MONITOR_TRANSFORM_FLIPPED_270:
          layout_transform = META_MONITOR_TRANSFORM_90;
          break;
        case META_MONITOR_TRANSFORM_180:
        case META_MONITOR_TRANSFORM_FLIPPED_180:
          layout_transform = META_MONITOR_TRANSFORM_180;
          break;
        case META_MONITOR_TRANSFORM_90:
        case META_MONITOR_TRANSFORM_FLIPPED_90:
          layout_transform = META_MONITOR_TRANSFORM_270;
          break;
        }
    }
  else
    {
      layout_transform = logical_monitor->transform;
    }

  meta_monitor_calculate_crtc_pos (monitor, monitor_mode, output,
                                   layout_transform,
                                   &viewport_x,
                                   &viewport_y);
  viewport_x += logical_monitor->rect.x;
  viewport_y += logical_monitor->rect.y;
  if (meta_monitor_transform_is_rotated (logical_monitor->transform))
    {
      viewport_width = monitor_crtc_mode->crtc_mode->height;
      viewport_height = monitor_crtc_mode->crtc_mode->width;
    }
  else
    {
      viewport_width = monitor_crtc_mode->crtc_mode->width;
      viewport_height = monitor_crtc_mode->crtc_mode->height;
    }
  cogl_framebuffer_set_viewport (onscreen,
                                 viewport_x, viewport_y,
                                 viewport_width, viewport_height);

  cogl_framebuffer_draw_textured_rectangle (onscreen,
                                            data->stage_nested->pipeline,
                                            0, 0, 1, 1,
                                            s_1, t_1, s_2, t_2);

  cogl_framebuffer_pop_matrix (onscreen);
  return TRUE;
}

static void
draw_logical_monitor (MetaStageX11Nested    *stage_nested,
                      MetaLogicalMonitor    *logical_monitor,
                      CoglTexture           *texture,
                      ClutterStageView      *view,
                      cairo_rectangle_int_t *view_layout)
{
  MetaMonitor *monitor;
  MetaMonitorMode *current_mode;

  cogl_pipeline_set_layer_wrap_mode (stage_nested->pipeline, 0,
                                     COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);

  monitor = meta_logical_monitor_get_monitors (logical_monitor)->data;
  current_mode = meta_monitor_get_current_mode (monitor);
  meta_monitor_mode_foreach_crtc (monitor, current_mode,
                                  draw_crtc,
                                  &(DrawCrtcData) {
                                    .stage_nested = stage_nested,
                                    .texture = texture,
                                    .view = view,
                                    .logical_monitor = logical_monitor
                                  },
                                  NULL);
}

static void
meta_stage_x11_nested_finish_frame (ClutterStageWindow *stage_window)
{
  MetaStageX11Nested *stage_nested = META_STAGE_X11_NESTED (stage_window);
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  MetaBackend *backend = meta_get_backend ();
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  CoglFramebuffer *onscreen = COGL_FRAMEBUFFER (stage_x11->onscreen);
  GList *l;

  if (!stage_nested->pipeline)
    stage_nested->pipeline = cogl_pipeline_new (clutter_backend->cogl_context);

  cogl_framebuffer_clear4f (onscreen,
                            COGL_BUFFER_BIT_COLOR,
                            0.0f, 0.0f, 0.0f, 1.0f);

  for (l = meta_renderer_get_views (renderer); l; l = l->next)
    {
      ClutterStageView *view = l->data;
      MetaRendererView *renderer_view = META_RENDERER_VIEW (view);
      MetaLogicalMonitor *logical_monitor;
      cairo_rectangle_int_t view_layout;
      CoglFramebuffer *framebuffer;
      CoglTexture *texture;

      clutter_stage_view_get_layout (view, &view_layout);

      framebuffer = clutter_stage_view_get_onscreen (view);
      texture = cogl_offscreen_get_texture (COGL_OFFSCREEN (framebuffer));

      cogl_pipeline_set_layer_texture (stage_nested->pipeline, 0, texture);

      logical_monitor = meta_renderer_view_get_logical_monitor (renderer_view);
      if (logical_monitor)
        {
          draw_logical_monitor (stage_nested, logical_monitor, texture, view, &view_layout);
        }
      else
        {
          MetaMonitorManager *monitor_manager =
            meta_backend_get_monitor_manager (backend);
          GList *logical_monitors;
          GList *k;

          logical_monitors =
            meta_monitor_manager_get_logical_monitors (monitor_manager);
          for (k = logical_monitors; k; k = k->next)
            {
              logical_monitor = k->data;

              draw_logical_monitor (stage_nested, logical_monitor, texture, view, &view_layout);
            }
        }
    }

  cogl_onscreen_swap_buffers (stage_x11->onscreen);
}

static void
meta_stage_x11_nested_unrealize (ClutterStageWindow *stage_window)
{
  MetaStageX11Nested *stage_nested = META_STAGE_X11_NESTED (stage_window);
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);
  MetaBackend *backend = meta_get_backend ();
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  GList *l;

  /* Clutter still uses part of the deprecated stateful API of Cogl
   * (in particulart cogl_set_framebuffer). It means Cogl can keep an
   * internal reference to the onscreen object we rendered to. In the
   * case of foreign window, we want to avoid this, as we don't know
   * what's going to happen to that window.
   *
   * The following call sets the current Cogl framebuffer to a dummy
   * 1x1 one if we're unrealizing the current one, so Cogl doesn't
   * keep any reference to the foreign window.
   */
  for (l = meta_renderer_get_views (renderer); l ;l = l->next)
    {
      ClutterStageView *view = l->data;
      CoglFramebuffer *framebuffer = clutter_stage_view_get_framebuffer (view);

      if (cogl_get_draw_framebuffer () == framebuffer)
        {
          _clutter_backend_reset_cogl_framebuffer (stage_cogl->backend);
          break;
        }
    }

  g_clear_pointer (&stage_nested->pipeline, cogl_object_unref);

  clutter_stage_window_parent_iface->unrealize (stage_window);
}

static void
meta_stage_x11_nested_init (MetaStageX11Nested *stage_x11_nested)
{
}

static void
meta_stage_x11_nested_class_init (MetaStageX11NestedClass *klass)
{
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  clutter_stage_window_parent_iface = g_type_interface_peek_parent (iface);

  iface->resize = meta_stage_x11_nested_resize;
  iface->can_clip_redraws = meta_stage_x11_nested_can_clip_redraws;
  iface->unrealize = meta_stage_x11_nested_unrealize;
  iface->get_views = meta_stage_x11_nested_get_views;
  iface->finish_frame = meta_stage_x11_nested_finish_frame;
}
