/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat
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
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include "meta-cursor-renderer-native.h"

#include <gbm.h>

#include "meta-cursor-private.h"
#include "meta-monitor-manager.h"

struct _MetaCursorRendererNativePrivate
{
  gboolean has_hw_cursor;

  int drm_fd;
  struct gbm_device *gbm;
};
typedef struct _MetaCursorRendererNativePrivate MetaCursorRendererNativePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaCursorRendererNative, meta_cursor_renderer_native, META_TYPE_CURSOR_RENDERER);

static void
meta_cursor_renderer_native_finalize (GObject *object)
{
  MetaCursorRendererNative *renderer = META_CURSOR_RENDERER_NATIVE (object);
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (renderer);

  if (priv->gbm)
    gbm_device_destroy (priv->gbm);

  G_OBJECT_CLASS (meta_cursor_renderer_native_parent_class)->finalize (object);
}

static void
set_crtc_cursor (MetaCursorRendererNative *native,
                 MetaCRTC                 *crtc,
                 MetaCursorReference      *cursor,
                 gboolean                  force)
{
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (native);

  if (crtc->cursor == cursor && !force)
    return;

  crtc->cursor = cursor;

  if (cursor)
    {
      struct gbm_bo *bo;
      union gbm_bo_handle handle;
      int width, height;
      int hot_x, hot_y;

      bo = meta_cursor_reference_get_gbm_bo (cursor, &hot_x, &hot_y);

      handle = gbm_bo_get_handle (bo);
      width = gbm_bo_get_width (bo);
      height = gbm_bo_get_height (bo);

      drmModeSetCursor2 (priv->drm_fd, crtc->crtc_id, handle.u32,
                         width, height, hot_x, hot_y);
    }
  else
    {
      drmModeSetCursor2 (priv->drm_fd, crtc->crtc_id, 0, 0, 0, 0, 0);
    }
}

static void
update_hw_cursor (MetaCursorRendererNative *native,
                  gboolean                  force)
{
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (native);
  MetaCursorRenderer *renderer = META_CURSOR_RENDERER (native);
  const MetaRectangle *cursor_rect = meta_cursor_renderer_get_rect (renderer);
  MetaCursorReference *cursor = meta_cursor_renderer_get_cursor (renderer);
  MetaMonitorManager *monitors;
  MetaCRTC *crtcs;
  unsigned int i, n_crtcs;

  monitors = meta_monitor_manager_get ();
  meta_monitor_manager_get_resources (monitors, NULL, NULL, &crtcs, &n_crtcs, NULL, NULL);

  for (i = 0; i < n_crtcs; i++)
    {
      gboolean crtc_should_have_cursor;
      MetaCursorReference *crtc_cursor;
      MetaRectangle *crtc_rect;

      crtc_rect = &crtcs[i].rect;

      crtc_should_have_cursor = (priv->has_hw_cursor && meta_rectangle_overlap (cursor_rect, crtc_rect));
      if (crtc_should_have_cursor)
        crtc_cursor = cursor;
      else
        crtc_cursor = NULL;

      set_crtc_cursor (native, &crtcs[i], crtc_cursor, force);

      if (crtc_cursor)
        {
          drmModeMoveCursor (priv->drm_fd, crtcs[i].crtc_id,
                             cursor_rect->x - crtc_rect->x,
                             cursor_rect->y - crtc_rect->y);
        }
    }
}

static gboolean
should_have_hw_cursor (MetaCursorRenderer *renderer)
{
  MetaCursorReference *cursor = meta_cursor_renderer_get_cursor (renderer);

  if (cursor)
    return (meta_cursor_reference_get_gbm_bo (cursor, NULL, NULL) != NULL);
  else
    return FALSE;
}

static gboolean
meta_cursor_renderer_native_update_cursor (MetaCursorRenderer *renderer)
{
  MetaCursorRendererNative *native = META_CURSOR_RENDERER_NATIVE (renderer);
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (native);

  priv->has_hw_cursor = should_have_hw_cursor (renderer);
  update_hw_cursor (native, FALSE);
  return priv->has_hw_cursor;
}

static void
meta_cursor_renderer_native_class_init (MetaCursorRendererNativeClass *klass)
{
  MetaCursorRendererClass *renderer_class = META_CURSOR_RENDERER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_cursor_renderer_native_finalize;
  renderer_class->update_cursor = meta_cursor_renderer_native_update_cursor;
}

static void
on_monitors_changed (MetaMonitorManager       *monitors,
                     MetaCursorRendererNative *native)
{
  /* Our tracking is all messed up, so force an update. */
  update_hw_cursor (native, TRUE);
}

static void
meta_cursor_renderer_native_init (MetaCursorRendererNative *native)
{
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (native);
  CoglContext *ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
  MetaMonitorManager *monitors;

  monitors = meta_monitor_manager_get ();
  g_signal_connect_object (monitors, "monitors-changed",
                           G_CALLBACK (on_monitors_changed), native, 0);

#if defined(CLUTTER_WINDOWING_EGL)
  if (clutter_check_windowing_backend (CLUTTER_WINDOWING_EGL))
    {
      CoglRenderer *cogl_renderer = cogl_display_get_renderer (cogl_context_get_display (ctx));
      priv->drm_fd = cogl_kms_renderer_get_kms_fd (cogl_renderer);
      priv->gbm = gbm_create_device (priv->drm_fd);
    }
#endif
}

struct gbm_device *
meta_cursor_renderer_native_get_gbm_device (MetaCursorRendererNative *native)
{
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (native);

  return priv->gbm;
}

void
meta_cursor_renderer_native_force_update (MetaCursorRendererNative *native)
{
  update_hw_cursor (native, TRUE);
}
