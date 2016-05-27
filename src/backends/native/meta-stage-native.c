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

#include "backends/native/meta-stage-native.h"

#include "backends/meta-backend-private.h"
#include "meta/meta-backend.h"
#include "meta/meta-monitor-manager.h"
#include "meta/util.h"

struct _MetaStageNative
{
  ClutterStageCogl parent;

  CoglOnscreen *pending_onscreen;
};

static ClutterStageWindowIface *clutter_stage_window_parent_iface = NULL;

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaStageNative, meta_stage_native,
                         CLUTTER_TYPE_STAGE_COGL,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init))

static MetaRendererView *
get_legacy_view (MetaRenderer *renderer)
{
  GList *views;

  views = meta_renderer_get_views (renderer);
  g_assert (g_list_length (views) <= 1);

  if (views)
    return views->data;
  else
    return NULL;
}

static CoglOnscreen *
get_legacy_onscreen (MetaStageNative *stage_native)
{
  if (stage_native->pending_onscreen)
    {
      return stage_native->pending_onscreen;
    }
  else
    {
      MetaBackend *backend = meta_get_backend ();
      MetaRenderer *renderer = meta_backend_get_renderer (backend);
      ClutterStageView *stage_view;
      CoglFramebuffer *framebuffer;

      stage_view = CLUTTER_STAGE_VIEW (get_legacy_view (renderer));
      framebuffer = clutter_stage_view_get_framebuffer (stage_view);

      return COGL_ONSCREEN (framebuffer);
    }
}

void
meta_stage_native_legacy_set_size (MetaStageNative *stage_native,
                                   int              width,
                                   int              height)
{
  MetaBackend *backend = meta_get_backend ();
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  MetaRendererView *legacy_view;
  cairo_rectangle_int_t view_layout;

  legacy_view = get_legacy_view (renderer);
  if (!legacy_view)
    return;

  view_layout = (cairo_rectangle_int_t) {
    .width = width,
    .height = height
  };
  g_object_set (G_OBJECT (legacy_view),
                "layout", &view_layout,
                NULL);
}

static gboolean
meta_stage_native_realize (ClutterStageWindow *stage_window)
{
  MetaStageNative *stage_native = META_STAGE_NATIVE (stage_window);
  MetaBackend *backend = meta_get_backend ();
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  CoglFramebuffer *framebuffer;
  GError *error = NULL;

  stage_native->pending_onscreen =
    cogl_onscreen_new (clutter_backend->cogl_context, 1, 1);

  framebuffer = COGL_FRAMEBUFFER (stage_native->pending_onscreen);
  if (!cogl_framebuffer_allocate (framebuffer, &error))
    meta_fatal ("Failed to allocate onscreen framebuffer: %s\n",
                error->message);

  if (!(clutter_stage_window_parent_iface->realize (stage_window)))
    meta_fatal ("Failed to realize native stage window");

  return TRUE;
}

static void
meta_stage_native_unrealize (ClutterStageWindow *stage_window)
{
  MetaStageNative *stage_native = META_STAGE_NATIVE (stage_window);

  clutter_stage_window_parent_iface->unrealize (stage_window);

  g_clear_pointer (&stage_native->pending_onscreen, cogl_object_unref);
}

static gboolean
meta_stage_native_can_clip_redraws (ClutterStageWindow *stage_window)
{
  return TRUE;
}

static void
meta_stage_native_get_geometry (ClutterStageWindow    *stage_window,
                                cairo_rectangle_int_t *geometry)
{
  MetaBackend *backend = meta_get_backend ();
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  MetaRendererView *legacy_view;

  legacy_view = get_legacy_view (renderer);
  if (legacy_view)
    {
      clutter_stage_view_get_layout (CLUTTER_STAGE_VIEW (legacy_view),
                                     geometry);
    }
  else
    {
      *geometry = (cairo_rectangle_int_t) {
        .width = 1,
        .height = 1,
      };
    }
}

static void
ensure_legacy_view (ClutterStageWindow *stage_window)
{
  MetaStageNative *stage_native = META_STAGE_NATIVE (stage_window);
  MetaBackend *backend = meta_get_backend ();
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaRendererView *legacy_view;
  cairo_rectangle_int_t view_layout = { 0 };
  CoglFramebuffer *framebuffer;

  legacy_view = get_legacy_view (renderer);
  if (legacy_view)
    return;

  if (!monitor_manager)
    return;

  meta_monitor_manager_get_screen_size (monitor_manager,
                                        &view_layout.width,
                                        &view_layout.height);
  framebuffer = g_steal_pointer (&stage_native->pending_onscreen);
  legacy_view = g_object_new (META_TYPE_RENDERER_VIEW,
                              "layout", &view_layout,
                              "framebuffer", framebuffer,
                              NULL);
  meta_renderer_set_legacy_view (renderer, legacy_view);
}

static GList *
meta_stage_native_get_views (ClutterStageWindow *stage_window)
{
  MetaBackend *backend = meta_get_backend ();
  MetaRenderer *renderer = meta_backend_get_renderer (backend);

  ensure_legacy_view (stage_window);
  return meta_renderer_get_views (renderer);
}

static CoglClosure *
meta_stage_native_set_frame_callback (ClutterStageWindow *stage_window,
                                      CoglFrameCallback   callback,
                                      gpointer            user_data)
{
  MetaStageNative *stage_native = META_STAGE_NATIVE (stage_window);
  CoglOnscreen *legacy_onscreen;

  legacy_onscreen = get_legacy_onscreen (stage_native);
  cogl_onscreen_set_swap_throttled (legacy_onscreen,
                                    _clutter_get_sync_to_vblank ());

  return cogl_onscreen_add_frame_callback (legacy_onscreen,
                                           callback,
                                           user_data,
                                           NULL);
}

static void
meta_stage_native_remove_frame_callback (ClutterStageWindow *stage_window,
                                         CoglFrameClosure   *closure)
{
  MetaStageNative *stage_native = META_STAGE_NATIVE (stage_window);
  CoglOnscreen *legacy_onscreen;

  legacy_onscreen = get_legacy_onscreen (stage_native);

  cogl_onscreen_remove_frame_callback (legacy_onscreen, closure);
}

static int64_t
meta_stage_native_get_frame_counter (ClutterStageWindow *stage_window)
{
  MetaStageNative *stage_native = META_STAGE_NATIVE (stage_window);
  CoglOnscreen *legacy_onscreen;

  legacy_onscreen = get_legacy_onscreen (stage_native);

  return cogl_onscreen_get_frame_counter (legacy_onscreen);
}

static void
meta_stage_native_init (MetaStageNative *stage_native)
{
}

static void
meta_stage_native_class_init (MetaStageNativeClass *klass)
{
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  clutter_stage_window_parent_iface = g_type_interface_peek_parent (iface);

  iface->realize = meta_stage_native_realize;
  iface->unrealize = meta_stage_native_unrealize;
  iface->can_clip_redraws = meta_stage_native_can_clip_redraws;
  iface->get_geometry = meta_stage_native_get_geometry;
  iface->get_views = meta_stage_native_get_views;
  iface->set_frame_callback = meta_stage_native_set_frame_callback;
  iface->remove_frame_callback = meta_stage_native_remove_frame_callback;
  iface->get_frame_counter = meta_stage_native_get_frame_counter;
}
