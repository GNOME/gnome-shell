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

  CoglOnscreen *onscreen;
};

static ClutterStageWindowIface *clutter_stage_window_parent_iface = NULL;

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaStageNative, meta_stage_native,
                         CLUTTER_TYPE_STAGE_COGL,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init))
static gboolean
meta_stage_native_realize (ClutterStageWindow *stage_window)
{
  MetaStageNative *stage_native = META_STAGE_NATIVE (stage_window);
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);
  ClutterBackend *clutter_backend = CLUTTER_BACKEND (stage_cogl->backend);
  GError *error = NULL;

  stage_native->onscreen = cogl_onscreen_new (clutter_backend->cogl_context,
                                              1, 1);

  if (!cogl_framebuffer_allocate (COGL_FRAMEBUFFER (stage_native->onscreen),
                                  &error))
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

  g_clear_pointer (&stage_native->onscreen, cogl_object_unref);
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
  MetaStageNative *stage_native = META_STAGE_NATIVE (stage_window);
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (stage_native->onscreen);

  if (framebuffer)
    {
      *geometry = (cairo_rectangle_int_t) {
        .width = cogl_framebuffer_get_width (framebuffer),
        .height = cogl_framebuffer_get_height (framebuffer)
      };
    }
  else
    {
      *geometry = (cairo_rectangle_int_t) {
        .width = 1,
        .height = 1,
      };
    }
}

static CoglFramebuffer *
meta_stage_native_get_legacy_onscreen (ClutterStageWindow *stage_window)
{
  MetaStageNative *stage_native = META_STAGE_NATIVE (stage_window);

  return COGL_FRAMEBUFFER (stage_native->onscreen);
}

static CoglClosure *
meta_stage_native_set_frame_callback (ClutterStageWindow *stage_window,
                                      CoglFrameCallback   callback,
                                      gpointer            user_data)
{
  MetaStageNative *stage_native = META_STAGE_NATIVE (stage_window);

  cogl_onscreen_set_swap_throttled (stage_native->onscreen,
                                    _clutter_get_sync_to_vblank ());

  return cogl_onscreen_add_frame_callback (stage_native->onscreen,
                                           callback,
                                           user_data,
                                           NULL);
}

static void
meta_stage_native_remove_frame_callback (ClutterStageWindow *stage_window,
                                         CoglFrameClosure   *closure)
{
  MetaStageNative *stage_native = META_STAGE_NATIVE (stage_window);

  cogl_onscreen_remove_frame_callback (stage_native->onscreen, closure);
}

static int64_t
meta_stage_native_get_frame_counter (ClutterStageWindow *stage_window)
{
  MetaStageNative *stage_native = META_STAGE_NATIVE (stage_window);

  return cogl_onscreen_get_frame_counter (stage_native->onscreen);
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
  iface->get_legacy_onscreen = meta_stage_native_get_legacy_onscreen;
  iface->set_frame_callback = meta_stage_native_set_frame_callback;
  iface->remove_frame_callback = meta_stage_native_remove_frame_callback;
  iface->get_frame_counter = meta_stage_native_get_frame_counter;
}
