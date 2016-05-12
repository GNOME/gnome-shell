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
#include "meta-wayland-surface-role-cursor.h"
#include "meta-wayland-buffer.h"
#include "meta-xwayland.h"
#include "screen-private.h"

typedef struct _MetaWaylandSurfaceRoleCursorPrivate MetaWaylandSurfaceRoleCursorPrivate;

struct _MetaWaylandSurfaceRoleCursorPrivate
{
  int hot_x;
  int hot_y;
  MetaCursorSprite *cursor_sprite;
  MetaCursorRenderer *cursor_renderer;
  MetaWaylandBuffer *buffer;
};

G_DEFINE_TYPE_WITH_PRIVATE (MetaWaylandSurfaceRoleCursor,
                            meta_wayland_surface_role_cursor,
                            META_TYPE_WAYLAND_SURFACE_ROLE)

static void
update_cursor_sprite_texture (MetaWaylandSurfaceRoleCursor *cursor_role)
{
  MetaWaylandSurfaceRoleCursorPrivate *priv = meta_wayland_surface_role_cursor_get_instance_private (cursor_role);
  MetaWaylandSurface *surface = meta_wayland_surface_role_get_surface (META_WAYLAND_SURFACE_ROLE (cursor_role));
  MetaWaylandBuffer *buffer = meta_wayland_surface_get_buffer (surface);
  MetaCursorSprite *cursor_sprite = priv->cursor_sprite;

  g_return_if_fail (!buffer || buffer->texture);

  if (!priv->cursor_renderer || !cursor_sprite)
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
cursor_sprite_prepare_at (MetaCursorSprite             *cursor_sprite,
                          int                           x,
                          int                           y,
                          MetaWaylandSurfaceRoleCursor *cursor_role)
{
  MetaWaylandSurfaceRole *role = META_WAYLAND_SURFACE_ROLE (cursor_role);
  MetaWaylandSurface *surface = meta_wayland_surface_role_get_surface (role);
  MetaDisplay *display = meta_get_display ();
  MetaScreen *screen = display->screen;
  const MetaMonitorInfo *monitor;

  if (!meta_xwayland_is_xwayland_surface (surface))
    {
      monitor = meta_screen_get_monitor_for_point (screen, x, y);
      if (monitor)
        meta_cursor_sprite_set_texture_scale (cursor_sprite,
                                              (float) monitor->scale / surface->scale);
    }
  meta_wayland_surface_update_outputs (surface);
}

static void
cursor_surface_role_assigned (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandSurfaceRoleCursor *cursor_role =
    META_WAYLAND_SURFACE_ROLE_CURSOR (surface_role);
  MetaWaylandSurfaceRoleCursorPrivate *priv =
    meta_wayland_surface_role_cursor_get_instance_private (cursor_role);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWaylandBuffer *buffer = meta_wayland_surface_get_buffer (surface);

  if (buffer)
    {
      g_set_object (&priv->buffer, buffer);
      meta_wayland_surface_ref_buffer_use_count (surface);
    }

  meta_wayland_surface_queue_pending_frame_callbacks (surface);
}

static void
cursor_surface_role_pre_commit (MetaWaylandSurfaceRole  *surface_role,
                                MetaWaylandPendingState *pending)
{
  MetaWaylandSurfaceRoleCursor *cursor_role =
    META_WAYLAND_SURFACE_ROLE_CURSOR (surface_role);
  MetaWaylandSurfaceRoleCursorPrivate *priv =
    meta_wayland_surface_role_cursor_get_instance_private (cursor_role);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  if (pending->newly_attached && priv->buffer)
    {
      meta_wayland_surface_unref_buffer_use_count (surface);
      g_clear_object (&priv->buffer);
    }
}

static void
cursor_surface_role_commit (MetaWaylandSurfaceRole  *surface_role,
                            MetaWaylandPendingState *pending)
{
  MetaWaylandSurfaceRoleCursor *cursor_role =
    META_WAYLAND_SURFACE_ROLE_CURSOR (surface_role);
  MetaWaylandSurfaceRoleCursorPrivate *priv =
    meta_wayland_surface_role_cursor_get_instance_private (cursor_role);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWaylandBuffer *buffer = meta_wayland_surface_get_buffer (surface);

  if (pending->newly_attached)
    {
      g_set_object (&priv->buffer, buffer);
      if (priv->buffer)
        meta_wayland_surface_ref_buffer_use_count (surface);
    }

  meta_wayland_surface_queue_pending_state_frame_callbacks (surface, pending);

  if (pending->newly_attached)
    update_cursor_sprite_texture (META_WAYLAND_SURFACE_ROLE_CURSOR (surface_role));
}

static gboolean
cursor_surface_role_is_on_output (MetaWaylandSurfaceRole *role,
                                  MetaMonitorInfo        *monitor)
{
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (role);
  MetaWaylandSurfaceRoleCursor *cursor_role =
    META_WAYLAND_SURFACE_ROLE_CURSOR (surface->role);
  MetaWaylandSurfaceRoleCursorPrivate *priv =
    meta_wayland_surface_role_cursor_get_instance_private (cursor_role);
  MetaRectangle rect;

  rect = meta_cursor_renderer_calculate_rect (priv->cursor_renderer,
                                              priv->cursor_sprite);
  return meta_rectangle_overlap (&rect, &monitor->rect);
}

static void
cursor_surface_role_dispose (GObject *object)
{
  MetaWaylandSurfaceRoleCursor *cursor_role =
    META_WAYLAND_SURFACE_ROLE_CURSOR (object);
  MetaWaylandSurfaceRoleCursorPrivate *priv =
    meta_wayland_surface_role_cursor_get_instance_private (cursor_role);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (META_WAYLAND_SURFACE_ROLE (object));

  g_signal_handlers_disconnect_by_func (priv->cursor_sprite,
                                        cursor_sprite_prepare_at, cursor_role);

  g_clear_object (&priv->cursor_renderer);
  g_clear_object (&priv->cursor_sprite);

  if (priv->buffer)
    {
      meta_wayland_surface_unref_buffer_use_count (surface);
      g_clear_object (&priv->buffer);
    }

  G_OBJECT_CLASS (meta_wayland_surface_role_cursor_parent_class)->dispose (object);
}

static void
meta_wayland_surface_role_cursor_init (MetaWaylandSurfaceRoleCursor *role)
{
  MetaWaylandSurfaceRoleCursorPrivate *priv =
    meta_wayland_surface_role_cursor_get_instance_private (role);

  priv->cursor_sprite = meta_cursor_sprite_new ();
  g_signal_connect_object (priv->cursor_sprite,
                           "prepare-at",
                           G_CALLBACK (cursor_sprite_prepare_at),
                           role,
                           0);
}

static void
meta_wayland_surface_role_cursor_class_init (MetaWaylandSurfaceRoleCursorClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  surface_role_class->assigned = cursor_surface_role_assigned;
  surface_role_class->pre_commit = cursor_surface_role_pre_commit;
  surface_role_class->commit = cursor_surface_role_commit;
  surface_role_class->is_on_output = cursor_surface_role_is_on_output;

  object_class->dispose = cursor_surface_role_dispose;
}

MetaCursorSprite *
meta_wayland_surface_role_cursor_get_sprite (MetaWaylandSurfaceRoleCursor *cursor_role)
{
  MetaWaylandSurfaceRoleCursorPrivate *priv =
    meta_wayland_surface_role_cursor_get_instance_private (cursor_role);

  return priv->cursor_sprite;
}

void
meta_wayland_surface_role_cursor_set_hotspot (MetaWaylandSurfaceRoleCursor *cursor_role,
                                              gint                          hotspot_x,
                                              gint                          hotspot_y)
{
  MetaWaylandSurfaceRoleCursorPrivate *priv =
    meta_wayland_surface_role_cursor_get_instance_private (cursor_role);

  if (priv->hot_x == hotspot_x &&
      priv->hot_y == hotspot_y)
    return;

  priv->hot_x = hotspot_x;
  priv->hot_y = hotspot_y;
  update_cursor_sprite_texture (cursor_role);
}

void
meta_wayland_surface_role_cursor_get_hotspot (MetaWaylandSurfaceRoleCursor *cursor_role,
                                              gint                         *hotspot_x,
                                              gint                         *hotspot_y)
{
  MetaWaylandSurfaceRoleCursorPrivate *priv =
    meta_wayland_surface_role_cursor_get_instance_private (cursor_role);

  if (hotspot_x)
    *hotspot_x = priv->hot_x;
  if (hotspot_y)
    *hotspot_y = priv->hot_y;
}

void
meta_wayland_surface_role_cursor_set_renderer (MetaWaylandSurfaceRoleCursor *cursor_role,
                                               MetaCursorRenderer           *renderer)
{
  MetaWaylandSurfaceRoleCursorPrivate *priv =
    meta_wayland_surface_role_cursor_get_instance_private (cursor_role);

  if (priv->cursor_renderer == renderer)
    return;

  if (renderer)
    g_object_ref (renderer);
  if (priv->cursor_renderer)
    g_object_unref (priv->cursor_renderer);

  priv->cursor_renderer = renderer;
  update_cursor_sprite_texture (cursor_role);
}

MetaCursorRenderer *
meta_wayland_surface_role_cursor_get_renderer (MetaWaylandSurfaceRoleCursor *cursor_role)
{
  MetaWaylandSurfaceRoleCursorPrivate *priv =
    meta_wayland_surface_role_cursor_get_instance_private (cursor_role);

  return priv->cursor_renderer;
}
