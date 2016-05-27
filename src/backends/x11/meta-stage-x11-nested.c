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
#include "backends/meta-renderer.h"
#include "clutter/clutter-mutter.h"

static ClutterStageWindowIface *clutter_stage_window_parent_iface = NULL;

struct _MetaStageX11Nested
{
  ClutterStageX11 parent;

  CoglPipeline *pipeline;
  GList *views;
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
      cairo_rectangle_int_t view_layout;
      CoglFramebuffer *framebuffer;
      CoglTexture *texture;

      clutter_stage_view_get_layout (view, &view_layout);

      framebuffer = clutter_stage_view_get_framebuffer (view);
      texture = cogl_offscreen_get_texture (COGL_OFFSCREEN (framebuffer));
      cogl_framebuffer_set_viewport (onscreen,
                                     view_layout.x,
                                     view_layout.y,
                                     view_layout.width,
                                     view_layout.height);
      cogl_pipeline_set_layer_texture (stage_nested->pipeline, 0, texture);
      cogl_framebuffer_draw_rectangle (onscreen,
                                       stage_nested->pipeline,
                                       -1, 1, 1, -1);
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

  iface->can_clip_redraws = meta_stage_x11_nested_can_clip_redraws;
  iface->unrealize = meta_stage_x11_nested_unrealize;
  iface->get_views = meta_stage_x11_nested_get_views;
  iface->finish_frame = meta_stage_x11_nested_finish_frame;
}

