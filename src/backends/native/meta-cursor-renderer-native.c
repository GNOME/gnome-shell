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

#include <string.h>
#include <gbm.h>
#include <xf86drm.h>
#include <errno.h>

#include <meta/util.h>
#include <meta/meta-backend.h>

#include "backends/meta-backend-private.h"
#include "backends/meta-logical-monitor.h"
#include "backends/meta-monitor.h"
#include "backends/meta-monitor-manager-private.h"
#include "backends/meta-output.h"
#include "backends/native/meta-renderer-native.h"
#include "core/boxes-private.h"
#include "meta/boxes.h"

#ifndef DRM_CAP_CURSOR_WIDTH
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif
#ifndef DRM_CAP_CURSOR_HEIGHT
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

/* When animating a cursor, we usually call drmModeSetCursor2 once per frame.
 * Though, testing shows that we need to triple buffer the cursor buffer in
 * order to avoid glitches when animating the cursor, at least when running on
 * Intel. The reason for this might be (but is not confirmed to be) due to
 * the user space gbm_bo cache, making us reuse and overwrite the kernel side
 * buffer content before it was scanned out. To avoid this, we keep a user space
 * reference to each buffer we set until at least one frame after it was drawn.
 * In effect, this means we three active cursor gbm_bo's: one that that just has
 * been set, one that was previously set and may or may not have been scanned
 * out, and one pending that will be replaced if the cursor sprite changes.
 */
#define HW_CURSOR_BUFFER_COUNT 3

static GQuark quark_cursor_sprite = 0;

struct _MetaCursorRendererNative
{
  MetaCursorRenderer parent;
};

struct _MetaCursorRendererNativePrivate
{
  MetaMonitorManager *monitor_manager;

  gboolean hw_state_invalidated;
  gboolean has_hw_cursor;

  MetaCursorSprite *last_cursor;
  guint animation_timeout_id;
};
typedef struct _MetaCursorRendererNativePrivate MetaCursorRendererNativePrivate;

typedef struct _MetaCursorRendererNativeGpuData
{
  gboolean hw_cursor_broken;

  uint64_t cursor_width;
  uint64_t cursor_height;
} MetaCursorRendererNativeGpuData;

typedef enum _MetaCursorGbmBoState
{
  META_CURSOR_GBM_BO_STATE_NONE,
  META_CURSOR_GBM_BO_STATE_SET,
  META_CURSOR_GBM_BO_STATE_INVALIDATED,
} MetaCursorGbmBoState;

typedef struct _MetaCursorNativeGpuState
{
  guint active_bo;
  MetaCursorGbmBoState pending_bo_state;
  struct gbm_bo *bos[HW_CURSOR_BUFFER_COUNT];
} MetaCursorNativeGpuState;

typedef struct _MetaCursorNativePrivate
{
  GHashTable *gpu_states;
} MetaCursorNativePrivate;

static GQuark quark_cursor_renderer_native_gpu_data = 0;

G_DEFINE_TYPE_WITH_PRIVATE (MetaCursorRendererNative, meta_cursor_renderer_native, META_TYPE_CURSOR_RENDERER);

static MetaCursorNativeGpuState *
get_cursor_gpu_state (MetaCursorNativePrivate *cursor_priv,
                      MetaGpuKms              *gpu_kms);

static MetaCursorNativeGpuState *
ensure_cursor_gpu_state (MetaCursorNativePrivate *cursor_priv,
                         MetaGpuKms              *gpu_kms);

static MetaCursorNativePrivate *
ensure_cursor_priv (MetaCursorSprite *cursor_sprite);

static MetaCursorNativePrivate *
get_cursor_priv (MetaCursorSprite *cursor_sprite);

static MetaCursorRendererNativeGpuData *
meta_cursor_renderer_native_gpu_data_from_gpu (MetaGpuKms *gpu_kms)
{
  return g_object_get_qdata (G_OBJECT (gpu_kms),
                             quark_cursor_renderer_native_gpu_data);
}

static MetaCursorRendererNativeGpuData *
meta_create_cursor_renderer_native_gpu_data (MetaGpuKms *gpu_kms)
{
  MetaCursorRendererNativeGpuData *cursor_renderer_gpu_data;

  cursor_renderer_gpu_data = g_new0 (MetaCursorRendererNativeGpuData, 1);
  g_object_set_qdata_full (G_OBJECT (gpu_kms),
                           quark_cursor_renderer_native_gpu_data,
                           cursor_renderer_gpu_data,
                           g_free);

  return cursor_renderer_gpu_data;
}

static void
meta_cursor_renderer_native_finalize (GObject *object)
{
  MetaCursorRendererNative *renderer = META_CURSOR_RENDERER_NATIVE (object);
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (renderer);

  if (priv->animation_timeout_id)
    g_source_remove (priv->animation_timeout_id);

  G_OBJECT_CLASS (meta_cursor_renderer_native_parent_class)->finalize (object);
}

static guint
get_pending_cursor_sprite_gbm_bo_index (MetaCursorNativeGpuState *cursor_gpu_state)
{
  return (cursor_gpu_state->active_bo + 1) % HW_CURSOR_BUFFER_COUNT;
}

static struct gbm_bo *
get_pending_cursor_sprite_gbm_bo (MetaCursorNativeGpuState *cursor_gpu_state)
{
  guint pending_bo;

  pending_bo = get_pending_cursor_sprite_gbm_bo_index (cursor_gpu_state);
  return cursor_gpu_state->bos[pending_bo];
}

static struct gbm_bo *
get_active_cursor_sprite_gbm_bo (MetaCursorNativeGpuState *cursor_gpu_state)
{
  return cursor_gpu_state->bos[cursor_gpu_state->active_bo];
}

static void
set_pending_cursor_sprite_gbm_bo (MetaCursorSprite *cursor_sprite,
                                  MetaGpuKms       *gpu_kms,
                                  struct gbm_bo    *bo)
{
  MetaCursorNativePrivate *cursor_priv;
  MetaCursorNativeGpuState *cursor_gpu_state;
  guint pending_bo;

  cursor_priv = ensure_cursor_priv (cursor_sprite);
  cursor_gpu_state = ensure_cursor_gpu_state (cursor_priv, gpu_kms);

  pending_bo = get_pending_cursor_sprite_gbm_bo_index (cursor_gpu_state);
  cursor_gpu_state->bos[pending_bo] = bo;
  cursor_gpu_state->pending_bo_state = META_CURSOR_GBM_BO_STATE_SET;
}

static void
set_crtc_cursor (MetaCursorRendererNative *native,
                 MetaCrtc                 *crtc,
                 MetaCursorSprite         *cursor_sprite)
{
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (native);
  MetaCursorRendererNativeGpuData *cursor_renderer_gpu_data;
  MetaGpuKms *gpu_kms;
  int kms_fd;

  gpu_kms = META_GPU_KMS (meta_crtc_get_gpu (crtc));
  cursor_renderer_gpu_data =
    meta_cursor_renderer_native_gpu_data_from_gpu (gpu_kms);
  kms_fd = meta_gpu_kms_get_fd (gpu_kms);

  if (cursor_sprite)
    {
      MetaCursorNativePrivate *cursor_priv;
      MetaCursorNativeGpuState *cursor_gpu_state;
      struct gbm_bo *bo;
      union gbm_bo_handle handle;
      int hot_x, hot_y;

      cursor_priv = get_cursor_priv (cursor_sprite);
      cursor_gpu_state = get_cursor_gpu_state (cursor_priv, gpu_kms);

      if (cursor_gpu_state->pending_bo_state == META_CURSOR_GBM_BO_STATE_SET)
        bo = get_pending_cursor_sprite_gbm_bo (cursor_gpu_state);
      else
        bo = get_active_cursor_sprite_gbm_bo (cursor_gpu_state);

      if (!priv->hw_state_invalidated && bo == crtc->cursor_renderer_private)
        return;

      crtc->cursor_renderer_private = bo;

      handle = gbm_bo_get_handle (bo);
      meta_cursor_sprite_get_hotspot (cursor_sprite, &hot_x, &hot_y);

      if (drmModeSetCursor2 (kms_fd, crtc->crtc_id, handle.u32,
                             cursor_renderer_gpu_data->cursor_width,
                             cursor_renderer_gpu_data->cursor_height,
                             hot_x, hot_y) < 0)
        {
          if (errno != EACCES)
            {
              g_warning ("drmModeSetCursor2 failed with (%s), "
                         "drawing cursor with OpenGL from now on",
                         strerror (errno));
              priv->has_hw_cursor = FALSE;
              cursor_renderer_gpu_data->hw_cursor_broken = TRUE;
            }
        }

      if (cursor_gpu_state->pending_bo_state == META_CURSOR_GBM_BO_STATE_SET)
        {
          cursor_gpu_state->active_bo =
            (cursor_gpu_state->active_bo + 1) % HW_CURSOR_BUFFER_COUNT;
          cursor_gpu_state->pending_bo_state = META_CURSOR_GBM_BO_STATE_NONE;
        }
    }
  else
    {
      if (priv->hw_state_invalidated || crtc->cursor_renderer_private != NULL)
        {
          drmModeSetCursor2 (kms_fd, crtc->crtc_id, 0, 0, 0, 0, 0);
          crtc->cursor_renderer_private = NULL;
        }
    }
}

typedef struct
{
  MetaCursorRendererNative *in_cursor_renderer_native;
  MetaLogicalMonitor *in_logical_monitor;
  ClutterRect in_local_cursor_rect;
  MetaCursorSprite *in_cursor_sprite;

  gboolean out_painted;
} UpdateCrtcCursorData;

static gboolean
update_monitor_crtc_cursor (MetaMonitor         *monitor,
                            MetaMonitorMode     *monitor_mode,
                            MetaMonitorCrtcMode *monitor_crtc_mode,
                            gpointer             user_data,
                            GError             **error)
{
  UpdateCrtcCursorData *data = user_data;
  MetaCursorRendererNative *cursor_renderer_native =
    data->in_cursor_renderer_native;
  MetaCursorRendererNativePrivate *priv =
    meta_cursor_renderer_native_get_instance_private (cursor_renderer_native);
  ClutterRect scaled_crtc_rect;
  float scale;
  int crtc_x, crtc_y;

  if (meta_is_stage_views_scaled ())
    scale = meta_logical_monitor_get_scale (data->in_logical_monitor);
  else
    scale = 1.0;

  meta_monitor_calculate_crtc_pos (monitor, monitor_mode,
                                   monitor_crtc_mode->output,
                                   META_MONITOR_TRANSFORM_NORMAL,
                                   &crtc_x, &crtc_y);

  scaled_crtc_rect = (ClutterRect) {
    .origin = {
      .x = crtc_x / scale,
      .y = crtc_y / scale
    },
    .size = {
      .width = monitor_crtc_mode->crtc_mode->width / scale,
      .height = monitor_crtc_mode->crtc_mode->height / scale
    },
  };

  if (priv->has_hw_cursor &&
      clutter_rect_intersection (&scaled_crtc_rect,
                                 &data->in_local_cursor_rect,
                                 NULL))
    {
      MetaGpuKms *gpu_kms;
      int kms_fd;
      float crtc_cursor_x, crtc_cursor_y;

      set_crtc_cursor (data->in_cursor_renderer_native,
                       monitor_crtc_mode->output->crtc,
                       data->in_cursor_sprite);

      gpu_kms = META_GPU_KMS (meta_monitor_get_gpu (monitor));
      kms_fd = meta_gpu_kms_get_fd (gpu_kms);
      crtc_cursor_x = (data->in_local_cursor_rect.origin.x -
                       scaled_crtc_rect.origin.x) * scale;
      crtc_cursor_y = (data->in_local_cursor_rect.origin.y -
                       scaled_crtc_rect.origin.y) * scale;
      drmModeMoveCursor (kms_fd,
                         monitor_crtc_mode->output->crtc->crtc_id,
                         roundf (crtc_cursor_x),
                         roundf (crtc_cursor_y));

      data->out_painted = data->out_painted || TRUE;
    }
  else
    {
      set_crtc_cursor (data->in_cursor_renderer_native,
                       monitor_crtc_mode->output->crtc, NULL);
    }

  return TRUE;
}

static void
update_hw_cursor (MetaCursorRendererNative *native,
                  MetaCursorSprite         *cursor_sprite)
{
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (native);
  MetaCursorRenderer *renderer = META_CURSOR_RENDERER (native);
  MetaMonitorManager *monitor_manager = priv->monitor_manager;
  GList *logical_monitors;
  GList *l;
  ClutterRect rect;
  gboolean painted = FALSE;

  if (cursor_sprite)
    rect = meta_cursor_renderer_calculate_rect (renderer, cursor_sprite);
  else
    rect = (ClutterRect) { 0 };

  logical_monitors =
    meta_monitor_manager_get_logical_monitors (monitor_manager);
  for (l = logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;
      UpdateCrtcCursorData data;
      GList *monitors;
      GList *k;

      data = (UpdateCrtcCursorData) {
        .in_cursor_renderer_native = native,
        .in_logical_monitor = logical_monitor,
        .in_local_cursor_rect = (ClutterRect) {
          .origin = {
            .x = rect.origin.x - logical_monitor->rect.x,
            .y = rect.origin.y - logical_monitor->rect.y
          },
          .size = rect.size
        },
        .in_cursor_sprite = cursor_sprite
      };

      monitors = meta_logical_monitor_get_monitors (logical_monitor);
      for (k = monitors; k; k = k->next)
        {
          MetaMonitor *monitor = k->data;
          MetaMonitorMode *monitor_mode;

          monitor_mode = meta_monitor_get_current_mode (monitor);
          meta_monitor_mode_foreach_crtc (monitor, monitor_mode,
                                          update_monitor_crtc_cursor,
                                          &data,
                                          NULL);
        }

      painted = painted || data.out_painted;
    }

  priv->hw_state_invalidated = FALSE;

  if (painted)
    meta_cursor_renderer_emit_painted (renderer, cursor_sprite);
}

static gboolean
has_valid_cursor_sprite_gbm_bo (MetaCursorSprite *cursor_sprite,
                                MetaGpuKms       *gpu_kms)
{
  MetaCursorNativePrivate *cursor_priv;
  MetaCursorNativeGpuState *cursor_gpu_state;

  cursor_priv = get_cursor_priv (cursor_sprite);
  if (!cursor_priv)
    return FALSE;

  cursor_gpu_state = get_cursor_gpu_state (cursor_priv, gpu_kms);
  if (!cursor_gpu_state)
    return FALSE;

  switch (cursor_gpu_state->pending_bo_state)
    {
    case META_CURSOR_GBM_BO_STATE_NONE:
      return get_active_cursor_sprite_gbm_bo (cursor_gpu_state) != NULL;
    case META_CURSOR_GBM_BO_STATE_SET:
      return TRUE;
    case META_CURSOR_GBM_BO_STATE_INVALIDATED:
      return FALSE;
    }

  g_assert_not_reached ();

  return FALSE;
}

static gboolean
cursor_over_transformed_logical_monitor (MetaCursorRenderer *renderer,
                                         MetaCursorSprite   *cursor_sprite)
{
  MetaCursorRendererNative *cursor_renderer_native =
    META_CURSOR_RENDERER_NATIVE (renderer);
  MetaCursorRendererNativePrivate *priv =
    meta_cursor_renderer_native_get_instance_private (cursor_renderer_native);
  MetaMonitorManager *monitor_manager = priv->monitor_manager;
  GList *logical_monitors;
  GList *l;
  ClutterRect cursor_rect;

  cursor_rect = meta_cursor_renderer_calculate_rect (renderer, cursor_sprite);

  logical_monitors =
    meta_monitor_manager_get_logical_monitors (monitor_manager);
  for (l = logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;
      MetaRectangle logical_monitor_layout;
      ClutterRect logical_monitor_rect;
      MetaMonitorTransform transform;

      logical_monitor_layout =
        meta_logical_monitor_get_layout (logical_monitor);
      logical_monitor_rect =
        meta_rectangle_to_clutter_rect (&logical_monitor_layout);

      if (!clutter_rect_intersection (&cursor_rect, &logical_monitor_rect,
                                      NULL))
        continue;

      transform = meta_logical_monitor_get_transform (logical_monitor);
      if (transform != META_MONITOR_TRANSFORM_NORMAL)
        return TRUE;
    }

  return FALSE;
}

static float
calculate_cursor_crtc_sprite_scale (MetaCursorSprite   *cursor_sprite,
                                    MetaLogicalMonitor *logical_monitor)
{
  return (meta_logical_monitor_get_scale (logical_monitor) *
          meta_cursor_sprite_get_texture_scale (cursor_sprite));
}

static gboolean
can_draw_cursor_unscaled (MetaCursorRenderer *renderer,
                          MetaCursorSprite   *cursor_sprite)
{
  MetaCursorRendererNative *cursor_renderer_native =
    META_CURSOR_RENDERER_NATIVE (renderer);
  MetaCursorRendererNativePrivate *priv =
    meta_cursor_renderer_native_get_instance_private (cursor_renderer_native);
  MetaMonitorManager *monitor_manager = priv->monitor_manager;
  ClutterRect cursor_rect;
  GList *logical_monitors;
  GList *l;
  gboolean has_visible_crtc_sprite = FALSE;

  if (!meta_is_stage_views_scaled ())
   return meta_cursor_sprite_get_texture_scale (cursor_sprite) == 1.0;

  logical_monitors =
    meta_monitor_manager_get_logical_monitors (monitor_manager);

  if (!logical_monitors)
    return FALSE;

  cursor_rect = meta_cursor_renderer_calculate_rect (renderer, cursor_sprite);

  for (l = logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;
      ClutterRect logical_monitor_rect =
        meta_rectangle_to_clutter_rect (&logical_monitor->rect);

      if (!clutter_rect_intersection (&cursor_rect,
                                      &logical_monitor_rect,
                                      NULL))
        continue;

      if (calculate_cursor_crtc_sprite_scale (cursor_sprite,
                                              logical_monitor) != 1.0)
        return FALSE;

      has_visible_crtc_sprite = TRUE;
    }

  return has_visible_crtc_sprite;
}

static gboolean
should_have_hw_cursor (MetaCursorRenderer *renderer,
                       MetaCursorSprite   *cursor_sprite)
{
  MetaCursorRendererNative *native = META_CURSOR_RENDERER_NATIVE (renderer);
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (native);
  GList *gpus;
  GList *l;
  CoglTexture *texture;

  if (!cursor_sprite)
    return FALSE;

  gpus = meta_monitor_manager_get_gpus (priv->monitor_manager);
  for (l = gpus; l; l = l->next)
    {
      MetaGpuKms *gpu_kms = l->data;
      MetaCursorRendererNativeGpuData *cursor_renderer_gpu_data;

      cursor_renderer_gpu_data =
        meta_cursor_renderer_native_gpu_data_from_gpu (gpu_kms);
      if (!cursor_renderer_gpu_data)
        return FALSE;

      if (cursor_renderer_gpu_data->hw_cursor_broken)
        return FALSE;

      if (!has_valid_cursor_sprite_gbm_bo (cursor_sprite, gpu_kms))
        return FALSE;
    }

  if (cursor_over_transformed_logical_monitor (renderer, cursor_sprite))
    return FALSE;

  texture = meta_cursor_sprite_get_cogl_texture (cursor_sprite);
  if (!texture)
    return FALSE;

  if (!can_draw_cursor_unscaled (renderer, cursor_sprite))
    return FALSE;

  return TRUE;
}

static gboolean
meta_cursor_renderer_native_update_animation (MetaCursorRendererNative *native)
{
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (native);
  MetaCursorRenderer *renderer = META_CURSOR_RENDERER (native);
  MetaCursorSprite *cursor_sprite = meta_cursor_renderer_get_cursor (renderer);

  priv->animation_timeout_id = 0;
  meta_cursor_sprite_tick_frame (cursor_sprite);
  meta_cursor_renderer_force_update (renderer);

  return G_SOURCE_REMOVE;
}

static void
meta_cursor_renderer_native_trigger_frame (MetaCursorRendererNative *native,
                                           MetaCursorSprite         *cursor_sprite)
{
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (native);
  gboolean cursor_change;
  guint delay;

  cursor_change = cursor_sprite != priv->last_cursor;
  priv->last_cursor = cursor_sprite;

  if (!cursor_change && priv->animation_timeout_id)
    return;

  if (priv->animation_timeout_id)
    {
      g_source_remove (priv->animation_timeout_id);
      priv->animation_timeout_id = 0;
    }

  if (cursor_sprite && meta_cursor_sprite_is_animated (cursor_sprite))
    {
      delay = meta_cursor_sprite_get_current_frame_time (cursor_sprite);

      if (delay == 0)
        return;

      priv->animation_timeout_id =
        g_timeout_add (delay,
                       (GSourceFunc) meta_cursor_renderer_native_update_animation,
                       native);
      g_source_set_name_by_id (priv->animation_timeout_id,
                               "[mutter] meta_cursor_renderer_native_update_animation");
    }
}

static gboolean
meta_cursor_renderer_native_update_cursor (MetaCursorRenderer *renderer,
                                           MetaCursorSprite   *cursor_sprite)
{
  MetaCursorRendererNative *native = META_CURSOR_RENDERER_NATIVE (renderer);
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (native);

  if (cursor_sprite)
    meta_cursor_sprite_realize_texture (cursor_sprite);

  meta_cursor_renderer_native_trigger_frame (native, cursor_sprite);

  priv->has_hw_cursor = should_have_hw_cursor (renderer, cursor_sprite);
  update_hw_cursor (native, cursor_sprite);
  return priv->has_hw_cursor;
}

static void
cursor_gpu_state_free (MetaCursorNativeGpuState *cursor_gpu_state)
{
  int i;

  for (i = 0; i < HW_CURSOR_BUFFER_COUNT; i++)
    g_clear_pointer (&cursor_gpu_state->bos[i], (GDestroyNotify) gbm_bo_destroy);
  g_free (cursor_gpu_state);
}

static MetaCursorNativeGpuState *
get_cursor_gpu_state (MetaCursorNativePrivate *cursor_priv,
                      MetaGpuKms              *gpu_kms)
{
  return g_hash_table_lookup (cursor_priv->gpu_states, gpu_kms);
}

static MetaCursorNativeGpuState *
ensure_cursor_gpu_state (MetaCursorNativePrivate *cursor_priv,
                         MetaGpuKms              *gpu_kms)
{
  MetaCursorNativeGpuState *cursor_gpu_state;

  cursor_gpu_state = get_cursor_gpu_state (cursor_priv, gpu_kms);
  if (cursor_gpu_state)
    return cursor_gpu_state;

  cursor_gpu_state = g_new0 (MetaCursorNativeGpuState, 1);
  g_hash_table_insert (cursor_priv->gpu_states, gpu_kms, cursor_gpu_state);

  return cursor_gpu_state;
}

static void
cursor_priv_free (MetaCursorNativePrivate *cursor_priv)
{
  g_hash_table_destroy (cursor_priv->gpu_states);
}

static MetaCursorNativePrivate *
get_cursor_priv (MetaCursorSprite *cursor_sprite)
{
  return g_object_get_qdata (G_OBJECT (cursor_sprite), quark_cursor_sprite);
}

static MetaCursorNativePrivate *
ensure_cursor_priv (MetaCursorSprite *cursor_sprite)
{
  MetaCursorNativePrivate *cursor_priv;

  cursor_priv = get_cursor_priv (cursor_sprite);
  if (cursor_priv)
    return cursor_priv;

  cursor_priv = g_new0 (MetaCursorNativePrivate, 1);
  cursor_priv->gpu_states =
    g_hash_table_new_full (g_direct_hash,
                           g_direct_equal,
                           NULL,
                           (GDestroyNotify) cursor_gpu_state_free);
  g_object_set_qdata_full (G_OBJECT (cursor_sprite),
                           quark_cursor_sprite,
                           cursor_priv,
                           (GDestroyNotify) cursor_priv_free);

  return cursor_priv;
}

static void
load_cursor_sprite_gbm_buffer_for_gpu (MetaCursorRendererNative *native,
                                       MetaGpuKms               *gpu_kms,
                                       MetaCursorSprite         *cursor_sprite,
                                       uint8_t                  *pixels,
                                       uint                      width,
                                       uint                      height,
                                       int                       rowstride,
                                       uint32_t                  gbm_format)
{
  uint64_t cursor_width, cursor_height;
  MetaCursorRendererNativeGpuData *cursor_renderer_gpu_data;
  struct gbm_device *gbm_device;

  cursor_renderer_gpu_data =
    meta_cursor_renderer_native_gpu_data_from_gpu (gpu_kms);
  if (!cursor_renderer_gpu_data)
    return;

  cursor_width = (uint64_t) cursor_renderer_gpu_data->cursor_width;
  cursor_height = (uint64_t) cursor_renderer_gpu_data->cursor_height;

  if (width > cursor_width || height > cursor_height)
    {
      meta_warning ("Invalid theme cursor size (must be at most %ux%u)\n",
                    (unsigned int)cursor_width, (unsigned int)cursor_height);
      return;
    }

  gbm_device = meta_gbm_device_from_gpu (gpu_kms);
  if (gbm_device_is_format_supported (gbm_device, gbm_format,
                                      GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE))
    {
      struct gbm_bo *bo;
      uint8_t buf[4 * cursor_width * cursor_height];
      uint i;

      bo = gbm_bo_create (gbm_device, cursor_width, cursor_height,
                          gbm_format, GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);
      if (!bo)
        {
          meta_warning ("Failed to allocate HW cursor buffer\n");
          return;
        }

      memset (buf, 0, sizeof(buf));
      for (i = 0; i < height; i++)
        memcpy (buf + i * 4 * cursor_width, pixels + i * rowstride, width * 4);
      if (gbm_bo_write (bo, buf, cursor_width * cursor_height * 4) != 0)
        {
          meta_warning ("Failed to write cursors buffer data: %s",
                        g_strerror (errno));
          gbm_bo_destroy (bo);
          return;
        }

      set_pending_cursor_sprite_gbm_bo (cursor_sprite, gpu_kms, bo);
    }
  else
    {
      meta_warning ("HW cursor for format %d not supported\n", gbm_format);
    }
}

static void
invalidate_pending_cursor_sprite_gbm_bo (MetaCursorSprite *cursor_sprite,
                                         MetaGpuKms       *gpu_kms)
{
  MetaCursorNativePrivate *cursor_priv;
  MetaCursorNativeGpuState *cursor_gpu_state;
  guint pending_bo;

  cursor_priv = get_cursor_priv (cursor_sprite);
  if (!cursor_priv)
    return;

  cursor_gpu_state = get_cursor_gpu_state (cursor_priv, gpu_kms);
  if (!cursor_gpu_state)
    return;

  pending_bo = get_pending_cursor_sprite_gbm_bo_index (cursor_gpu_state);
  g_clear_pointer (&cursor_gpu_state->bos[pending_bo],
                   (GDestroyNotify) gbm_bo_destroy);
  cursor_gpu_state->pending_bo_state = META_CURSOR_GBM_BO_STATE_INVALIDATED;
}

#ifdef HAVE_WAYLAND
static void
meta_cursor_renderer_native_realize_cursor_from_wl_buffer_for_gpu (MetaCursorRenderer *renderer,
                                                                   MetaGpuKms         *gpu_kms,
                                                                   MetaCursorSprite   *cursor_sprite,
                                                                   struct wl_resource *buffer)
{
  MetaCursorRendererNative *native = META_CURSOR_RENDERER_NATIVE (renderer);
  MetaCursorRendererNativeGpuData *cursor_renderer_gpu_data;
  uint32_t gbm_format;
  uint64_t cursor_width, cursor_height;
  CoglTexture *texture;
  uint width, height;

  cursor_renderer_gpu_data =
    meta_cursor_renderer_native_gpu_data_from_gpu (gpu_kms);
  if (!cursor_renderer_gpu_data || cursor_renderer_gpu_data->hw_cursor_broken)
    return;

  /* Destroy any previous pending cursor buffer; we'll always either fail (which
   * should unset, or succeed, which will set new buffer.
   */
  invalidate_pending_cursor_sprite_gbm_bo (cursor_sprite, gpu_kms);

  texture = meta_cursor_sprite_get_cogl_texture (cursor_sprite);
  width = cogl_texture_get_width (texture);
  height = cogl_texture_get_height (texture);

  struct wl_shm_buffer *shm_buffer = wl_shm_buffer_get (buffer);
  if (shm_buffer)
    {
      int rowstride = wl_shm_buffer_get_stride (shm_buffer);
      uint8_t *buffer_data;

      wl_shm_buffer_begin_access (shm_buffer);

      switch (wl_shm_buffer_get_format (shm_buffer))
        {
#if G_BYTE_ORDER == G_BIG_ENDIAN
        case WL_SHM_FORMAT_ARGB8888:
          gbm_format = GBM_FORMAT_ARGB8888;
          break;
        case WL_SHM_FORMAT_XRGB8888:
          gbm_format = GBM_FORMAT_XRGB8888;
          break;
#else
        case WL_SHM_FORMAT_ARGB8888:
          gbm_format = GBM_FORMAT_ARGB8888;
          break;
        case WL_SHM_FORMAT_XRGB8888:
          gbm_format = GBM_FORMAT_XRGB8888;
          break;
#endif
        default:
          g_warn_if_reached ();
          gbm_format = GBM_FORMAT_ARGB8888;
        }

      buffer_data = wl_shm_buffer_get_data (shm_buffer);
      load_cursor_sprite_gbm_buffer_for_gpu (native,
                                             gpu_kms,
                                             cursor_sprite,
                                             buffer_data,
                                             width, height, rowstride,
                                             gbm_format);

      wl_shm_buffer_end_access (shm_buffer);
    }
  else
    {
      struct gbm_device *gbm_device;
      struct gbm_bo *bo;

      /* HW cursors have a predefined size (at least 64x64), which usually is
       * bigger than cursor theme size, so themed cursors must be padded with
       * transparent pixels to fill the overlay. This is trivial if we have CPU
       * access to the data, but it's not possible if the buffer is in GPU
       * memory (and possibly tiled too), so if we don't get the right size, we
       * fallback to GL. */
      cursor_width = (uint64_t) cursor_renderer_gpu_data->cursor_width;
      cursor_height = (uint64_t) cursor_renderer_gpu_data->cursor_height;

      if (width != cursor_width || height != cursor_height)
        {
          meta_warning ("Invalid cursor size (must be 64x64), falling back to software (GL) cursors\n");
          return;
        }

      gbm_device = meta_gbm_device_from_gpu (gpu_kms);
      bo = gbm_bo_import (gbm_device,
                          GBM_BO_IMPORT_WL_BUFFER,
                          buffer,
                          GBM_BO_USE_CURSOR);
      if (!bo)
        {
          meta_warning ("Importing HW cursor from wl_buffer failed\n");
          return;
        }

      set_pending_cursor_sprite_gbm_bo (cursor_sprite, gpu_kms, bo);
    }
}

static void
meta_cursor_renderer_native_realize_cursor_from_wl_buffer (MetaCursorRenderer *renderer,
                                                           MetaCursorSprite *cursor_sprite,
                                                           struct wl_resource *buffer)
{
  MetaCursorRendererNative *native = META_CURSOR_RENDERER_NATIVE (renderer);
  MetaCursorRendererNativePrivate *priv =
	  meta_cursor_renderer_native_get_instance_private (native);
  GList *gpus;
  GList *l;

  gpus = meta_monitor_manager_get_gpus (priv->monitor_manager);
  for (l = gpus; l; l = l->next)
    {
      MetaGpuKms *gpu_kms = l->data;

      meta_cursor_renderer_native_realize_cursor_from_wl_buffer_for_gpu (
        renderer,
        gpu_kms,
        cursor_sprite,
        buffer);
    }
}
#endif

static void
meta_cursor_renderer_native_realize_cursor_from_xcursor_for_gpu (MetaCursorRenderer *renderer,
                                                                 MetaGpuKms         *gpu_kms,
                                                                 MetaCursorSprite   *cursor_sprite,
                                                                 XcursorImage       *xc_image)
{
  MetaCursorRendererNative *native = META_CURSOR_RENDERER_NATIVE (renderer);
  MetaCursorRendererNativeGpuData *cursor_renderer_gpu_data;

  cursor_renderer_gpu_data =
    meta_cursor_renderer_native_gpu_data_from_gpu (gpu_kms);
  if (!cursor_renderer_gpu_data || cursor_renderer_gpu_data->hw_cursor_broken)
    return;

  invalidate_pending_cursor_sprite_gbm_bo (cursor_sprite, gpu_kms);

  load_cursor_sprite_gbm_buffer_for_gpu (native,
                                         gpu_kms,
                                         cursor_sprite,
                                         (uint8_t *) xc_image->pixels,
                                         xc_image->width,
                                         xc_image->height,
                                         xc_image->width * 4,
                                         GBM_FORMAT_ARGB8888);
}

static void
meta_cursor_renderer_native_realize_cursor_from_xcursor (MetaCursorRenderer *renderer,
                                                         MetaCursorSprite   *cursor_sprite,
                                                         XcursorImage       *xc_image)
{
  MetaCursorRendererNative *native = META_CURSOR_RENDERER_NATIVE (renderer);
  MetaCursorRendererNativePrivate *priv =
	  meta_cursor_renderer_native_get_instance_private (native);
  GList *gpus;
  GList *l;

  gpus = meta_monitor_manager_get_gpus (priv->monitor_manager);
  for (l = gpus; l; l = l->next)
    {
      MetaGpuKms *gpu_kms = l->data;

      meta_cursor_renderer_native_realize_cursor_from_xcursor_for_gpu (
        renderer,
        gpu_kms,
        cursor_sprite,
        xc_image);
    }
}

static void
meta_cursor_renderer_native_class_init (MetaCursorRendererNativeClass *klass)
{
  MetaCursorRendererClass *renderer_class = META_CURSOR_RENDERER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_cursor_renderer_native_finalize;
  renderer_class->update_cursor = meta_cursor_renderer_native_update_cursor;
#ifdef HAVE_WAYLAND
  renderer_class->realize_cursor_from_wl_buffer =
    meta_cursor_renderer_native_realize_cursor_from_wl_buffer;
#endif
  renderer_class->realize_cursor_from_xcursor =
    meta_cursor_renderer_native_realize_cursor_from_xcursor;

  quark_cursor_sprite = g_quark_from_static_string ("-meta-cursor-native");
  quark_cursor_renderer_native_gpu_data =
    g_quark_from_static_string ("-meta-cursor-renderer-native-gpu-data");
}

static void
force_update_hw_cursor (MetaCursorRendererNative *native)
{
  MetaCursorRenderer *renderer = META_CURSOR_RENDERER (native);
  MetaCursorRendererNativePrivate *priv =
    meta_cursor_renderer_native_get_instance_private (native);

  priv->hw_state_invalidated = TRUE;
  update_hw_cursor (native, meta_cursor_renderer_get_cursor (renderer));
}

static void
on_monitors_changed (MetaMonitorManager       *monitors,
                     MetaCursorRendererNative *native)
{
  /* Our tracking is all messed up, so force an update. */
  force_update_hw_cursor (native);
}

static void
init_hw_cursor_support (MetaCursorRendererNative *cursor_renderer_native)
{
  MetaCursorRendererNativePrivate *priv =
    meta_cursor_renderer_native_get_instance_private (cursor_renderer_native);
  GList *gpus;
  GList *l;

  gpus = meta_monitor_manager_get_gpus (priv->monitor_manager);
  for (l = gpus; l; l = l->next)
    {
      MetaGpuKms *gpu_kms = l->data;
      MetaCursorRendererNativeGpuData *cursor_renderer_gpu_data;
      int kms_fd;
      struct gbm_device *gbm_device;
      uint64_t width, height;

      gbm_device = meta_gbm_device_from_gpu (gpu_kms);
      if (!gbm_device)
        continue;

      cursor_renderer_gpu_data =
        meta_create_cursor_renderer_native_gpu_data (gpu_kms);

      kms_fd = meta_gpu_kms_get_fd (gpu_kms);
      if (drmGetCap (kms_fd, DRM_CAP_CURSOR_WIDTH, &width) == 0 &&
          drmGetCap (kms_fd, DRM_CAP_CURSOR_HEIGHT, &height) == 0)
        {
          cursor_renderer_gpu_data->cursor_width = width;
          cursor_renderer_gpu_data->cursor_height = height;
        }
      else
        {
          cursor_renderer_gpu_data->cursor_width = 64;
          cursor_renderer_gpu_data->cursor_height = 64;
        }
    }
}

MetaCursorRendererNative *
meta_cursor_renderer_native_new (MetaBackend *backend)
{
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaCursorRendererNative *cursor_renderer_native;
  MetaCursorRendererNativePrivate *priv;

  cursor_renderer_native =
    g_object_new (META_TYPE_CURSOR_RENDERER_NATIVE, NULL);
  priv =
    meta_cursor_renderer_native_get_instance_private (cursor_renderer_native);

  g_signal_connect_object (monitor_manager, "monitors-changed",
                           G_CALLBACK (on_monitors_changed),
                           cursor_renderer_native, 0);

  priv->monitor_manager = monitor_manager;
  priv->hw_state_invalidated = TRUE;

  init_hw_cursor_support (cursor_renderer_native);

  return cursor_renderer_native;
}

static void
meta_cursor_renderer_native_init (MetaCursorRendererNative *native)
{
}

void
meta_cursor_renderer_native_force_update (MetaCursorRendererNative *native)
{
  force_update_hw_cursor (native);
}
