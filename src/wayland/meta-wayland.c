/*
 * Wayland Support
 *
 * Copyright (C) 2012,2013 Intel Corporation
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

#include <config.h>

#include <clutter/clutter.h>
#include <clutter/wayland/clutter-wayland-compositor.h>
#include <clutter/wayland/clutter-wayland-surface.h>

#include <glib.h>
#include <sys/time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/wait.h>

#include <wayland-server.h>

#include "xserver-server-protocol.h"

#include "meta-wayland-private.h"
#include "meta-xwayland-private.h"
#include "meta-window-actor-private.h"
#include "display-private.h"
#include "window-private.h"
#include <meta/types.h>
#include <meta/main.h>
#include "frame.h"

static MetaWaylandCompositor _meta_wayland_compositor;

MetaWaylandCompositor *
meta_wayland_compositor_get_default (void)
{
  return &_meta_wayland_compositor;
}

static guint32
get_time (void)
{
  struct timeval tv;
  gettimeofday (&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static gboolean
wayland_event_source_prepare (GSource *base, int *timeout)
{
  WaylandEventSource *source = (WaylandEventSource *)base;

  *timeout = -1;

  wl_display_flush_clients (source->display);

  return FALSE;
}

static gboolean
wayland_event_source_check (GSource *base)
{
  WaylandEventSource *source = (WaylandEventSource *)base;
  return source->pfd.revents;
}

static gboolean
wayland_event_source_dispatch (GSource *base,
                               GSourceFunc callback,
                               void *data)
{
  WaylandEventSource *source = (WaylandEventSource *)base;
  struct wl_event_loop *loop = wl_display_get_event_loop (source->display);

  wl_event_loop_dispatch (loop, 0);

  return TRUE;
}

static GSourceFuncs wayland_event_source_funcs =
{
  wayland_event_source_prepare,
  wayland_event_source_check,
  wayland_event_source_dispatch,
  NULL
};

static GSource *
wayland_event_source_new (struct wl_display *display)
{
  WaylandEventSource *source;
  struct wl_event_loop *loop = wl_display_get_event_loop (display);

  source = (WaylandEventSource *) g_source_new (&wayland_event_source_funcs,
                                                sizeof (WaylandEventSource));
  source->display = display;
  source->pfd.fd = wl_event_loop_get_fd (loop);
  source->pfd.events = G_IO_IN | G_IO_ERR;
  g_source_add_poll (&source->source, &source->pfd);

  return &source->source;
}

static void
meta_wayland_buffer_destroy_handler (struct wl_listener *listener,
                                     void *data)
{
  MetaWaylandBuffer *buffer =
    wl_container_of (listener, buffer, destroy_listener);

  wl_signal_emit (&buffer->destroy_signal, buffer);
  g_slice_free (MetaWaylandBuffer, buffer);
}

static MetaWaylandBuffer *
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
      buffer = g_slice_new0 (MetaWaylandBuffer);

      buffer->resource = resource;
      wl_signal_init (&buffer->destroy_signal);
      buffer->destroy_listener.notify = meta_wayland_buffer_destroy_handler;
      wl_resource_add_destroy_listener (resource, &buffer->destroy_listener);
    }

  return buffer;
}

static void
meta_wayland_buffer_reference_handle_destroy (struct wl_listener *listener,
                                          void *data)
{
  MetaWaylandBufferReference *ref =
    wl_container_of (listener, ref, destroy_listener);

  g_assert (data == ref->buffer);

  ref->buffer = NULL;
}

static void
meta_wayland_buffer_reference (MetaWaylandBufferReference *ref,
                               MetaWaylandBuffer *buffer)
{
  if (ref->buffer && buffer != ref->buffer)
    {
      ref->buffer->busy_count--;

      if (ref->buffer->busy_count == 0)
        {
          g_assert (wl_resource_get_client (ref->buffer->resource));
          wl_resource_queue_event (ref->buffer->resource, WL_BUFFER_RELEASE);
        }

      wl_list_remove (&ref->destroy_listener.link);
    }

  if (buffer && buffer != ref->buffer)
    {
      buffer->busy_count++;
      wl_signal_add (&buffer->destroy_signal, &ref->destroy_listener);
    }

  ref->buffer = buffer;
  ref->destroy_listener.notify = meta_wayland_buffer_reference_handle_destroy;
}

static void
surface_process_damage (MetaWaylandSurface *surface,
                        cairo_region_t *region)
{
  if (surface->window &&
      surface->buffer_ref.buffer)
    {
      MetaWindowActor *window_actor =
        META_WINDOW_ACTOR (meta_window_get_compositor_private (surface->window));

      if (window_actor)
        {
          int i, n_rectangles = cairo_region_num_rectangles (region);

          for (i = 0; i < n_rectangles; i++)
            {
              cairo_rectangle_int_t rectangle;

              cairo_region_get_rectangle (region, i, &rectangle);

              meta_window_actor_process_wayland_damage (window_actor,
                                                        rectangle.x,
                                                        rectangle.y,
                                                        rectangle.width,
                                                        rectangle.height);
            }
        }
    }
}

static void
meta_wayland_surface_destroy (struct wl_client *wayland_client,
                              struct wl_resource *wayland_resource)
{
  wl_resource_destroy (wayland_resource);
}

static void
meta_wayland_surface_attach (struct wl_client *wayland_client,
                             struct wl_resource *wayland_surface_resource,
                             struct wl_resource *wayland_buffer_resource,
                             gint32 sx, gint32 sy)
{
  MetaWaylandSurface *surface =
    wl_resource_get_user_data (wayland_surface_resource);
  MetaWaylandBuffer *buffer;

  if (wayland_buffer_resource)
    buffer = meta_wayland_buffer_from_resource (wayland_buffer_resource);
  else
    buffer = NULL;

  /* Attach without commit in between does not send wl_buffer.release */
  if (surface->pending.buffer)
    wl_list_remove (&surface->pending.buffer_destroy_listener.link);

  surface->pending.sx = sx;
  surface->pending.sy = sy;
  surface->pending.buffer = buffer;
  surface->pending.newly_attached = TRUE;

  if (buffer)
    wl_signal_add (&buffer->destroy_signal,
                   &surface->pending.buffer_destroy_listener);
}

static void
meta_wayland_surface_damage (struct wl_client *client,
                             struct wl_resource *surface_resource,
                             gint32 x,
                             gint32 y,
                             gint32 width,
                             gint32 height)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  cairo_rectangle_int_t rectangle = { x, y, width, height };

  cairo_region_union_rectangle (surface->pending.damage, &rectangle);
}

static void
destroy_frame_callback (struct wl_resource *callback_resource)
{
  MetaWaylandFrameCallback *callback =
    wl_resource_get_user_data (callback_resource);

  wl_list_remove (&callback->link);
  g_slice_free (MetaWaylandFrameCallback, callback);
}

static void
meta_wayland_surface_frame (struct wl_client *client,
                            struct wl_resource *surface_resource,
                            guint32 callback_id)
{
  MetaWaylandFrameCallback *callback;
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);

  callback = g_slice_new0 (MetaWaylandFrameCallback);
  callback->compositor = surface->compositor;
  callback->resource = wl_resource_create (client,
					   &wl_callback_interface, 1,
					   callback_id);
  wl_resource_set_user_data (callback->resource, callback);
  wl_resource_set_destructor (callback->resource, destroy_frame_callback);

  wl_list_insert (surface->pending.frame_callback_list.prev, &callback->link);
}

static void
meta_wayland_surface_set_opaque_region (struct wl_client *client,
                                        struct wl_resource *resource,
                                        struct wl_resource *region)
{
  g_warning ("TODO: support set_opaque_region request");
}

static void
meta_wayland_surface_set_input_region (struct wl_client *client,
                                       struct wl_resource *resource,
                                       struct wl_resource *region)
{
  g_warning ("TODO: support set_input_region request");
}

static void
empty_region (cairo_region_t *region)
{
  cairo_rectangle_int_t rectangle = { 0, 0, 0, 0 };
  cairo_region_intersect_rectangle (region, &rectangle);
}

static void
meta_wayland_surface_commit (struct wl_client *client,
                             struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  MetaWaylandCompositor *compositor = surface->compositor;

  /* wl_surface.attach */
  if (surface->pending.newly_attached &&
      surface->buffer_ref.buffer != surface->pending.buffer)
    {
      /* Note: we set this before informing any window-actor since the
       * window actor will expect to find the new buffer within the
       * surface. */
      meta_wayland_buffer_reference (&surface->buffer_ref,
                                     surface->pending.buffer);

      if (surface->pending.buffer)
        {
          MetaWaylandBuffer *buffer = surface->pending.buffer;

          if (surface->window)
            {
              MetaWindow *window = surface->window;
              MetaWindowActor *window_actor =
                META_WINDOW_ACTOR (meta_window_get_compositor_private (window));
              MetaRectangle rect;

              meta_window_get_input_rect (surface->window, &rect);

              if (window_actor)
                meta_window_actor_attach_wayland_buffer (window_actor, buffer);

              /* XXX: we resize X based surfaces according to X events */
              if (surface->xid == 0 &&
                  (buffer->width != rect.width || buffer->height != rect.height))
                meta_window_resize (surface->window, FALSE, buffer->width, buffer->height);
            }
        }
    }

  if (surface->pending.buffer)
    {
      wl_list_remove (&surface->pending.buffer_destroy_listener.link);
      surface->pending.buffer = NULL;
    }
  surface->pending.sx = 0;
  surface->pending.sy = 0;
  surface->pending.newly_attached = FALSE;

  surface_process_damage (surface, surface->pending.damage);
  empty_region (surface->pending.damage);

  /* wl_surface.frame */
  wl_list_insert_list (&compositor->frame_callbacks,
                       &surface->pending.frame_callback_list);
  wl_list_init (&surface->pending.frame_callback_list);
}

static void
meta_wayland_surface_set_buffer_transform (struct wl_client *client,
                                           struct wl_resource *resource,
                                           int32_t transform)
{
  g_warning ("TODO: support set_buffer_transform request");
}

static void
meta_wayland_surface_set_buffer_scale (struct wl_client *client,
                                       struct wl_resource *resource,
                                       int scale)
{
  g_warning ("TODO: support set_buffer_scale request");
}

const struct wl_surface_interface meta_wayland_surface_interface = {
  meta_wayland_surface_destroy,
  meta_wayland_surface_attach,
  meta_wayland_surface_damage,
  meta_wayland_surface_frame,
  meta_wayland_surface_set_opaque_region,
  meta_wayland_surface_set_input_region,
  meta_wayland_surface_commit,
  meta_wayland_surface_set_buffer_transform,
  meta_wayland_surface_set_buffer_scale
};

static void
window_destroyed_cb (void *user_data, GObject *old_object)
{
  MetaWaylandSurface *surface = user_data;

  surface->window = NULL;
}

static void
meta_wayland_surface_free (MetaWaylandSurface *surface)
{
  MetaWaylandCompositor *compositor = surface->compositor;
  MetaWaylandFrameCallback *cb, *next;

  compositor->surfaces = g_list_remove (compositor->surfaces, surface);

  meta_wayland_buffer_reference (&surface->buffer_ref, NULL);

  if (surface->window)
    g_object_weak_unref (G_OBJECT (surface->window),
                         window_destroyed_cb,
                         surface);

  /* NB: If the surface corresponds to an X window then we will be
   * sure to free the MetaWindow according to some X event. */
  if (surface->window &&
      surface->window->client_type == META_WINDOW_CLIENT_TYPE_WAYLAND)
    {
      MetaDisplay *display = meta_get_display ();
      guint32 timestamp = meta_display_get_current_time_roundtrip (display);
      meta_window_unmanage (surface->window, timestamp);
    }

  if (surface->pending.buffer)
    wl_list_remove (&surface->pending.buffer_destroy_listener.link);

  cairo_region_destroy (surface->pending.damage);

  wl_list_for_each_safe (cb, next,
                         &surface->pending.frame_callback_list, link)
    wl_resource_destroy (cb->resource);

  g_slice_free (MetaWaylandSurface, surface);
}

static void
meta_wayland_surface_resource_destroy_cb (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  meta_wayland_surface_free (surface);
}

static void
surface_handle_pending_buffer_destroy (struct wl_listener *listener,
                                       void *data)
{
  MetaWaylandSurface *surface =
    wl_container_of (listener, surface, pending.buffer_destroy_listener);

  surface->pending.buffer = NULL;
}

static void
meta_wayland_compositor_create_surface (struct wl_client *wayland_client,
                                        struct wl_resource *wayland_compositor_resource,
                                        guint32 id)
{
  MetaWaylandCompositor *compositor =
    wl_resource_get_user_data (wayland_compositor_resource);
  MetaWaylandSurface *surface = g_slice_new0 (MetaWaylandSurface);

  surface->compositor = compositor;

  /* a surface inherits the version from the compositor */
  surface->resource = wl_resource_create (wayland_client,
					  &wl_surface_interface,
					  wl_resource_get_version (wayland_compositor_resource),
					  id);
  wl_resource_set_implementation (surface->resource, &meta_wayland_surface_interface, surface,
				  meta_wayland_surface_resource_destroy_cb);

  surface->pending.damage = cairo_region_create ();

  surface->pending.buffer_destroy_listener.notify =
    surface_handle_pending_buffer_destroy;
  wl_list_init (&surface->pending.frame_callback_list);

  compositor->surfaces = g_list_prepend (compositor->surfaces, surface);
}

static void
meta_wayland_region_destroy (struct wl_client *client,
                             struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
meta_wayland_region_add (struct wl_client *client,
                         struct wl_resource *resource,
                         gint32 x,
                         gint32 y,
                         gint32 width,
                         gint32 height)
{
  MetaWaylandRegion *region = wl_resource_get_user_data (resource);
  cairo_rectangle_int_t rectangle = { x, y, width, height };

  cairo_region_union_rectangle (region->region, &rectangle);
}

static void
meta_wayland_region_subtract (struct wl_client *client,
                              struct wl_resource *resource,
                              gint32 x,
                              gint32 y,
                              gint32 width,
                              gint32 height)
{
  MetaWaylandRegion *region = wl_resource_get_user_data (resource);
  cairo_rectangle_int_t rectangle = { x, y, width, height };

  cairo_region_subtract_rectangle (region->region, &rectangle);
}

const struct wl_region_interface meta_wayland_region_interface = {
  meta_wayland_region_destroy,
  meta_wayland_region_add,
  meta_wayland_region_subtract
};

static void
meta_wayland_region_resource_destroy_cb (struct wl_resource *resource)
{
  MetaWaylandRegion *region = wl_resource_get_user_data (resource);

  cairo_region_destroy (region->region);
  g_slice_free (MetaWaylandRegion, region);
}

static void
meta_wayland_compositor_create_region (struct wl_client *wayland_client,
                                       struct wl_resource *compositor_resource,
                                       uint32_t id)
{
  MetaWaylandRegion *region = g_slice_new0 (MetaWaylandRegion);

  region->resource = wl_resource_create (wayland_client,
					 &wl_region_interface, 1,
					 id);
  wl_resource_set_implementation (region->resource,
				  &meta_wayland_region_interface, region,
				  meta_wayland_region_resource_destroy_cb);

  region->region = cairo_region_create ();
}

static void
bind_output (struct wl_client *client,
             void *data,
             guint32 version,
             guint32 id)
{
  MetaWaylandOutput *output = data;
  struct wl_resource *resource =
    wl_resource_create (client, &wl_output_interface, version, id);
  GList *l;

  wl_resource_post_event (resource,
                          WL_OUTPUT_GEOMETRY,
                          output->x, output->y,
                          output->width_mm,
                          output->height_mm,
                          0, /* subpixel: unknown */
                          "unknown", /* make */
                          "unknown"); /* model */

  for (l = output->modes; l; l = l->next)
    {
      MetaWaylandMode *mode = l->data;
      wl_resource_post_event (resource,
                              WL_OUTPUT_MODE,
                              mode->flags,
                              mode->width,
                              mode->height,
                              mode->refresh);
    }
}

static void
meta_wayland_compositor_create_output (MetaWaylandCompositor *compositor,
                                       int x,
                                       int y,
                                       int width,
                                       int height,
                                       int width_mm,
                                       int height_mm)
{
  MetaWaylandOutput *output = g_slice_new0 (MetaWaylandOutput);
  MetaWaylandMode *mode;
  float final_width, final_height;

  /* XXX: eventually we will support sliced stages and an output should
   * correspond to a slice/CoglFramebuffer, but for now we only support
   * one output so we make sure it always matches the size of the stage
   */
  clutter_actor_set_size (compositor->stage, width, height);

  /* Read back the actual size we were given.
   * XXX: This really needs re-thinking later though so we know the
   * correct output geometry to use. */
  clutter_actor_get_size (compositor->stage, &final_width, &final_height);
  width = final_width;
  height = final_height;

  output->wayland_output.interface = &wl_output_interface;

  output->x = x;
  output->y = y;
  output->width_mm = width_mm;
  output->height_mm = height_mm;

  wl_global_create (compositor->wayland_display,
		    &wl_output_interface, 2,
		    output, bind_output);

  mode = g_slice_new0 (MetaWaylandMode);
  mode->flags = WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
  mode->width = width;
  mode->height = height;
  mode->refresh = 60;

  output->modes = g_list_prepend (output->modes, mode);

  compositor->outputs = g_list_prepend (compositor->outputs, output);
}

const static struct wl_compositor_interface meta_wayland_compositor_interface = {
  meta_wayland_compositor_create_surface,
  meta_wayland_compositor_create_region
};

static void
paint_finished_cb (ClutterActor *self, void *user_data)
{
  MetaWaylandCompositor *compositor = user_data;

  while (!wl_list_empty (&compositor->frame_callbacks))
    {
      MetaWaylandFrameCallback *callback =
        wl_container_of (compositor->frame_callbacks.next, callback, link);

      wl_resource_post_event (callback->resource,
                              WL_CALLBACK_DONE, get_time ());
      wl_resource_destroy (callback->resource);
    }
}

static void
compositor_bind (struct wl_client *client,
		 void *data,
                 guint32 version,
                 guint32 id)
{
  MetaWaylandCompositor *compositor = data;
  struct wl_resource *resource;

  resource = wl_resource_create (client, &wl_compositor_interface, version, id);
  wl_resource_set_implementation (resource, &meta_wayland_compositor_interface, compositor, NULL);
}

static void
shell_surface_pong (struct wl_client *client,
                    struct wl_resource *resource,
                    guint32 serial)
{
}

static void
shell_surface_move (struct wl_client *client,
                    struct wl_resource *resource,
                    struct wl_resource *seat,
                    guint32 serial)
{
}

static void
shell_surface_resize (struct wl_client *client,
                      struct wl_resource *resource,
                      struct wl_resource *seat,
                      guint32 serial,
                      guint32 edges)
{
  g_warning ("TODO: support shell_surface_resize request");
}

static void
ensure_surface_window (MetaWaylandSurface *surface)
{
  MetaDisplay *display = meta_get_display ();

  if (!surface->window)
    {
      int width, height;

      if (surface->buffer_ref.buffer)
        {
          MetaWaylandBuffer *buffer = surface->buffer_ref.buffer;
          width = buffer->width;
          height = buffer->width;
        }
      else
        {
          width = 0;
          height = 0;
        }

      surface->window =
        meta_window_new_for_wayland (display, width, height, surface);

      /* If the MetaWindow becomes unmanaged (surface->window will be
       * freed in this case) we need to make sure to clear our
       * ->window pointers. */
      g_object_weak_ref (G_OBJECT (surface->window),
                         window_destroyed_cb,
                         surface);

      meta_window_calc_showing (surface->window);
    }
}

static void
shell_surface_set_toplevel (struct wl_client *client,
                            struct wl_resource *resource)
{
  MetaWaylandCompositor *compositor = &_meta_wayland_compositor;
  MetaWaylandShellSurface *shell_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = shell_surface->surface;

  /* NB: Surfaces from xwayland become managed based on X events. */
  if (client == compositor->xwayland_client)
    return;

  ensure_surface_window (surface);

  meta_window_unmake_fullscreen (surface->window);
}

static void
shell_surface_set_transient (struct wl_client *client,
                             struct wl_resource *resource,
                             struct wl_resource *parent,
                             int x,
                             int y,
                             guint32 flags)
{
  MetaWaylandCompositor *compositor = &_meta_wayland_compositor;
  MetaWaylandShellSurface *shell_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = shell_surface->surface;

  /* NB: Surfaces from xwayland become managed based on X events. */
  if (client == compositor->xwayland_client)
    return;

  ensure_surface_window (surface);
}

static void
shell_surface_set_fullscreen (struct wl_client *client,
                              struct wl_resource *resource,
                              guint32 method,
                              guint32 framerate,
                              struct wl_resource *output)
{
  MetaWaylandCompositor *compositor = &_meta_wayland_compositor;
  MetaWaylandShellSurface *shell_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = shell_surface->surface;

  /* NB: Surfaces from xwayland become managed based on X events. */
  if (client == compositor->xwayland_client)
    return;

  ensure_surface_window (surface);

  meta_window_make_fullscreen (surface->window);
}

static void
shell_surface_set_popup (struct wl_client *client,
                         struct wl_resource *resource,
                         struct wl_resource *seat,
                         guint32 serial,
                         struct wl_resource *parent,
                         gint32 x,
                         gint32 y,
                         guint32 flags)
{
}

static void
shell_surface_set_maximized (struct wl_client *client,
                             struct wl_resource *resource,
                             struct wl_resource *output)
{
  g_warning ("TODO: support shell_surface_set_maximized request");
}

static void
shell_surface_set_title (struct wl_client *client,
                         struct wl_resource *resource,
                         const char *title)
{
  g_warning ("TODO: support shell_surface_set_title request");
}

static void
shell_surface_set_class (struct wl_client *client,
                         struct wl_resource *resource,
                         const char *class_)
{
  g_warning ("TODO: support shell_surface_set_class request");
}

static const struct wl_shell_surface_interface meta_wayland_shell_surface_interface =
{
  shell_surface_pong,
  shell_surface_move,
  shell_surface_resize,
  shell_surface_set_toplevel,
  shell_surface_set_transient,
  shell_surface_set_fullscreen,
  shell_surface_set_popup,
  shell_surface_set_maximized,
  shell_surface_set_title,
  shell_surface_set_class
};

static void
shell_handle_surface_destroy (struct wl_listener *listener,
                              void *data)
{
  MetaWaylandShellSurface *shell_surface =
    wl_container_of (listener, shell_surface, surface_destroy_listener);
  shell_surface->surface->has_shell_surface = FALSE;
  shell_surface->surface = NULL;
  wl_resource_destroy (shell_surface->resource);
}

static void
destroy_shell_surface (struct wl_resource *resource)
{
  MetaWaylandShellSurface *shell_surface = wl_resource_get_user_data (resource);

  /* In case cleaning up a dead client destroys shell_surface first */
  if (shell_surface->surface)
    {
      wl_list_remove (&shell_surface->surface_destroy_listener.link);
      shell_surface->surface->has_shell_surface = FALSE;
    }

  g_free (shell_surface);
}

static void
get_shell_surface (struct wl_client *client,
                   struct wl_resource *resource,
                   guint32 id,
                   struct wl_resource *surface_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWaylandShellSurface *shell_surface;

  if (surface->has_shell_surface)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "wl_shell::get_shell_surface already requested");
      return;
    }

  shell_surface = g_new0 (MetaWaylandShellSurface, 1);

  /* a shell surface inherits the version from the shell */
  shell_surface->resource =
    wl_resource_create (client, &wl_shell_surface_interface,
			wl_resource_get_version (resource), id);
  wl_resource_set_implementation (shell_surface->resource, &meta_wayland_shell_surface_interface,
				  shell_surface, destroy_shell_surface);

  shell_surface->surface = surface;
  shell_surface->surface_destroy_listener.notify = shell_handle_surface_destroy;
  wl_resource_add_destroy_listener (surface->resource,
                                    &shell_surface->surface_destroy_listener);
  surface->has_shell_surface = TRUE;
}

static const struct wl_shell_interface meta_wayland_shell_interface =
{
  get_shell_surface
};

static void
bind_shell (struct wl_client *client,
            void *data,
            guint32 version,
            guint32 id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &wl_shell_interface, version, id);
  wl_resource_set_implementation (resource, &meta_wayland_shell_interface, data, NULL);
}

static void
xserver_set_window_id (struct wl_client *client,
                       struct wl_resource *compositor_resource,
                       struct wl_resource *surface_resource,
                       guint32 xid)
{
  MetaWaylandCompositor *compositor =
    wl_resource_get_user_data (compositor_resource);
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaDisplay *display = meta_get_display ();
  MetaWindow *window;

  g_return_if_fail (surface->xid == None);

  surface->xid = xid;

  g_hash_table_insert (compositor->window_surfaces, &xid, surface);

  window  = meta_display_lookup_x_window (display, xid);
  if (window)
    {
      MetaWindowActor *window_actor =
        META_WINDOW_ACTOR (meta_window_get_compositor_private (window));

      meta_window_actor_set_wayland_surface (window_actor, surface);

      surface->window = window;
      window->surface = surface;

      /* If the MetaWindow becomes unmanaged (surface->window will be
       * freed in this case) we need to make sure to clear our
       * ->window pointers in this case. */
      g_object_weak_ref (G_OBJECT (surface->window),
                         window_destroyed_cb,
                         surface);
    }
#warning "FIXME: Handle surface destroy and remove window_surfaces mapping"
}

MetaWaylandSurface *
meta_wayland_lookup_surface_for_xid (guint32 xid)
{
  return g_hash_table_lookup (_meta_wayland_compositor.window_surfaces, &xid);
}

static const struct xserver_interface xserver_implementation = {
    xserver_set_window_id
};

static void
bind_xserver (struct wl_client *client,
	      void *data,
              guint32 version,
              guint32 id)
{
  MetaWaylandCompositor *compositor = data;

  /* If it's a different client than the xserver we launched,
   * don't start the wm. */
  if (client != compositor->xwayland_client)
    return;

  compositor->xserver_resource =
    wl_resource_create (client, &xserver_interface, version, id);
  wl_resource_set_implementation (compositor->xserver_resource,
				  &xserver_implementation, compositor, NULL);

  wl_resource_post_event (compositor->xserver_resource,
                          XSERVER_LISTEN_SOCKET,
                          compositor->xwayland_abstract_fd);

  wl_resource_post_event (compositor->xserver_resource,
                          XSERVER_LISTEN_SOCKET,
                          compositor->xwayland_unix_fd);

  /* Make sure xwayland will recieve the above sockets in a finite
   * time before unblocking the initialization mainloop since we are
   * then going to immediately try and connect to those as the window
   * manager. */
  wl_client_flush (client);

  /* At this point xwayland is all setup to start accepting
   * connections so we can quit the transient initialization mainloop
   * and unblock meta_wayland_init() to continue initializing mutter.
   * */
  g_main_loop_quit (compositor->init_loop);
  compositor->init_loop = NULL;
}

static void
stage_destroy_cb (void)
{
  meta_quit (META_EXIT_SUCCESS);
}

void
meta_wayland_init (void)
{
  MetaWaylandCompositor *compositor = &_meta_wayland_compositor;

  memset (compositor, 0, sizeof (MetaWaylandCompositor));

  compositor->wayland_display = wl_display_create ();
  if (compositor->wayland_display == NULL)
    g_error ("failed to create wayland display");

  wl_display_init_shm (compositor->wayland_display);

  wl_list_init (&compositor->frame_callbacks);

  if (!wl_global_create (compositor->wayland_display,
			 &wl_compositor_interface, 3,
			 compositor, compositor_bind))
    g_error ("Failed to register wayland compositor object");

  compositor->wayland_loop =
    wl_display_get_event_loop (compositor->wayland_display);
  compositor->wayland_event_source =
    wayland_event_source_new (compositor->wayland_display);

  /* XXX: Here we are setting the wayland event source to have a
   * slightly lower priority than the X event source, because we are
   * much more likely to get confused being told about surface changes
   * relating to X clients when we don't know what's happened to them
   * according to the X protocol.
   *
   * At some point we could perhaps try and get the X protocol proxied
   * over the wayland protocol so that we don't have to worry about
   * synchronizing the two command streams. */
  g_source_set_priority (compositor->wayland_event_source,
                         GDK_PRIORITY_EVENTS + 1);
  g_source_attach (compositor->wayland_event_source, NULL);

  clutter_wayland_set_compositor_display (compositor->wayland_display);

  if (clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS)
    g_error ("Failed to initialize Clutter");

  compositor->stage = clutter_stage_new ();
  clutter_stage_set_user_resizable (CLUTTER_STAGE (compositor->stage), FALSE);
  g_signal_connect_after (compositor->stage, "paint",
                          G_CALLBACK (paint_finished_cb), compositor);
  g_signal_connect (compositor->stage, "destroy",
                    G_CALLBACK (stage_destroy_cb), NULL);

  meta_wayland_compositor_create_output (compositor, 0, 0, 1024, 600, 222, 125);

  if (wl_global_create (compositor->wayland_display,
			&wl_shell_interface, 1,
			compositor, bind_shell) == NULL)
    g_error ("Failed to register a global shell object");

  clutter_actor_show (compositor->stage);

  if (wl_display_add_socket (compositor->wayland_display, "wayland-0"))
    g_error ("Failed to create socket");

  wl_global_create (compositor->wayland_display,
		    &xserver_interface, 1,
		    compositor, bind_xserver);

  /* We need a mapping from xids to wayland surfaces... */
  compositor->window_surfaces = g_hash_table_new (g_int_hash, g_int_equal);

  /* XXX: It's important that we only try and start xwayland after we
   * have initialized EGL because EGL implements the "wl_drm"
   * interface which xwayland requires to determine what drm device
   * name it should use.
   *
   * By waiting until we've shown the stage above we ensure that the
   * underlying GL resources for the surface have also been allocated
   * and so EGL must be initialized by this point.
   */

  if (!meta_xwayland_start (compositor))
    g_error ("Failed to start X Wayland");

  putenv (g_strdup_printf ("DISPLAY=:%d", compositor->xwayland_display_index));

  /* We need to run a mainloop until we know xwayland has a binding
   * for our xserver interface at which point we can assume it's
   * ready to start accepting connections. */
  compositor->init_loop = g_main_loop_new (NULL, FALSE);

  g_main_loop_run (compositor->init_loop);
}

void
meta_wayland_finalize (void)
{
  meta_xwayland_stop (meta_wayland_compositor_get_default ());
}

void
meta_wayland_handle_sig_child (void)
{
  int status;
  pid_t pid = waitpid (-1, &status, WNOHANG);
  MetaWaylandCompositor *compositor = &_meta_wayland_compositor;

  /* The simplest measure to avoid infinitely re-spawning a crashing
   * X server */
  if (pid == compositor->xwayland_pid)
    {
      if (!WIFEXITED (status))
        g_critical ("X Wayland crashed; aborting");
      else
        {
          /* For now we simply abort if we see the server exit.
           *
           * In the future X will only be loaded lazily for legacy X support
           * but for now it's a hard requirement. */
          g_critical ("Spurious exit of X Wayland server");
        }
    }
}
