/*
 * Wayland Support
 *
 * Copyright (C) 2015 Red Hat, Inc.
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
 */

#include "config.h"

#include <cogl/cogl.h>
#include <cogl/cogl-wayland-server.h>
#include "meta-wayland-cursor-surface.h"
#include "meta-wayland-buffer.h"
#include "meta-xwayland.h"
#include "screen-private.h"
#include "meta-wayland-private.h"
#include "backends/meta-backend-private.h"
#include "backends/meta-logical-monitor.h"
#include "core/boxes-private.h"
#include "wayland/meta-cursor-sprite-wayland.h"

typedef struct _MetaWaylandCursorSurfacePrivate MetaWaylandCursorSurfacePrivate;

struct _MetaWaylandCursorSurfacePrivate
{
  int hot_x;
  int hot_y;
  MetaCursorSpriteWayland *cursor_sprite;
  MetaCursorRenderer *cursor_renderer;
  MetaWaylandBuffer *buffer;
  struct wl_list frame_callbacks;
  gulong cursor_painted_handler_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (MetaWaylandCursorSurface,
                            meta_wayland_cursor_surface,
                            META_TYPE_WAYLAND_SURFACE_ROLE)

static void
update_cursor_sprite_texture (MetaWaylandCursorSurface *cursor_surface)
{
  MetaWaylandCursorSurfacePrivate *priv =
    meta_wayland_cursor_surface_get_instance_private (cursor_surface);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (META_WAYLAND_SURFACE_ROLE (cursor_surface));
  MetaWaylandBuffer *buffer = meta_wayland_surface_get_buffer (surface);
  MetaCursorSprite *cursor_sprite = META_CURSOR_SPRITE (priv->cursor_sprite);

  g_return_if_fail (!buffer || buffer->texture);

  if (!priv->cursor_renderer)
    return;

  if (buffer)
    {
      meta_cursor_sprite_set_texture (cursor_sprite,
                                      buffer->texture,
                                      priv->hot_x * surface->scale,
                                      priv->hot_y * surface->scale);

      if (priv->buffer)
        {
          struct wl_resource *buffer_resource;

          g_assert (priv->buffer == buffer);
          buffer_resource = buffer->resource;
          meta_cursor_renderer_realize_cursor_from_wl_buffer (priv->cursor_renderer,
                                                              cursor_sprite,
                                                              buffer_resource);

          meta_wayland_surface_unref_buffer_use_count (surface);
          g_clear_object (&priv->buffer);
        }
    }
  else
    {
      meta_cursor_sprite_set_texture (cursor_sprite, NULL, 0, 0);
    }

  meta_cursor_renderer_force_update (priv->cursor_renderer);
}

static void
cursor_sprite_prepare_at (MetaCursorSprite         *cursor_sprite,
                          int                       x,
                          int                       y,
                          MetaWaylandCursorSurface *cursor_surface)
{
  MetaWaylandSurfaceRole *role = META_WAYLAND_SURFACE_ROLE (cursor_surface);
  MetaWaylandSurface *surface = meta_wayland_surface_role_get_surface (role);

  if (!meta_xwayland_is_xwayland_surface (surface))
    {
      MetaBackend *backend = meta_get_backend ();
      MetaMonitorManager *monitor_manager =
        meta_backend_get_monitor_manager (backend);
      MetaLogicalMonitor *logical_monitor;

      logical_monitor =
        meta_monitor_manager_get_logical_monitor_at (monitor_manager, x, y);
      if (logical_monitor)
        {
          float texture_scale;

          if (meta_is_stage_views_scaled ())
            texture_scale = 1.0 / surface->scale;
          else
            texture_scale = (meta_logical_monitor_get_scale (logical_monitor) /
                             surface->scale);

          meta_cursor_sprite_set_texture_scale (cursor_sprite, texture_scale);
        }
    }
  meta_wayland_surface_update_outputs (surface);
}

static void
meta_wayland_cursor_surface_assigned (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWaylandCursorSurface *cursor_surface =
    META_WAYLAND_CURSOR_SURFACE (surface_role);
  MetaWaylandCursorSurfacePrivate *priv =
    meta_wayland_cursor_surface_get_instance_private (cursor_surface);

  wl_list_insert_list (&priv->frame_callbacks,
                       &surface->pending_frame_callback_list);
  wl_list_init (&surface->pending_frame_callback_list);
}

static void
meta_wayland_cursor_surface_pre_commit (MetaWaylandSurfaceRole  *surface_role,
                                        MetaWaylandPendingState *pending)
{
  MetaWaylandCursorSurface *cursor_surface =
    META_WAYLAND_CURSOR_SURFACE (surface_role);
  MetaWaylandCursorSurfacePrivate *priv =
    meta_wayland_cursor_surface_get_instance_private (cursor_surface);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  if (pending->newly_attached && priv->buffer)
    {
      meta_wayland_surface_unref_buffer_use_count (surface);
      g_clear_object (&priv->buffer);
    }
}

static void
meta_wayland_cursor_surface_commit (MetaWaylandSurfaceRole  *surface_role,
                                    MetaWaylandPendingState *pending)
{
  MetaWaylandCursorSurface *cursor_surface =
    META_WAYLAND_CURSOR_SURFACE (surface_role);
  MetaWaylandCursorSurfacePrivate *priv =
    meta_wayland_cursor_surface_get_instance_private (cursor_surface);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWaylandBuffer *buffer = meta_wayland_surface_get_buffer (surface);

  if (pending->newly_attached)
    {
      g_set_object (&priv->buffer, buffer);
      if (priv->buffer)
        meta_wayland_surface_ref_buffer_use_count (surface);
    }

  wl_list_insert_list (&priv->frame_callbacks,
                       &pending->frame_callback_list);
  wl_list_init (&pending->frame_callback_list);

  if (pending->newly_attached)
    update_cursor_sprite_texture (META_WAYLAND_CURSOR_SURFACE (surface_role));
}

static gboolean
meta_wayland_cursor_surface_is_on_logical_monitor (MetaWaylandSurfaceRole *role,
                                                   MetaLogicalMonitor     *logical_monitor)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (role);
  MetaWaylandCursorSurface *cursor_surface =
    META_WAYLAND_CURSOR_SURFACE (surface->role);
  MetaWaylandCursorSurfacePrivate *priv =
    meta_wayland_cursor_surface_get_instance_private (cursor_surface);
  ClutterPoint point;
  ClutterRect logical_monitor_rect;

  logical_monitor_rect =
    meta_rectangle_to_clutter_rect (&logical_monitor->rect);

  point = meta_cursor_renderer_get_position (priv->cursor_renderer);

  return clutter_rect_contains_point (&logical_monitor_rect, &point);
}

static void
meta_wayland_cursor_surface_dispose (GObject *object)
{
  MetaWaylandCursorSurface *cursor_surface =
    META_WAYLAND_CURSOR_SURFACE (object);
  MetaWaylandCursorSurfacePrivate *priv =
    meta_wayland_cursor_surface_get_instance_private (cursor_surface);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (META_WAYLAND_SURFACE_ROLE (object));
  MetaWaylandFrameCallback *cb, *next;

  wl_list_for_each_safe (cb, next, &priv->frame_callbacks, link)
    wl_resource_destroy (cb->resource);

  g_signal_handlers_disconnect_by_func (priv->cursor_sprite,
                                        cursor_sprite_prepare_at, cursor_surface);

  g_clear_object (&priv->cursor_renderer);
  g_clear_object (&priv->cursor_sprite);

  if (priv->buffer)
    {
      meta_wayland_surface_unref_buffer_use_count (surface);
      g_clear_object (&priv->buffer);
    }

  G_OBJECT_CLASS (meta_wayland_cursor_surface_parent_class)->dispose (object);
}

static void
meta_wayland_cursor_surface_constructed (GObject *object)
{
  MetaWaylandCursorSurface *cursor_surface =
    META_WAYLAND_CURSOR_SURFACE (object);
  MetaWaylandCursorSurfacePrivate *priv =
    meta_wayland_cursor_surface_get_instance_private (cursor_surface);
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (cursor_surface);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWaylandBuffer *buffer;

  buffer = meta_wayland_surface_get_buffer (surface);

  g_warn_if_fail (!buffer || buffer->resource);

  if (buffer && buffer->resource)
    {
      g_set_object (&priv->buffer, buffer);
      meta_wayland_surface_ref_buffer_use_count (surface);
    }
}

static void
meta_wayland_cursor_surface_init (MetaWaylandCursorSurface *role)
{
  MetaWaylandCursorSurfacePrivate *priv =
    meta_wayland_cursor_surface_get_instance_private (role);

  priv->cursor_sprite = meta_cursor_sprite_wayland_new ();
  g_signal_connect_object (priv->cursor_sprite,
                           "prepare-at",
                           G_CALLBACK (cursor_sprite_prepare_at),
                           role,
                           0);
  wl_list_init (&priv->frame_callbacks);
}

static void
meta_wayland_cursor_surface_class_init (MetaWaylandCursorSurfaceClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  surface_role_class->assigned = meta_wayland_cursor_surface_assigned;
  surface_role_class->pre_commit = meta_wayland_cursor_surface_pre_commit;
  surface_role_class->commit = meta_wayland_cursor_surface_commit;
  surface_role_class->is_on_logical_monitor =
    meta_wayland_cursor_surface_is_on_logical_monitor;

  object_class->constructed = meta_wayland_cursor_surface_constructed;
  object_class->dispose = meta_wayland_cursor_surface_dispose;
}

MetaCursorSprite *
meta_wayland_cursor_surface_get_sprite (MetaWaylandCursorSurface *cursor_surface)
{
  MetaWaylandCursorSurfacePrivate *priv =
    meta_wayland_cursor_surface_get_instance_private (cursor_surface);

  return META_CURSOR_SPRITE (priv->cursor_sprite);
}

void
meta_wayland_cursor_surface_set_hotspot (MetaWaylandCursorSurface *cursor_surface,
                                         int                       hotspot_x,
                                         int                       hotspot_y)
{
  MetaWaylandCursorSurfacePrivate *priv =
    meta_wayland_cursor_surface_get_instance_private (cursor_surface);

  if (priv->hot_x == hotspot_x &&
      priv->hot_y == hotspot_y)
    return;

  priv->hot_x = hotspot_x;
  priv->hot_y = hotspot_y;
  update_cursor_sprite_texture (cursor_surface);
}

void
meta_wayland_cursor_surface_get_hotspot (MetaWaylandCursorSurface *cursor_surface,
                                         int                      *hotspot_x,
                                         int                      *hotspot_y)
{
  MetaWaylandCursorSurfacePrivate *priv =
    meta_wayland_cursor_surface_get_instance_private (cursor_surface);

  if (hotspot_x)
    *hotspot_x = priv->hot_x;
  if (hotspot_y)
    *hotspot_y = priv->hot_y;
}

static void
on_cursor_painted (MetaCursorRenderer       *renderer,
                   MetaCursorSprite         *displayed_sprite,
                   MetaWaylandCursorSurface *cursor_surface)
{
  MetaWaylandCursorSurfacePrivate *priv =
    meta_wayland_cursor_surface_get_instance_private (cursor_surface);
  guint32 time = (guint32) (g_get_monotonic_time () / 1000);

  if (displayed_sprite != META_CURSOR_SPRITE (priv->cursor_sprite))
    return;

  while (!wl_list_empty (&priv->frame_callbacks))
    {
      MetaWaylandFrameCallback *callback =
        wl_container_of (priv->frame_callbacks.next, callback, link);

      wl_callback_send_done (callback->resource, time);
      wl_resource_destroy (callback->resource);
    }
}

void
meta_wayland_cursor_surface_set_renderer (MetaWaylandCursorSurface *cursor_surface,
                                          MetaCursorRenderer       *renderer)
{
  MetaWaylandCursorSurfacePrivate *priv =
    meta_wayland_cursor_surface_get_instance_private (cursor_surface);

  if (priv->cursor_renderer == renderer)
    return;

  if (priv->cursor_renderer)
    {
      g_signal_handler_disconnect (priv->cursor_renderer,
                                   priv->cursor_painted_handler_id);
      priv->cursor_painted_handler_id = 0;
      g_object_unref (priv->cursor_renderer);
    }
  if (renderer)
    {
      priv->cursor_painted_handler_id =
        g_signal_connect_object (renderer, "cursor-painted",
                                 G_CALLBACK (on_cursor_painted), cursor_surface, 0);
      g_object_ref (renderer);
    }

  priv->cursor_renderer = renderer;
  update_cursor_sprite_texture (cursor_surface);
}

MetaCursorRenderer *
meta_wayland_cursor_surface_get_renderer (MetaWaylandCursorSurface *cursor_surface)
{
  MetaWaylandCursorSurfacePrivate *priv =
    meta_wayland_cursor_surface_get_instance_private (cursor_surface);

  return priv->cursor_renderer;
}
