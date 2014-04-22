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

#include "meta-cursor-renderer.h"
#include "meta-cursor-private.h"

#include <cogl/cogl.h>
#include <cogl/cogl-wayland-server.h>
#include <clutter/clutter.h>
#include <gbm.h>

#include "meta-monitor-manager.h"
#include "meta-stage.h"

#include "wayland/meta-wayland-private.h"

struct _MetaCursorRendererPrivate
{
  gboolean has_hw_cursor;

  int current_x, current_y;
  MetaRectangle current_rect;

  int drm_fd;
  struct gbm_device *gbm;

  MetaCursorReference *displayed_cursor;
};
typedef struct _MetaCursorRendererPrivate MetaCursorRendererPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaCursorRenderer, meta_cursor_renderer, G_TYPE_OBJECT);

static void
set_crtc_cursor (MetaCursorRenderer  *renderer,
                 MetaCRTC            *crtc,
                 MetaCursorReference *cursor,
                 gboolean             force)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);

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
update_hw_cursor (MetaCursorRenderer *renderer,
                  gboolean            force)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);
  MetaRectangle *cursor_rect = &priv->current_rect;
  MetaMonitorManager *monitors;
  MetaCRTC *crtcs;
  unsigned int i, n_crtcs;

  monitors = meta_monitor_manager_get ();
  meta_monitor_manager_get_resources (monitors, NULL, NULL, &crtcs, &n_crtcs, NULL, NULL);

  for (i = 0; i < n_crtcs; i++)
    {
      gboolean crtc_should_have_cursor;
      MetaCursorReference *cursor;
      MetaRectangle *crtc_rect;

      crtc_rect = &crtcs[i].rect;

      crtc_should_have_cursor = (priv->has_hw_cursor && meta_rectangle_overlap (cursor_rect, crtc_rect));
      if (crtc_should_have_cursor)
        cursor = priv->displayed_cursor;
      else
        cursor = NULL;

      set_crtc_cursor (renderer, &crtcs[i], cursor, force);

      if (cursor)
        {
          drmModeMoveCursor (priv->drm_fd, crtcs[i].crtc_id,
                             cursor_rect->x - crtc_rect->x,
                             cursor_rect->y - crtc_rect->y);
        }
    }
}

static void
on_monitors_changed (MetaMonitorManager *monitors,
                     MetaCursorRenderer *renderer)
{
  /* Our tracking is all messed up, so force an update. */
  update_hw_cursor (renderer, TRUE);
}

static void
meta_cursor_renderer_finalize (GObject *object)
{
  MetaCursorRenderer *renderer = META_CURSOR_RENDERER (object);
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);

  if (priv->gbm)
    gbm_device_destroy (priv->gbm);

  G_OBJECT_CLASS (meta_cursor_renderer_parent_class)->finalize (object);
}

static void
meta_cursor_renderer_class_init (MetaCursorRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_cursor_renderer_finalize;
}

static void
meta_cursor_renderer_init (MetaCursorRenderer *renderer)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);
  CoglContext *ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
  MetaMonitorManager *monitors;

  monitors = meta_monitor_manager_get ();
  g_signal_connect_object (monitors, "monitors-changed",
                           G_CALLBACK (on_monitors_changed), renderer, 0);

#if defined(CLUTTER_WINDOWING_EGL)
  if (clutter_check_windowing_backend (CLUTTER_WINDOWING_EGL))
    {
      CoglRenderer *cogl_renderer = cogl_display_get_renderer (cogl_context_get_display (ctx));
      priv->drm_fd = cogl_kms_renderer_get_kms_fd (cogl_renderer);
      priv->gbm = gbm_create_device (priv->drm_fd);
    }
#endif
}

static void
queue_redraw (MetaCursorRenderer *renderer)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  ClutterActor *stage = compositor->stage;

  g_assert (meta_is_wayland_compositor ());

  /* During early initialization, we can have no stage */
  if (!stage)
    return;

  /* If we're not using a MetaStage, quit early */
  if (!META_IS_STAGE (stage))
    return;

  meta_stage_set_cursor (META_STAGE (stage),
                         priv->displayed_cursor,
                         &priv->current_rect);
}

static gboolean
should_have_hw_cursor (MetaCursorRenderer *renderer)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);

  if (priv->displayed_cursor)
    return (meta_cursor_reference_get_gbm_bo (priv->displayed_cursor, NULL, NULL) != NULL);
  else
    return FALSE;
}

static void
update_cursor (MetaCursorRenderer *renderer)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);

  if (priv->displayed_cursor)
    {
      CoglTexture *texture;
      int hot_x, hot_y;

      texture = meta_cursor_reference_get_cogl_texture (priv->displayed_cursor, &hot_x, &hot_y);

      priv->current_rect.x = priv->current_x - hot_x;
      priv->current_rect.y = priv->current_y - hot_y;
      priv->current_rect.width = cogl_texture_get_width (COGL_TEXTURE (texture));
      priv->current_rect.height = cogl_texture_get_height (COGL_TEXTURE (texture));
    }
  else
    {
      priv->current_rect.x = 0;
      priv->current_rect.y = 0;
      priv->current_rect.width = 0;
      priv->current_rect.height = 0;
    }

  if (meta_is_wayland_compositor ())
    {
      priv->has_hw_cursor = should_have_hw_cursor (renderer);
      update_hw_cursor (renderer, FALSE);

      if (!priv->has_hw_cursor)
        queue_redraw (renderer);
    }
}

MetaCursorRenderer *
meta_cursor_renderer_new (void)
{
  return g_object_new (META_TYPE_CURSOR_RENDERER, NULL);
}

void
meta_cursor_renderer_set_cursor (MetaCursorRenderer  *renderer,
                                 MetaCursorReference *cursor)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);

  if (priv->displayed_cursor == cursor)
    return;

  priv->displayed_cursor = cursor;
  update_cursor (renderer);
}

void
meta_cursor_renderer_set_position (MetaCursorRenderer *renderer,
                                   int x, int y)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);

  g_assert (meta_is_wayland_compositor ());

  priv->current_x = x;
  priv->current_y = y;

  update_cursor (renderer);
}

void
meta_cursor_renderer_force_update (MetaCursorRenderer *renderer)
{
  g_assert (meta_is_wayland_compositor ());

  update_hw_cursor (renderer, TRUE);
}

struct gbm_device *
meta_cursor_renderer_get_gbm_device (MetaCursorRenderer *renderer)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);

  return priv->gbm;
}
