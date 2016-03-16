/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Endless Mobile
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

#include "meta-wayland-buffer.h"

#include <clutter/clutter.h>
#include <cogl/cogl-wayland-server.h>
#include <meta/util.h>

enum
{
  RESOURCE_DESTROYED,

  LAST_SIGNAL
};

guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (MetaWaylandBuffer, meta_wayland_buffer, G_TYPE_OBJECT);

static void
meta_wayland_buffer_destroy_handler (struct wl_listener *listener,
                                     void *data)
{
  MetaWaylandBuffer *buffer =
    wl_container_of (listener, buffer, destroy_listener);

  buffer->resource = NULL;
  g_signal_emit (buffer, signals[RESOURCE_DESTROYED], 0);
  g_object_unref (buffer);
}

MetaWaylandBuffer *
meta_wayland_buffer_from_resource (struct wl_resource *resource)
{
  MetaWaylandBuffer *buffer;
  struct wl_listener *listener;

  listener =
    wl_resource_get_destroy_listener (resource,
                                      meta_wayland_buffer_destroy_handler);

  if (listener)
    {
      buffer = wl_container_of (listener, buffer, destroy_listener);
    }
  else
    {
      buffer = g_object_new (META_TYPE_WAYLAND_BUFFER, NULL);

      buffer->resource = resource;
      buffer->destroy_listener.notify = meta_wayland_buffer_destroy_handler;
      wl_resource_add_destroy_listener (resource, &buffer->destroy_listener);
    }

  return buffer;
}

CoglTexture *
meta_wayland_buffer_ensure_texture (MetaWaylandBuffer *buffer)
{
  CoglContext *ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
  CoglError *catch_error = NULL;
  CoglTexture *texture;
  struct wl_shm_buffer *shm_buffer;

  g_return_val_if_fail (buffer->resource, NULL);

  if (buffer->texture)
    goto out;

  shm_buffer = wl_shm_buffer_get (buffer->resource);

  if (shm_buffer)
    wl_shm_buffer_begin_access (shm_buffer);

  texture = COGL_TEXTURE (cogl_wayland_texture_2d_new_from_buffer (ctx,
                                                                   buffer->resource,
                                                                   &catch_error));

  if (shm_buffer)
    wl_shm_buffer_end_access (shm_buffer);

  if (!texture)
    {
      cogl_error_free (catch_error);
      meta_fatal ("Could not import pending buffer, ignoring commit\n");
    }

  buffer->texture = texture;

 out:
  return buffer->texture;
}

void
meta_wayland_buffer_process_damage (MetaWaylandBuffer *buffer,
                                    cairo_region_t    *region)
{
  struct wl_shm_buffer *shm_buffer;

  shm_buffer = wl_shm_buffer_get (buffer->resource);

  if (shm_buffer)
    {
      int i, n_rectangles;

      n_rectangles = cairo_region_num_rectangles (region);

      wl_shm_buffer_begin_access (shm_buffer);

      for (i = 0; i < n_rectangles; i++)
        {
          cairo_rectangle_int_t rect;
          cairo_region_get_rectangle (region, i, &rect);
          cogl_wayland_texture_set_region_from_shm_buffer (buffer->texture,
                                                           rect.x, rect.y, rect.width, rect.height,
                                                           shm_buffer,
                                                           rect.x, rect.y, 0, NULL);
        }

      wl_shm_buffer_end_access (shm_buffer);
    }
}

static void
meta_wayland_buffer_finalize (GObject *object)
{
  MetaWaylandBuffer *buffer = META_WAYLAND_BUFFER (object);

  g_clear_pointer (&buffer->texture, cogl_object_unref);

  G_OBJECT_CLASS (meta_wayland_buffer_parent_class)->finalize (object);
}

static void
meta_wayland_buffer_init (MetaWaylandBuffer *buffer)
{
}

static void
meta_wayland_buffer_class_init (MetaWaylandBufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_wayland_buffer_finalize;

  signals[RESOURCE_DESTROYED] = g_signal_new ("resource-destroyed",
                                              G_TYPE_FROM_CLASS (object_class),
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              NULL, NULL, NULL,
                                              G_TYPE_NONE, 0);
}
