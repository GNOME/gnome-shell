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
set_crtc_has_hw_cursor (MetaCursorRenderer *renderer,
                        MetaCRTC           *crtc,
                        gboolean            has)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);

  if (has)
    {
      MetaCursorReference *displayed_cursor = priv->displayed_cursor;
      struct gbm_bo *bo;
      union gbm_bo_handle handle;
      int width, height;
      int hot_x, hot_y;

      bo = meta_cursor_reference_get_gbm_bo (displayed_cursor, &hot_x, &hot_y);

      handle = gbm_bo_get_handle (bo);
      width = gbm_bo_get_width (bo);
      height = gbm_bo_get_height (bo);

      drmModeSetCursor2 (priv->drm_fd, crtc->crtc_id, handle.u32,
                         width, height, hot_x, hot_y);
      crtc->has_hw_cursor = TRUE;
    }
  else
    {
      drmModeSetCursor2 (priv->drm_fd, crtc->crtc_id, 0, 0, 0, 0, 0);
      crtc->has_hw_cursor = FALSE;
    }
}

static void
on_monitors_changed (MetaMonitorManager *monitors,
                     MetaCursorRenderer *renderer)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);
  MetaCRTC *crtcs;
  unsigned int i, n_crtcs;

  if (!priv->has_hw_cursor)
    return;

  /* Go through the new list of monitors, find out where the cursor is */
  meta_monitor_manager_get_resources (monitors, NULL, NULL, &crtcs, &n_crtcs, NULL, NULL);

  for (i = 0; i < n_crtcs; i++)
    {
      MetaRectangle *rect = &crtcs[i].rect;
      gboolean has;

      has = meta_rectangle_overlap (&priv->current_rect, rect);

      /* Need to do it unconditionally here, our tracking is
         wrong because we reloaded the CRTCs */
      set_crtc_has_hw_cursor (renderer, &crtcs[i], has);
    }
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
update_hw_cursor (MetaCursorRenderer *renderer)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);
  MetaMonitorManager *monitors;
  MetaCRTC *crtcs;
  unsigned int i, n_crtcs;
  gboolean enabled;

  enabled = should_have_hw_cursor (renderer);
  priv->has_hw_cursor = enabled;

  monitors = meta_monitor_manager_get ();
  meta_monitor_manager_get_resources (monitors, NULL, NULL, &crtcs, &n_crtcs, NULL, NULL);

  for (i = 0; i < n_crtcs; i++)
    {
      MetaRectangle *rect = &crtcs[i].rect;
      gboolean has;

      has = enabled && meta_rectangle_overlap (&priv->current_rect, rect);

      if (has || crtcs[i].has_hw_cursor)
        set_crtc_has_hw_cursor (renderer, &crtcs[i], has);
    }
}

static void
move_hw_cursor (MetaCursorRenderer *renderer)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);
  MetaMonitorManager *monitors;
  MetaCRTC *crtcs;
  unsigned int i, n_crtcs;

  monitors = meta_monitor_manager_get ();
  meta_monitor_manager_get_resources (monitors, NULL, NULL, &crtcs, &n_crtcs, NULL, NULL);

  g_assert (priv->has_hw_cursor);

  for (i = 0; i < n_crtcs; i++)
    {
      MetaRectangle *rect = &crtcs[i].rect;
      gboolean has;

      has = meta_rectangle_overlap (&priv->current_rect, rect);

      if (has != crtcs[i].has_hw_cursor)
        set_crtc_has_hw_cursor (renderer, &crtcs[i], has);
      if (has)
        drmModeMoveCursor (priv->drm_fd, crtcs[i].crtc_id,
                           priv->current_rect.x - rect->x,
                           priv->current_rect.y - rect->y);
    }
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
      if (priv->has_hw_cursor)
        {
          update_hw_cursor (renderer);
          move_hw_cursor (renderer);
        }
      else
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

  update_hw_cursor (renderer);
}

struct gbm_device *
meta_cursor_renderer_get_gbm_device (MetaCursorRenderer *renderer)
{
  MetaCursorRendererPrivate *priv = meta_cursor_renderer_get_instance_private (renderer);

  return priv->gbm;
}
