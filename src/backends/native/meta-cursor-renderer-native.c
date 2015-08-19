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

#include "meta-monitor-manager-private.h"
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

struct _MetaCursorRendererNativePrivate
{
  gboolean has_hw_cursor;

  MetaCursorSprite *last_cursor;
  guint animation_timeout_id;

  int drm_fd;
  struct gbm_device *gbm;

  uint64_t cursor_width;
  uint64_t cursor_height;
};
typedef struct _MetaCursorRendererNativePrivate MetaCursorRendererNativePrivate;

typedef enum _MetaCursorGbmBoState
{
  META_CURSOR_GBM_BO_STATE_NONE,
  META_CURSOR_GBM_BO_STATE_SET,
  META_CURSOR_GBM_BO_STATE_INVALIDATED,
} MetaCursorGbmBoState;

typedef struct _MetaCursorNativePrivate
{
  guint active_bo;
  MetaCursorGbmBoState pending_bo_state;
  struct gbm_bo *bos[HW_CURSOR_BUFFER_COUNT];
} MetaCursorNativePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaCursorRendererNative, meta_cursor_renderer_native, META_TYPE_CURSOR_RENDERER);

static MetaCursorNativePrivate *
ensure_cursor_priv (MetaCursorSprite *cursor_sprite);

static void
meta_cursor_renderer_native_finalize (GObject *object)
{
  MetaCursorRendererNative *renderer = META_CURSOR_RENDERER_NATIVE (object);
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (renderer);

  if (priv->animation_timeout_id)
    g_source_remove (priv->animation_timeout_id);

  if (priv->gbm)
    gbm_device_destroy (priv->gbm);

  G_OBJECT_CLASS (meta_cursor_renderer_native_parent_class)->finalize (object);
}

static guint
get_pending_cursor_sprite_gbm_bo_index (MetaCursorSprite *cursor_sprite)
{
  MetaCursorNativePrivate *cursor_priv =
    g_object_get_qdata (G_OBJECT (cursor_sprite), quark_cursor_sprite);

  return (cursor_priv->active_bo + 1) % HW_CURSOR_BUFFER_COUNT;
}

static struct gbm_bo *
get_pending_cursor_sprite_gbm_bo (MetaCursorSprite *cursor_sprite)
{
  MetaCursorNativePrivate *cursor_priv =
    g_object_get_qdata (G_OBJECT (cursor_sprite), quark_cursor_sprite);
  guint pending_bo;

  if (!cursor_priv)
    return NULL;

  pending_bo = get_pending_cursor_sprite_gbm_bo_index (cursor_sprite);
  return cursor_priv->bos[pending_bo];
}

static struct gbm_bo *
get_active_cursor_sprite_gbm_bo (MetaCursorSprite *cursor_sprite)
{
  MetaCursorNativePrivate *cursor_priv =
    g_object_get_qdata (G_OBJECT (cursor_sprite), quark_cursor_sprite);

  if (!cursor_priv)
    return NULL;

  return cursor_priv->bos[cursor_priv->active_bo];
}

static void
set_pending_cursor_sprite_gbm_bo (MetaCursorSprite *cursor_sprite,
                                  struct gbm_bo    *bo)
{
  MetaCursorNativePrivate *cursor_priv;
  guint pending_bo;

  cursor_priv = ensure_cursor_priv (cursor_sprite);

  pending_bo = get_pending_cursor_sprite_gbm_bo_index (cursor_sprite);
  cursor_priv->bos[pending_bo] = bo;
  cursor_priv->pending_bo_state = META_CURSOR_GBM_BO_STATE_SET;
}

static void
set_crtc_cursor (MetaCursorRendererNative *native,
                 MetaCRTC                 *crtc,
                 MetaCursorSprite         *cursor_sprite,
                 gboolean                  force)
{
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (native);

  if (cursor_sprite)
    {
      MetaCursorNativePrivate *cursor_priv =
        g_object_get_qdata (G_OBJECT (cursor_sprite), quark_cursor_sprite);
      struct gbm_bo *bo;
      union gbm_bo_handle handle;
      int hot_x, hot_y;

      if (cursor_priv->pending_bo_state == META_CURSOR_GBM_BO_STATE_SET)
        bo = get_pending_cursor_sprite_gbm_bo (cursor_sprite);
      else
        bo = get_active_cursor_sprite_gbm_bo (cursor_sprite);

      if (!force && bo == crtc->cursor_renderer_private)
        return;

      crtc->cursor_renderer_private = bo;

      handle = gbm_bo_get_handle (bo);
      meta_cursor_sprite_get_hotspot (cursor_sprite, &hot_x, &hot_y);

      drmModeSetCursor2 (priv->drm_fd, crtc->crtc_id, handle.u32,
                         priv->cursor_width, priv->cursor_height, hot_x, hot_y);

      if (cursor_priv->pending_bo_state == META_CURSOR_GBM_BO_STATE_SET)
        {
          cursor_priv->active_bo =
            (cursor_priv->active_bo + 1) % HW_CURSOR_BUFFER_COUNT;
          cursor_priv->pending_bo_state = META_CURSOR_GBM_BO_STATE_NONE;
        }
    }
  else
    {
      if (force || crtc->cursor_renderer_private != NULL)
        {
          drmModeSetCursor2 (priv->drm_fd, crtc->crtc_id, 0, 0, 0, 0, 0);
          crtc->cursor_renderer_private = NULL;
        }
    }
}

static void
update_hw_cursor (MetaCursorRendererNative *native,
                  MetaCursorSprite         *cursor_sprite,
                  gboolean                  force)
{
  MetaCursorRendererNativePrivate *priv = meta_cursor_renderer_native_get_instance_private (native);
  MetaCursorRenderer *renderer = META_CURSOR_RENDERER (native);
  MetaMonitorManager *monitors;
  MetaCRTC *crtcs;
  unsigned int i, n_crtcs;
  MetaRectangle rect;

  monitors = meta_monitor_manager_get ();
  meta_monitor_manager_get_resources (monitors, NULL, NULL, &crtcs, &n_crtcs, NULL, NULL);

  if (cursor_sprite)
    rect = meta_cursor_renderer_calculate_rect (renderer, cursor_sprite);
  else
    rect = (MetaRectangle) { 0 };

  for (i = 0; i < n_crtcs; i++)
    {
      gboolean crtc_should_use_cursor;
      MetaCursorSprite *crtc_cursor;
      MetaRectangle *crtc_rect;

      crtc_rect = &crtcs[i].rect;

      crtc_should_use_cursor = (priv->has_hw_cursor &&
                                meta_rectangle_overlap (&rect, crtc_rect));
      if (crtc_should_use_cursor)
        crtc_cursor = cursor_sprite;
      else
        crtc_cursor = NULL;

      set_crtc_cursor (native, &crtcs[i], crtc_cursor, force);

      if (crtc_cursor)
        {
          drmModeMoveCursor (priv->drm_fd, crtcs[i].crtc_id,
                             rect.x - crtc_rect->x,
                             rect.y - crtc_rect->y);
        }
    }
}

static gboolean
has_valid_cursor_sprite_gbm_bo (MetaCursorSprite *cursor_sprite)
{
  MetaCursorNativePrivate *cursor_priv =
    g_object_get_qdata (G_OBJECT (cursor_sprite), quark_cursor_sprite);

  switch (cursor_priv->pending_bo_state)
    {
    case META_CURSOR_GBM_BO_STATE_NONE:
      return get_active_cursor_sprite_gbm_bo (cursor_sprite) != NULL;
    case META_CURSOR_GBM_BO_STATE_SET:
      return TRUE;
    case META_CURSOR_GBM_BO_STATE_INVALIDATED:
      return FALSE;
    }

  g_assert_not_reached ();

  return FALSE;
}

static gboolean
should_have_hw_cursor (MetaCursorRenderer *renderer,
                       MetaCursorSprite   *cursor_sprite)
{
  CoglTexture *texture;

  if (!cursor_sprite)
    return FALSE;

  texture = meta_cursor_sprite_get_cogl_texture (cursor_sprite);
  if (!texture)
    return FALSE;

  if (meta_cursor_sprite_get_texture_scale (cursor_sprite) != 1)
    return FALSE;

  if (!has_valid_cursor_sprite_gbm_bo (cursor_sprite))
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
  update_hw_cursor (native, cursor_sprite, FALSE);
  return priv->has_hw_cursor;
}

static void
get_hardware_cursor_size (MetaCursorRendererNative *native,
                          uint64_t *width, uint64_t *height)
{
  MetaCursorRendererNativePrivate *priv =
    meta_cursor_renderer_native_get_instance_private (native);

  *width = priv->cursor_width;
  *height = priv->cursor_height;
}

static void
cursor_priv_free (gpointer data)
{
  MetaCursorNativePrivate *cursor_priv = data;
  guint i;

  if (!data)
    return;

  for (i = 0; i < HW_CURSOR_BUFFER_COUNT; i++)
    g_clear_pointer (&cursor_priv->bos[0], (GDestroyNotify) gbm_bo_destroy);
  g_slice_free (MetaCursorNativePrivate, cursor_priv);
}

static MetaCursorNativePrivate *
ensure_cursor_priv (MetaCursorSprite *cursor_sprite)
{
  MetaCursorNativePrivate *cursor_priv =
    g_object_get_qdata (G_OBJECT (cursor_sprite), quark_cursor_sprite);

  if (!cursor_priv)
    {
      cursor_priv = g_slice_new0 (MetaCursorNativePrivate);
      g_object_set_qdata_full (G_OBJECT (cursor_sprite),
                               quark_cursor_sprite,
                               cursor_priv,
                               cursor_priv_free);
    }

  return cursor_priv;
}

static void
load_cursor_sprite_gbm_buffer (MetaCursorRendererNative *native,
                               MetaCursorSprite         *cursor_sprite,
                               uint8_t                  *pixels,
                               uint                      width,
                               uint                      height,
                               int                       rowstride,
                               uint32_t                  gbm_format)
{
  MetaCursorRendererNativePrivate *priv =
    meta_cursor_renderer_native_get_instance_private (native);
  uint64_t cursor_width, cursor_height;

  get_hardware_cursor_size (native, &cursor_width, &cursor_height);

  if (width > cursor_width || height > cursor_height)
    {
      meta_warning ("Invalid theme cursor size (must be at most %ux%u)\n",
                    (unsigned int)cursor_width, (unsigned int)cursor_height);
      return;
    }

  if (gbm_device_is_format_supported (priv->gbm, gbm_format,
                                      GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE))
    {
      struct gbm_bo *bo;
      uint8_t buf[4 * cursor_width * cursor_height];
      uint i;

      bo = gbm_bo_create (priv->gbm, cursor_width, cursor_height,
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

      set_pending_cursor_sprite_gbm_bo (cursor_sprite, bo);
    }
  else
    {
      meta_warning ("HW cursor for format %d not supported\n", gbm_format);
    }
}

static void
invalidate_pending_cursor_sprite_gbm_bo (MetaCursorSprite *cursor_sprite)
{
  MetaCursorNativePrivate *cursor_priv =
    g_object_get_qdata (G_OBJECT (cursor_sprite), quark_cursor_sprite);
  guint pending_bo;

  if (!cursor_priv)
    return;

  pending_bo = get_pending_cursor_sprite_gbm_bo_index (cursor_sprite);
  g_clear_pointer (&cursor_priv->bos[pending_bo],
                   (GDestroyNotify) gbm_bo_destroy);
  cursor_priv->pending_bo_state = META_CURSOR_GBM_BO_STATE_INVALIDATED;
}

#ifdef HAVE_WAYLAND
static void
meta_cursor_renderer_native_realize_cursor_from_wl_buffer (MetaCursorRenderer *renderer,
                                                           MetaCursorSprite *cursor_sprite,
                                                           struct wl_resource *buffer)
{
  MetaCursorRendererNative *native = META_CURSOR_RENDERER_NATIVE (renderer);
  MetaCursorRendererNativePrivate *priv =
    meta_cursor_renderer_native_get_instance_private (native);
  uint32_t gbm_format;
  uint64_t cursor_width, cursor_height;
  CoglTexture *texture;
  uint width, height;

  /* Destroy any previous pending cursor buffer; we'll always either fail (which
   * should unset, or succeed, which will set new buffer.
   */
  invalidate_pending_cursor_sprite_gbm_bo (cursor_sprite);

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
      load_cursor_sprite_gbm_buffer (native,
                                     cursor_sprite,
                                     buffer_data,
                                     width, height, rowstride,
                                     gbm_format);

      wl_shm_buffer_end_access (shm_buffer);
    }
  else
    {
      struct gbm_bo *bo;

      /* HW cursors have a predefined size (at least 64x64), which usually is
       * bigger than cursor theme size, so themed cursors must be padded with
       * transparent pixels to fill the overlay. This is trivial if we have CPU
       * access to the data, but it's not possible if the buffer is in GPU
       * memory (and possibly tiled too), so if we don't get the right size, we
       * fallback to GL. */
      get_hardware_cursor_size (native, &cursor_width, &cursor_height);

      if (width != cursor_width || height != cursor_height)
        {
          meta_warning ("Invalid cursor size (must be 64x64), falling back to software (GL) cursors\n");
          return;
        }

      bo = gbm_bo_import (priv->gbm,
                          GBM_BO_IMPORT_WL_BUFFER,
                          buffer,
                          GBM_BO_USE_CURSOR);
      if (!bo)
        {
          meta_warning ("Importing HW cursor from wl_buffer failed\n");
          return;
        }

      set_pending_cursor_sprite_gbm_bo (cursor_sprite, bo);
    }
}
#endif

static void
meta_cursor_renderer_native_realize_cursor_from_xcursor (MetaCursorRenderer *renderer,
                                                         MetaCursorSprite *cursor_sprite,
                                                         XcursorImage *xc_image)
{
  MetaCursorRendererNative *native = META_CURSOR_RENDERER_NATIVE (renderer);

  invalidate_pending_cursor_sprite_gbm_bo (cursor_sprite);

  load_cursor_sprite_gbm_buffer (native,
                                 cursor_sprite,
                                 (uint8_t *) xc_image->pixels,
                                 xc_image->width,
                                 xc_image->height,
                                 xc_image->width * 4,
                                 GBM_FORMAT_ARGB8888);
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
}

static void
force_update_hw_cursor (MetaCursorRendererNative *native)
{
  MetaCursorRenderer *renderer = META_CURSOR_RENDERER (native);

  update_hw_cursor (native, meta_cursor_renderer_get_cursor (renderer), TRUE);
}

static void
on_monitors_changed (MetaMonitorManager       *monitors,
                     MetaCursorRendererNative *native)
{
  /* Our tracking is all messed up, so force an update. */
  force_update_hw_cursor (native);
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

      uint64_t width, height;
      if (drmGetCap (priv->drm_fd, DRM_CAP_CURSOR_WIDTH, &width) == 0 &&
          drmGetCap (priv->drm_fd, DRM_CAP_CURSOR_HEIGHT, &height) == 0)
        {
          priv->cursor_width = width;
          priv->cursor_height = height;
        }
      else
        {
          priv->cursor_width = 64;
          priv->cursor_height = 64;
        }
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
  force_update_hw_cursor (native);
}
